#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    setSize (800, 600);

    setupGuiComponents();
    setupAudioPlayer();
}

MainComponent::~MainComponent()
{
    // Stop UI timer first to avoid repaint after teardown
    stopTimer();

    // This shuts down the audio device and clears the audio source.
    shutdownAudio();

    // Ensure transport is stopped and reader released before destruction
    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();
}

void MainComponent::setupGuiComponents()
{
    addAndMakeVisible (loadButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);

    smoothingLabel.setText ("Smoothing", juce::dontSendNotification);
    smoothingLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (smoothingLabel);

    smoothingSlider.setRange (0.0, 1.0, 0.001);

    // Invert mapping: slider shows "smoothing amount", alpha is "snappiness".
    smoothingSlider.setValue (rmsSmoothingAlpha, juce::dontSendNotification);
    smoothingSlider.setTextValueSuffix ("");
    smoothingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    smoothingSlider.onValueChange = [this]
    {
        const double sliderVal = smoothingSlider.getValue();
        rmsSmoothingAlpha = (float) (sliderVal);
    };
    addAndMakeVisible (smoothingSlider);

    loadButton.onClick = [this] { chooseAndLoadFile(); };
    playButton.onClick = [this]
    {
        const double len = transport.getLengthInSeconds();
        if (len > 0.0 && transport.getCurrentPosition() >= len - 1e-6)
            transport.setPosition (0.0);

        transport.start();
        setButtonsEnabledState();
    };
    stopButton.onClick = [this]
    {
        transport.stop();
        setButtonsEnabledState();
    };

    setButtonsEnabledState();
}

void MainComponent::setupAudioPlayer()
{
    formatManager.registerBasicFormats();
    setAudioChannels (0, 2);
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused (samplesPerBlockExpected);
    transport.prepareToPlay (samplesPerBlockExpected, sampleRate);

    // Prepare RMS arrays to current output channels (from device)
    int numOutChans = 1;
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        numOutChans = juce::jmax (1, dev->getActiveOutputChannels().countNumberOfSetBits());

    {
        const juce::SpinLock::ScopedLockType sl (rmsLock);
        lastRms.clearQuick();
        smoothedRms.clearQuick();
        lastRms.insertMultiple (0, 0.0f, numOutChans);
        smoothedRms.insertMultiple (0, 0.0f, numOutChans);
    }
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    // 1) All zero if no source ------------------
    if (readerSource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        // Also reset RMS to zero when no source
        const juce::SpinLock::ScopedLockType sl (rmsLock);
        for (int c = 0; c < smoothedRms.size(); ++c)
            smoothedRms.set (c, 0.0f);
        lastRms = smoothedRms;
        return;
    }

    transport.getNextAudioBlock (bufferToFill);

    // 2) Get audio buffer data -------------------
    auto* buffer = bufferToFill.buffer;
    if (buffer == nullptr || bufferToFill.numSamples <= 0)
        return;

    const int numChans = buffer->getNumChannels();
    const int n = bufferToFill.numSamples;
    const int start = bufferToFill.startSample;

    // 3) Calculate instant RMS
    juce::Array<float> instantRms;
    instantRms.insertMultiple (0, 0.0f, numChans);

    for (int ch = 0; ch < numChans; ++ch)
    {
        const float* data = buffer->getReadPointer (ch, start);

        double sumSquares = 0.0;
        for (int i = 0; i < n; ++i) // n = bufferSize
        {
            const float s = data[i];
            sumSquares += (double) s * (double) s;
        }
        auto sumSquaresDiv = sumSquares / (double) n;
        const float rms = std::sqrt ((float) sumSquaresDiv);
        instantRms.set (ch, rms);
    }

    // Exponential smoothing and publishs
    {
        const juce::SpinLock::ScopedLockType sl (rmsLock); // Lock
        const float a = juce::jlimit (0.0f, 1.0f, rmsSmoothingAlpha);
        const float b = 1.0f - a;

        for (int ch = 0; ch < smoothedRms.size(); ++ch)
        {
            const float sm = b * instantRms[ch] + a * smoothedRms[ch];
            smoothedRms.set (ch, sm);
            lastRms.set (ch, sm);
        }
    }
}

void MainComponent::releaseResources()
{
    transport.releaseResources();
}

static juce::Colour rmsToColour (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);

    juce::Colour green  = juce::Colour::fromRGB (0, 200, 0);
    juce::Colour yellow = juce::Colour::fromRGB (255, 220, 0);
    juce::Colour orange = juce::Colour::fromRGB (255, 140, 0);
    juce::Colour red    = juce::Colour::fromRGB (255, 0, 0);

    if (v < 0.33f)       // green → yellow
    {
        float t = v / 0.33f;
        return green.interpolatedWith (yellow, t);
    }
    else if (v < 0.66f)  // yellow → orange
    {
        float t = (v - 0.33f) / 0.33f;
        return yellow.interpolatedWith (orange, t);
    }
    else                 // orange → red
    {
        float t = (v - 0.66f) / 0.34f;
        return orange.interpolatedWith (red, t);
    }
}



//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto bounds = getLocalBounds().reduced (20);
    auto rmsValues = lastRms;   // use smoothed RMS

    const int numCircles = juce::jmax (1, rmsValues.size());
    auto area = bounds;
    const int gap = 20;
    const int circleSlotWidth = (area.getWidth() - (gap * (numCircles - 1))) / numCircles;

    g.setFont (juce::Font (juce::FontOptions (25.0f)));

    for (int i = 0; i < numCircles; ++i)
    {
        float value = juce::jlimit (0.0f, 1.0f, rmsValues[i]);

        // Color determined by RMS
        juce::Colour circleColour = rmsToColour (value);


        // Circle radius (scaled by RMS)
        float maxRadius = area.getHeight() * 0.5f;
        float radius = value * maxRadius;

        float cx = i * (circleSlotWidth + gap) + area.getX() + circleSlotWidth * 0.5f;
        float cy = area.getCentreY();

        juce::Rectangle<float> circle (cx - radius, cy - radius,
                                       radius * 2.0f, radius * 2.0f);

        // Draw ellipse with RMS-based colour
        g.setColour (circleColour);
        g.fillEllipse (circle);

        // Numeric label
        juce::String text = juce::String (value, 2);
        g.setColour (juce::Colours::white);

        juce::Rectangle<int> label (int(cx - circleSlotWidth * 0.5f),
                                    area.getBottom(),
                                    circleSlotWidth,
                                    18);

        g.drawFittedText (text, label, juce::Justification::centred, 1);
    }
}



void MainComponent::resized()
{
    // Simple horizontal layout for the buttons and slider row
    auto area = getLocalBounds().reduced (20);
    auto buttonHeight = 32;
    auto row = area.removeFromTop (buttonHeight);

    loadButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (10);
    playButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (10);
    stopButton.setBounds (row.removeFromLeft (120));

    // Next row for smoothing control
    auto controlRow = area.removeFromTop (28);
    smoothingLabel.setBounds (controlRow.removeFromLeft (100));
    controlRow.removeFromLeft (8);
    smoothingSlider.setBounds (controlRow.removeFromLeft (juce::jmax (200, controlRow.getWidth() / 2)));
}

//==============================================================================
void MainComponent::buttonClicked (juce::Button* button)
{
    // Not used because we used onClick lambdas, but kept for completeness
    if (button == &loadButton) chooseAndLoadFile();
    if (button == &playButton) { transport.start(); setButtonsEnabledState(); }
    if (button == &stopButton) { transport.stop();  setButtonsEnabledState(); }
}

void MainComponent::setButtonsEnabledState()
{
    const bool hasFile = (readerSource != nullptr);
    const bool isPlaying = transport.isPlaying();

    playButton.setEnabled (hasFile && !isPlaying);
    stopButton.setEnabled (hasFile && isPlaying);

    // Drive UI updates while playing; stop when not.
    if (isPlaying)
    {
        if (! isTimerRunning())
            startTimerHz (30); // Test different values
    }
    else
    {
        stopTimer();
        // One last repaint to show zeroed meters if stopped
        repaint();
    }
}

juce::Array<float> MainComponent::getLatestRms() const
{
    const juce::SpinLock::ScopedLockType sl (rmsLock);
    return lastRms; // returns a copy
}

void MainComponent::timerCallback()
{
    // Poll transport state transition: if playback stopped externally, update buttons/timer
    /// Test commenting this code and check what happens when file ends playing
    if (! transport.isPlaying())
    {
        // If we reached the end of the file, rewind so Play works again
        const double len = transport.getLengthInSeconds();
        if (len > 0.0 && transport.getCurrentPosition() >= len - 1e-6)
            transport.setPosition (0.0);

        setButtonsEnabledState();// setButtonsEnabledState will stopTimer and repaint
        return;
    }

    // Trigger repaint to update meters and DBG output
    repaint(); // Test commenting
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

    auto inputStream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress));

    if (inputStream == nullptr)
        return;

    // AudioFormatReader: Reads samples from an audio file stream.
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
