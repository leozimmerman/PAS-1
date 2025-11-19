#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize (800, 600);

    // Register audio formats we can read
    formatManager.registerBasicFormats();

    // Transport-related UI
    addAndMakeVisible (loadButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);

    loadButton.onClick = [this] { chooseAndLoadFile(); };
    playButton.onClick = [this] { transport.start(); setButtonsEnabledState(); };
    stopButton.onClick = [this] { transport.stop();  setButtonsEnabledState(); };

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
        // coefficients updated in audio thread before processing
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

    // Listen for transport state changes (start/stop/end-of-stream)
    juce::MessageManagerLock mmLock; // ensure we're on the message thread for listener ops
    transport.addChangeListener (this);

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? 2 : 0, 2); });
    }
    else
    {
        // We only need outputs to play files; inputs can be 0
        setAudioChannels (0, 2);
    }
}

MainComponent::~MainComponent()
{
    {
        juce::MessageManagerLock mmLock; // remove listener safely
        transport.removeChangeListener (this);
    }

    // This shuts down the audio device and clears the audio source.
    shutdownAudio();

    // Ensure transport is stopped and reader released before destruction
    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused (samplesPerBlockExpected);
    currentSampleRate = (sampleRate > 0.0 ? sampleRate : 44100.0);
    updateCoefficients();

    // reset state (size will be ensured on first getNextAudioBlock)
    z1.clearQuick();

    transport.prepareToPlay (samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Fill from transport, or clear if no source
    if (readerSource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    transport.getNextAudioBlock (bufferToFill);

    // Apply simple first-order filter in-place
    auto* buffer = bufferToFill.buffer;
    if (buffer == nullptr) return;

    const int numChannels = buffer->getNumChannels();
    const int numSamples  = bufferToFill.numSamples;
    const int startSample = bufferToFill.startSample;

    ensureStateSize (numChannels);
    updateCoefficients(); // cheap, in case cutoff/type changed

    const auto type = filterType.load (std::memory_order_relaxed);

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
    transport.releaseResources();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    // Layout:
    // Top row: transport buttons
    // Next rows: filter controls with labels attached on the left
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
void MainComponent::buttonClicked (juce::Button* button)
{
    // Not used because we used onClick lambdas, but kept for completeness
    if (button == &loadButton) chooseAndLoadFile();
    if (button == &playButton) { transport.start(); setButtonsEnabledState(); }
    if (button == &stopButton) { transport.stop();  setButtonsEnabledState(); }
}

void MainComponent::chooseAndLoadFile()
{
    // Use async FileChooser so GUI stays responsive
    auto chooser = std::make_shared<juce::FileChooser> ("Select an audio file to play...",
                                                         juce::File(),
                                                         "*.wav;*.aiff;*.mp3;*.flac;*.ogg;*.m4a");
    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
    {
        auto url = fc.getURLResult(); // Works for local files and sandboxed URLs (iOS/macOS)
        if (url.isEmpty())
            return;

        loadURL (url);
    });
}

void MainComponent::loadURL (const juce::URL& url)
{
    // Stop current playback and detach current source
    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();

    // Open via AudioFormatReader
    std::unique_ptr<juce::InputStream> inputStream (url.createInputStream (false));
    if (inputStream == nullptr)
        return;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (std::move (inputStream)));
    if (reader == nullptr)
        return;

    // Capture the file's sample rate from the reader before transferring ownership
    const double fileSampleRate = reader->sampleRate;

    // Create the reader source (takes ownership of reader)
    readerSource.reset (new juce::AudioFormatReaderSource (reader.release(), true));

    // Set the source; pass the file's sample rate so Transport can resample if needed
    transport.setSource (readerSource.get(), 0, nullptr, fileSampleRate);

    // Reset position to start
    transport.setPosition (0.0);

    setButtonsEnabledState();
}

void MainComponent::setButtonsEnabledState()
{
    const bool hasFile = (readerSource != nullptr);
    const bool isPlaying = transport.isPlaying();

    playButton.setEnabled (hasFile && !isPlaying);
    stopButton.setEnabled (hasFile && isPlaying);
}

// Listen for end-of-stream and other state changes
void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &transport)
    {
        // If playback has stopped and the stream finished, rewind to start
        if (! transport.isPlaying() && transport.hasStreamFinished())
            transport.setPosition (0.0);

        // Refresh UI state on any change
        setButtonsEnabledState();
    }
}

//==============================================================================
// Simple first-order filter implementation

void MainComponent::ensureStateSize (int numChannels)
{
    if (z1.size() < numChannels)
    {
        const int oldSize = z1.size();
        z1.resize (numChannels);
        for (int i = oldSize; i < numChannels; ++i)
            z1.setUnchecked (i, 0.0f);
    }
}

void MainComponent::updateCoefficients()
{
    // One-pole filter using bilinear transform
    // Low-pass: y[n] = a0 * x[n] + b1 * y[n-1], with
    // alpha = exp(-2*pi*fc/fs); a0 = 1 - alpha; b1 = alpha
    // High-pass (complement): y[n] = x[n] - LP(x[n])
    auto fs = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
    auto fc = juce::jlimit (10.0f, (float) (0.45 * fs), cutoffHz.load (std::memory_order_relaxed));
    const double alpha = std::exp (-2.0 * juce::MathConstants<double>::pi * (double) fc / fs);

    a0 = (float) (1.0 - alpha);
    b1 = (float) alpha;
}

inline float MainComponent::processSampleLP (float x, int ch)
{
    float y = a0 * x + b1 * z1.getUnchecked (ch);
    z1.setUnchecked (ch, y);
    return y;
}

inline float MainComponent::processSampleHP (float x, int ch)
{
    // High-pass as input minus low-pass output (same pole)
    float lp = processSampleLP (x, ch);
    return x - lp;
}
