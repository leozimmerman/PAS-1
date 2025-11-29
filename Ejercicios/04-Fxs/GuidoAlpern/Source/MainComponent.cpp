#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize(900, 600);

    // Register audio formats we can read
    formatManager.registerBasicFormats();

    // Transport UI
    addAndMakeVisible(loadButton);
    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);

    loadButton.onClick = [this]
        {
            chooseAndLoadFile();
        };
    playButton.onClick = [this]
        {
            transport.start();
            setButtonsEnabledState();
        };
    stopButton.onClick = [this]
        {
            transport.stop();
            setButtonsEnabledState();
        };

    // Ranges and defaults
    delayTimeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    delayTimeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    delayTimeSlider.setRange(1.0, 2000.0, 1.0); // ms
    delayTimeSlider.setValue(delayTimeMs);

    feedbackSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    feedbackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    feedbackSlider.setRange(0.0, 0.95, 0.001);
    feedbackSlider.setValue(feedback);

    // Skip toggle button
    skipToggle.setButtonText("Skip Mode");
    skipToggle.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(skipToggle);

    // Labels
    delayTimeLabel.attachToComponent(&delayTimeSlider, false);
    feedbackLabel.attachToComponent(&feedbackSlider, false);

    delayTimeLabel.setJustificationType(juce::Justification::centred);
    feedbackLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(delayTimeSlider);
    addAndMakeVisible(feedbackSlider);
    addAndMakeVisible(delayTimeLabel);
    addAndMakeVisible(feedbackLabel);

    // Slider callbacks
    delayTimeSlider.onValueChange = [this]
        {
            delayTimeMs = (float)delayTimeSlider.getValue();
            // Map ms to samples and clamp to current max
            const int newDelaySamples = (int)std::round((delayTimeMs * 0.001) * currentSampleRate);
            DBG("Delay time changed: " << delayTimeMs << " ms -> " << newDelaySamples << " samples");

            if (maxDelaySamples > 0) // maxDelaySeconds is set to 2 seconds.
                delaySamples = juce::jlimit(1, juce::jmax(1, maxDelaySamples - 1), newDelaySamples);
            else
                delaySamples = juce::jmax(1, newDelaySamples);
        };

    feedbackSlider.onValueChange = [this]
        {
            feedback = (float)feedbackSlider.getValue();
        };

    setButtonsEnabledState();

    juce::MessageManagerLock mmLock;
    transport.addChangeListener(this);

    setAudioChannels(0, 1);
}

MainComponent::~MainComponent()
{
    {
        juce::MessageManagerLock mmLock; // remove listener safely
        transport.removeChangeListener(this);
    }

    // This shuts down the audio device and clears the audio source.
    shutdownAudio();

    // Ensure transport is stopped and reader released before destruction
    transport.stop();
    transport.setSource(nullptr);
    readerSource.reset();
}

//==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused(samplesPerBlockExpected);
    currentSampleRate = sampleRate;
    transport.prepareToPlay(samplesPerBlockExpected, sampleRate);

    // Prepare delay state
    prepareDelayState();

    // Ensure delaySamples matches current delayTimeMs
    delayTimeSlider.onValueChange();
}

void MainComponent::prepareDelayState()
{
    // Choose a safe maximum delay time (in seconds)
    const float maxDelaySeconds = 2.0f; // 2 seconds max delay
    maxDelaySamples = (int)std::ceil(maxDelaySeconds * currentSampleRate);

    delayBuffer.assign((size_t)maxDelaySamples, 0.0f);
    writePos = 0;

    // Clamp delaySamples to valid range
    delaySamples = juce::jlimit(1, juce::jmax(1, maxDelaySamples - 1), delaySamples);
}

void MainComponent::processDelayChannel(juce::AudioBuffer<float>& buffer, int channelNum)
{
    if (buffer.getNumSamples() <= 0 || delayBuffer.empty())
        return;

    if (channelNum < 0 || channelNum >= buffer.getNumChannels())
        return;

    auto* data = buffer.getWritePointer(channelNum);
    const int numSamples = buffer.getNumSamples();
    int skipCounter = 0;
    const int skipRate = 8;
    const bool skipEnabled = skipToggle.getToggleState();

    // delayBuffSize es constante, definido en funcion del maximo de delay permitido (2s.)
    const int delayBuffSize = (int)delayBuffer.size();

    for (int i = 0; i < numSamples; ++i)
    {
        int readPos = writePos - delaySamples;
        if (readPos < 0)
            readPos += delayBuffSize;

        const float delayed = delayBuffer[(size_t)readPos];
        const float in = data[i]; // read data from buffer

        delayBuffer[(size_t)writePos] = in + feedback * delayed;

        // Output write to streaming AudioBuffer: dry + wet (fixed 50/50)
        if (skipEnabled)
        {
            // Compute factor as float to avoid integer division
            const float factor = (float)(skipCounter % skipRate) / (float)(skipRate - 1);

            // Print the factor for the first sample in this block to avoid flooding the debug output
            if (i == 0)
                DBG("skip factor: " << factor << " (skipCounter=" << skipCounter << ", skipRate=" << skipRate << ")");

            data[i] = in + factor * delayed;  // Aplicar delay

            skipCounter++;
        }
        else
        {
            data[i] = in + delayed;
        }

        writePos = (writePos + 1) % delayBuffSize;
    }
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Fill from transport, or clear if no source
    if (readerSource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    transport.getNextAudioBlock(bufferToFill);

    // Apply simple delay on a specific channel (current behavior: channel 0)
    if (bufferToFill.buffer != nullptr && bufferToFill.numSamples > 0 && bufferToFill.buffer->getNumChannels() > 0)
    {
        processDelayChannel(*bufferToFill.buffer, 0);
    }

}

void MainComponent::releaseResources()
{
    transport.releaseResources();

    // Clear delay buffers
    delayBuffer.clear();
    writePos = 0;
    maxDelaySamples = 0;
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    // Top row: buttons
    auto area = getLocalBounds().reduced(20);
    auto buttonHeight = 32;
    auto row = area.removeFromTop(buttonHeight);

    loadButton.setBounds(row.removeFromLeft(120));
    row.removeFromLeft(10);
    playButton.setBounds(row.removeFromLeft(120));
    row.removeFromLeft(10);
    stopButton.setBounds(row.removeFromLeft(120));
    row.removeFromLeft(10);
    skipToggle.setBounds(row.removeFromLeft(120));

    area.removeFromTop(20);

    // Below: two rotary sliders in a row (time, feedback)
    auto controlsArea = area.removeFromTop(200);
    auto numKnobs = 2;
    auto knobWidth = controlsArea.getWidth() / numKnobs;

    auto placeKnob = [](juce::Component& c, juce::Rectangle<int> r)
        {
            c.setBounds(r.reduced(10));
        };

    juce::Rectangle<int> col;

    col = controlsArea.removeFromLeft(knobWidth);
    placeKnob(delayTimeSlider, col);

    col = controlsArea.removeFromLeft(knobWidth);
    placeKnob(feedbackSlider, col);
}

void MainComponent::chooseAndLoadFile()
{
    auto chooser = std::make_shared<juce::FileChooser>("Select an audio file to play...",
        juce::File(),
        "*.wav;*.aiff;*.mp3;*.flac;*.ogg;*.m4a");
    auto flags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync(flags, [this, chooser](const juce::FileChooser& fc)
        {
            auto url = fc.getURLResult();
            if (url.isEmpty())
                return;

            loadURL(url);
        });
}

void MainComponent::loadURL(const juce::URL& url)
{
    transport.stop();
    transport.setSource(nullptr);
    readerSource.reset();
    auto inputStream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress));

    if (inputStream == nullptr)
        return;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(std::move(inputStream)));
    if (reader == nullptr)
        return;
    const double fileSampleRate = reader->sampleRate;
    readerSource.reset(new juce::AudioFormatReaderSource(reader.release(), true));
    transport.setSource(readerSource.get(), 0, nullptr, fileSampleRate);
    transport.setPosition(0.0);

    setButtonsEnabledState();
}

void MainComponent::setButtonsEnabledState()
{
    const bool hasFile = (readerSource != nullptr);
    const bool isPlaying = transport.isPlaying();

    playButton.setEnabled(hasFile && !isPlaying);
    stopButton.setEnabled(hasFile && isPlaying);
}

// Listen for end-of-stream and other state changes
void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &transport)
    {
        if (!transport.isPlaying() && transport.hasStreamFinished())
            transport.setPosition(0.0);
        setButtonsEnabledState();
    }
}