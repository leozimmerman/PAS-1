#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    setSize (800, 600);

    // Transport-related UI
    addAndMakeVisible (loadButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);

    loadButton.onClick = [this]
    {
        chooseAndLoadFile();
    };
    playButton.onClick = [this]
    {
        audioManager.start();
        setButtonsEnabledState();
    };
    stopButton.onClick = [this] {
        audioManager.stop();
        setButtonsEnabledState();
    };

    // Filter UI setup
    // Cutoff slider
    cutoffSlider.setSliderStyle (juce::Slider::SliderStyle::LinearHorizontal);
    cutoffSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    cutoffSlider.setRange (20.0, 20000.0, 0.01);
    cutoffSlider.setSkewFactorFromMidPoint (1000.0); // perceptually-log feel
    cutoffSlider.setValue ((double) cutoffHz.load (std::memory_order_relaxed));
    cutoffSlider.onValueChange = [this]
    {
        cutoffHz.store ((float) cutoffSlider.getValue(), std::memory_order_relaxed);
    };
    addAndMakeVisible (cutoffSlider);

    cutoffLabel.setJustificationType (juce::Justification::centredLeft);
    cutoffLabel.attachToComponent (&cutoffSlider, true); // label on left
    addAndMakeVisible (cutoffLabel);

    // Filter type combo box
    filterTypeBox.addItem ("Low-Pass", 1);
    filterTypeBox.addItem ("High-Pass", 2);
    filterTypeBox.onChange = [this]
    {
        const int sel = filterTypeBox.getSelectedId();
        filterType.store (sel == 2 ? FilterType::HighPass : FilterType::LowPass,
                          std::memory_order_relaxed);
        // coefficients reused; HP uses LP internally
    };
    // initialise selection from current filterType
    filterTypeBox.setSelectedId (filterType.load() == FilterType::HighPass ? 2 : 1, juce::dontSendNotification);
    addAndMakeVisible (filterTypeBox);

    filterTypeLabel.setJustificationType (juce::Justification::centredLeft);
    filterTypeLabel.attachToComponent (&filterTypeBox, true);
    addAndMakeVisible (filterTypeLabel);

    setButtonsEnabledState();

    // Listen for transport state changes via manager
    audioManager.addChangeListener (this);
    setAudioChannels (0, 2);
}

MainComponent::~MainComponent()
{
    audioManager.removeChangeListener (this);

    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused (samplesPerBlockExpected);
    currentSampleRate = (sampleRate > 0.0 ? sampleRate : 44100.0);
    updateCoefficients();

    // reset state (size will be ensured on first getNextAudioBlock)
    prevValues.clearQuick();

    audioManager.prepareToPlay (samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Fill from transport, or clear if no source
    audioManager.getNextAudioBlock (bufferToFill);

    
    auto* buffer = bufferToFill.buffer;
    if (buffer == nullptr) return;

    const int numChannels = buffer->getNumChannels();
    const int numSamples  = bufferToFill.numSamples;
    const int startSample = bufferToFill.startSample;

    ensureStateSize (numChannels);
    
    // Apply simple first-order filter in-place
    
    updateCoefficients(); // cheap, in case cutoff/type changed

    const auto type = filterType.load (std::memory_order_relaxed); // atomic, thread-safe

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = buffer->getWritePointer (ch, startSample);

        if (type == FilterType::LowPass)
        {
            for (int n = 0; n < numSamples; ++n)
                data[n] = processSampleLP (data[n], ch);
        }
        else // HighPass
        {
            for (int n = 0; n < numSamples; ++n)
                data[n] = processSampleHP (data[n], ch);
        }
    }
}

void MainComponent::releaseResources()
{
    audioManager.releaseResources();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{

    auto area = getLocalBounds().reduced (20);

    // Transport row
    auto row = area.removeFromTop (32);
    loadButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (10);
    playButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (10);
    stopButton.setBounds (row.removeFromLeft (120));

    area.removeFromTop (20);

    // Filter type row (label attached to component)
    {
        auto typeRow = area.removeFromTop (28);
        // Reserve space for attached label (approx 110 px)
        auto labelWidth = 110;
        typeRow.removeFromLeft (labelWidth);
        filterTypeBox.setBounds (typeRow.removeFromLeft (180));
    }

    area.removeFromTop (10);

    // Cutoff row (label attached to component)
    {
        auto cutoffRow = area.removeFromTop (40);
        auto labelWidth = 110;
        cutoffRow.removeFromLeft (labelWidth);
        cutoffSlider.setBounds (cutoffRow);
    }
}

//==============================================================================
void MainComponent::chooseAndLoadFile()
{
    audioManager.chooseAndLoadFile();
    playButton.setEnabled(true);
}

void MainComponent::setButtonsEnabledState()
{
    const bool hasFile = audioManager.hasFileLoaded();
    const bool isPlaying = audioManager.isPlaying();

    playButton.setEnabled (hasFile && !isPlaying);
    stopButton.setEnabled (hasFile && isPlaying);
}

// Listen for end-of-stream and other state changes
void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == audioManager.getTransport())
    {
        // If playback has stopped and the stream finished, rewind to start
        if (! audioManager.isPlaying() && audioManager.hasStreamFinished())
            audioManager.setPosition (0.0);

        // Refresh UI state on any change
        setButtonsEnabledState();
    }
}

//==============================================================================
// Simple first-order filter implementation

void MainComponent::ensureStateSize (int numChannels)
{
    if (prevValues.size() < numChannels)
    {
        const int oldSize = prevValues.size();
        prevValues.resize (numChannels);
        for (int i = oldSize; i < numChannels; ++i)
            prevValues.setUnchecked (i, 0.0f);
    }
}

void MainComponent::updateCoefficients()
{
    // Low-pass: y[n] = a0 * x[n] + b1 * y[n-1],
    // High-pass (complement): y[n] = x[n] - LP(x[n])
    auto sampleRate = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
    // limita cutOffFreq entre 10hz y 45% del Sample Rate, cerca de Nyquist
    auto cutOff = juce::jlimit (10.0f, (float) (0.45 * sampleRate), cutoffHz.load (std::memory_order_relaxed));
    const double alpha = std::exp (-2.0 * juce::MathConstants<double>::pi * (double) cutOff / sampleRate);

    a0 = (float) (1.0 - alpha);
    b1 = (float) alpha;
}

inline float MainComponent::processSampleLP (float x, int ch)
{
    float y = a0 * x + b1 * prevValues.getUnchecked (ch); // prevValues[ch] + Lock
    prevValues.setUnchecked (ch, y); // prevValues[ch] = y; + Lock
    return y;
}

inline float MainComponent::processSampleHP (float x, int ch)
{
    // High-pass as input minus low-pass output (same pole)
    float lp = processSampleLP (x, ch);
    return x - lp;
}
