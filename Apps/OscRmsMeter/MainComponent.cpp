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

    disconnectOsc();
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

    // Minimal OSC GUI
    hostLabel.setJustificationType (juce::Justification::centredLeft);
    portLabel.setJustificationType (juce::Justification::centredLeft);
    addrLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (hostLabel);
    addAndMakeVisible (portLabel);
    addAndMakeVisible (addrLabel);

    hostEdit.setText (oscHost, juce::dontSendNotification);
    portEdit.setInputRestrictions (0, "0123456789");
    portEdit.setText (juce::String (oscPort), juce::dontSendNotification);
    addrEdit.setText (oscAddress, juce::dontSendNotification);
    addAndMakeVisible (hostEdit);
    addAndMakeVisible (portEdit);
    addAndMakeVisible (addrEdit);

    oscEnableToggle.onClick = [this] { handleOscEnableToggleClicked(); };
    addAndMakeVisible (oscEnableToggle);

    // Update connection if user edits fields while enabled
    hostEdit.onFocusLost = [this] { reconnectOscIfEnabled(); };
    portEdit.onFocusLost = [this] { reconnectOscIfEnabled(); };
    addrEdit.onFocusLost = [this] { reconnectOscIfEnabled(); };

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

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // Simple debug: draw RMS bars
    auto bounds = getLocalBounds().reduced (20);
    auto rmsValues = getLatestRms();

    const int numBars = juce::jmax (1, rmsValues.size()); // chanels(2)
    auto barsArea = bounds;//bounds.removeFromBottom (100);
    const int gap = 20; // Test different values
    const int barWidth = (barsArea.getWidth() - (gap * (numBars - 1))) / numBars;

    // Draw dynamic RMS bars and numeric labels
    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    for (int i = 0; i < numBars; ++i)
    {
        const float value = juce::jlimit (0.0f, 1.0f, rmsValues[i]);
        auto h = juce::roundToInt ((float) barsArea.getHeight() * value);
        
        // 1) Bar meter: X depends on i, H depends on value.
        auto bar = juce::Rectangle<int> (i * (barWidth + gap) + barsArea.getX(),
                                         barsArea.getBottom() - h,
                                         barWidth,
                                         h);

        // Bar
        g.setColour (juce::Colours::limegreen);
        g.fillRect (bar);

        // 2) Numeric label above the bar (linear value 0.00..1.00)
        juce::String labelText = juce::String (value, 2); // two decimals

        // Place the label slightly above the top of the bar, centered
        const int labelHeight = 18;
        // Label bounds: coordinates depends on bar coords.
        auto labelBounds = juce::Rectangle<int> (bar.getX(),
                                                 bounds.getBottom(),
                                                 bar.getWidth(),
                                                 labelHeight);

        // Draw label with contrasting colour and centered
        g.setColour (juce::Colours::white);
        g.drawFittedText (labelText, labelBounds, juce::Justification::centred, 1);
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

    // OSC minimal row(s)
    auto oscRow1 = area.removeFromTop (26);
    hostLabel.setBounds (oscRow1.removeFromLeft (50));
    oscRow1.removeFromLeft (6);
    hostEdit.setBounds (oscRow1.removeFromLeft (160));
    oscRow1.removeFromLeft (12);
    portLabel.setBounds (oscRow1.removeFromLeft (40));
    oscRow1.removeFromLeft (6);
    portEdit.setBounds (oscRow1.removeFromLeft (80));
    oscRow1.removeFromLeft (12);
    addrLabel.setBounds (oscRow1.removeFromLeft (70));
    oscRow1.removeFromLeft (6);
    addrEdit.setBounds (oscRow1.removeFromLeft (180));
    oscRow1.removeFromLeft (12);
    oscEnableToggle.setBounds (oscRow1.removeFromLeft (120));
}

//==============================================================================
void MainComponent::buttonClicked (juce::Button* button)
{
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

    // Trigger repaint to update meters
    repaint();

    /// Send OSC (from message thread) if enabled
    if (oscEnableToggle.getToggleState() && oscConnected)
    {
        auto values = getLatestRms();
        sendRmsOverOsc (values);
    }
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

void MainComponent::updateOscConnection()
{
    // If connected first disconnect
    if (oscConnected)
    {
        disconnectOsc();
    }

    // Validate needed config values
    if (oscHost.isEmpty() || oscPort <= 0)
    {
        oscConnected = false;
        return;
    }

    oscConnected = oscSender.connect (oscHost, oscPort);
}

void MainComponent::disconnectOsc()
{
    if (oscConnected)
    {
        oscSender.disconnect();
        oscConnected = false;
    }
}

void MainComponent::sendRmsOverOsc (const juce::Array<float>& values)
{
    if (! oscConnected)
        return;

    // Build message: /address <float ch0> <float ch1> ...
    juce::OSCMessage msg (oscAddress.isEmpty() ? "/rms" : oscAddress);

    for (auto v : values)
        msg.addFloat32 (juce::jlimit (0.0f, 1.0f, v));

    // Best-effort send; ignore failures here
    (void) oscSender.send (msg);
}

void MainComponent::reconnectOscIfEnabled()
{
    if (oscEnableToggle.getToggleState())
    {
        oscHost = hostEdit.getText().trim();
        oscPort = portEdit.getText().getIntValue();
        oscAddress = addrEdit.getText().trim();
        if (oscAddress.isEmpty())
            oscAddress = "/rms";

        updateOscConnection();
    }
}

void MainComponent::handleOscEnableToggleClicked()
{
    if (oscEnableToggle.getToggleState())
    {
        // Pull latest from fields settings and connect
        oscHost = hostEdit.getText().trim();
        oscPort = portEdit.getText().getIntValue();
        oscAddress = addrEdit.getText().trim();
        if (oscAddress.isEmpty()) // default address value
            oscAddress = "/rms";

        updateOscConnection();

        if (! oscConnected) // Fallback if connection failed
            oscEnableToggle.setToggleState (false, juce::dontSendNotification);
    }
    else
    {
        disconnectOsc();
    }
}
