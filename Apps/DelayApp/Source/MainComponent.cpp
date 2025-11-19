#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize (900, 600);

    // Register audio formats we can read
    formatManager.registerBasicFormats();

    // Transport UI
    addAndMakeVisible (loadButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);

    loadButton.onClick = [this] { chooseAndLoadFile(); };
    playButton.onClick = [this] { transport.start(); setButtonsEnabledState(); };
    stopButton.onClick = [this] { transport.stop();  setButtonsEnabledState(); };

    // Delay parameter controls setup
    auto setupRotary = [] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    };

    setupRotary (delayTimeSlider);
    setupRotary (feedbackSlider);
    setupRotary (wetSlider);
    setupRotary (drySlider);

    // Ranges and defaults
    delayTimeSlider.setRange (1.0, 2000.0, 1.0); // ms
    delayTimeSlider.setValue (delayTimeMs);
    feedbackSlider.setRange (0.0, 0.95, 0.001);
    feedbackSlider.setValue (feedback);
    wetSlider.setRange (0.0, 1.0, 0.001);
    wetSlider.setValue (wet);
    drySlider.setRange (0.0, 1.0, 0.001);
    drySlider.setValue (dry);

    // Labels
    delayTimeLabel.attachToComponent (&delayTimeSlider, false);
    feedbackLabel.attachToComponent (&feedbackSlider, false);
    wetLabel.attachToComponent (&wetSlider, false);
    dryLabel.attachToComponent (&drySlider, false);

    delayTimeLabel.setJustificationType (juce::Justification::centred);
    feedbackLabel.setJustificationType (juce::Justification::centred);
    wetLabel.setJustificationType (juce::Justification::centred);
    dryLabel.setJustificationType (juce::Justification::centred);

    // Add controls to UI
    addAndMakeVisible (delayTimeSlider);
    addAndMakeVisible (feedbackSlider);
    addAndMakeVisible (wetSlider);
    addAndMakeVisible (drySlider);
    addAndMakeVisible (delayTimeLabel);
    addAndMakeVisible (feedbackLabel);
    addAndMakeVisible (wetLabel);
    addAndMakeVisible (dryLabel);

    // Slider callbacks
    delayTimeSlider.onValueChange = [this]
    {
        delayTimeMs = (float) delayTimeSlider.getValue();
        // No need to reset buffers; we interpolate between taps each block
    };

    feedbackSlider.onValueChange = [this]
    {
        feedback = (float) feedbackSlider.getValue();
    };

    wetSlider.onValueChange = [this]
    {
        wet = (float) wetSlider.getValue();
        // Keep dry complementary if you want a constant-power style control; otherwise remove this line.
        // Here we keep both adjustable, but if user adjusts wet alone, keep dry = 1 - wet only if dry slider not being dragged.
        if (! drySlider.isMouseButtonDown())
        {
            dry = 1.0f - wet;
            drySlider.setValue (dry, juce::dontSendNotification);
        }
    };

    drySlider.onValueChange = [this]
    {
        dry = (float) drySlider.getValue();
        // Optionally keep wet complementary when user adjusts dry
        if (! wetSlider.isMouseButtonDown())
        {
            wet = 1.0f - dry;
            wetSlider.setValue (wet, juce::dontSendNotification);
        }
    };

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
    currentSampleRate = sampleRate;
    transport.prepareToPlay (samplesPerBlockExpected, sampleRate);

    // Prepare delay state
    resetDelayState();
}

void MainComponent::resetDelayState()
{
    // Choose a safe maximum delay time (in seconds)
    const float maxDelaySeconds = 2.0f; // 2 seconds max delay
    maxDelaySamples = (int) std::ceil (maxDelaySeconds * currentSampleRate);

    // Ensure internal vectors match current output channel count
    int numOutChans = 1;
    if (auto* device = deviceManager.getCurrentAudioDevice())
        numOutChans = juce::jmax (1, device->getActiveOutputChannels().countNumberOfSetBits());

    delayBufferPerChannel.resize ((size_t) numOutChans);
    writePositions.resize ((size_t) numOutChans);

    for (int ch = 0; ch < numOutChans; ++ch)
    {
        delayBufferPerChannel[(size_t) ch].assign ((size_t) maxDelaySamples, 0.0f);
        writePositions[(size_t) ch] = 0;
    }
}

void MainComponent::processDelay (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const int numChannels = juce::jmin ((int) delayBufferPerChannel.size(), buffer.getNumChannels());

    // Compute current delay in samples (clamp to max)
    float delaySamplesF = (delayTimeMs * 0.001f) * (float) currentSampleRate;
    delaySamplesF = juce::jlimit (1.0f, (float) maxDelaySamples - 1.0f, delaySamplesF);

    const int delayInt = (int) std::floor (delaySamplesF);
    const float frac = delaySamplesF - (float) delayInt;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* out = buffer.getWritePointer (ch, startSample);
        auto& delayBuf = delayBufferPerChannel[(size_t) ch];
        int& wpos = writePositions[(size_t) ch];

        const int size = (int) delayBuf.size();

        for (int i = 0; i < numSamples; ++i)
        {
            // Read index for integer and next (for linear interpolation)
            int rposA = wpos - delayInt;
            if (rposA < 0) rposA += size;
            int rposB = rposA + 1;
            if (rposB >= size) rposB -= size;

            const float delayedA = delayBuf[(size_t) rposA];
            const float delayedB = delayBuf[(size_t) rposB];
            const float delayed = delayedA + frac * (delayedB - delayedA);

            const float in = out[i];

            // Write input + feedback*delayed into the delay buffer
            const float toWrite = in + feedback * delayed;
            delayBuf[(size_t) wpos] = toWrite;

            // Mix wet/dry to output
            out[i] = dry * in + wet * delayed;

            // advance write position
            if (++wpos >= size) wpos = 0;
        }
    }

    // If output buffer has more channels than our delay buffers, just leave them as-is
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

    // Apply simple delay effect in-place
    if (bufferToFill.buffer != nullptr && bufferToFill.numSamples > 0)
        processDelay (*bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
}

void MainComponent::releaseResources()
{
    transport.releaseResources();

    // Clear delay buffers
    delayBufferPerChannel.clear();
    writePositions.clear();
    maxDelaySamples = 0;
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
    // Top row: buttons
    auto area = getLocalBounds().reduced (20);
    auto buttonHeight = 32;
    auto row = area.removeFromTop (buttonHeight);

    loadButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (10);
    playButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (10);
    stopButton.setBounds (row.removeFromLeft (120));

    area.removeFromTop (20);

    // Below: four rotary sliders in a row
    auto controlsArea = area.removeFromTop (200);
    auto numKnobs = 4;
    auto knobWidth = controlsArea.getWidth() / numKnobs;

    auto placeKnob = [] (juce::Component& c, juce::Rectangle<int> r)
    {
        c.setBounds (r.reduced (10));
    };

    juce::Rectangle<int> col;

    col = controlsArea.removeFromLeft (knobWidth);
    placeKnob (delayTimeSlider, col);

    col = controlsArea.removeFromLeft (knobWidth);
    placeKnob (feedbackSlider, col);

    col = controlsArea.removeFromLeft (knobWidth);
    placeKnob (wetSlider, col);

    col = controlsArea.removeFromLeft (knobWidth);
    placeKnob (drySlider, col);
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
