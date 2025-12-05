#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    setSize(800, 600);

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
    transport.setSource(nullptr);
    readerSource.reset();

    disconnectOsc();
}

void MainComponent::setupGuiComponents()
{
    addAndMakeVisible(loadButton);
    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);

    // Bass smoothing control
    bassSmoothingLabel.setText("Graves (Smooth)", juce::dontSendNotification);
    bassSmoothingLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bassSmoothingLabel);

    bassSmoothingSlider.setRange(0.0, 1.0, 0.001);
    bassSmoothingSlider.setValue(bassSmoothingAlpha, juce::dontSendNotification);
    bassSmoothingSlider.setTextValueSuffix("");
    bassSmoothingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    bassSmoothingSlider.onValueChange = [this]
        {
            bassSmoothingAlpha = (float)bassSmoothingSlider.getValue();
        };
    addAndMakeVisible(bassSmoothingSlider);

    // Mid smoothing control
    midSmoothingLabel.setText("Medios (Smooth)", juce::dontSendNotification);
    midSmoothingLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(midSmoothingLabel);

    midSmoothingSlider.setRange(0.0, 1.0, 0.001);
    midSmoothingSlider.setValue(midSmoothingAlpha, juce::dontSendNotification);
    midSmoothingSlider.setTextValueSuffix("");
    midSmoothingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    midSmoothingSlider.onValueChange = [this]
        {
            midSmoothingAlpha = (float)midSmoothingSlider.getValue();
        };
    addAndMakeVisible(midSmoothingSlider);

    // Treble smoothing control
    trebleSmoothingLabel.setText("Agudos (Smooth)", juce::dontSendNotification);
    trebleSmoothingLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(trebleSmoothingLabel);

    trebleSmoothingSlider.setRange(0.0, 1.0, 0.001);
    trebleSmoothingSlider.setValue(trebleSmoothingAlpha, juce::dontSendNotification);
    trebleSmoothingSlider.setTextValueSuffix("");
    trebleSmoothingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    trebleSmoothingSlider.onValueChange = [this]
        {
            trebleSmoothingAlpha = (float)trebleSmoothingSlider.getValue();
        };
    addAndMakeVisible(trebleSmoothingSlider);

    loadButton.onClick = [this] { chooseAndLoadFile(); };
    playButton.onClick = [this]
        {
            const double len = transport.getLengthInSeconds();
            if (len > 0.0 && transport.getCurrentPosition() >= len - 1e-6)
                transport.setPosition(0.0);

            transport.start();
            setButtonsEnabledState();
        };
    stopButton.onClick = [this]
        {
            transport.stop();
            setButtonsEnabledState();
        };

    // Minimal OSC GUI
    hostLabel.setJustificationType(juce::Justification::centredLeft);
    portLabel.setJustificationType(juce::Justification::centredLeft);
    addrLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(hostLabel);
    addAndMakeVisible(portLabel);
    addAndMakeVisible(addrLabel);

    hostEdit.setText(oscHost, juce::dontSendNotification);
    portEdit.setInputRestrictions(0, "0123456789");
    portEdit.setText(juce::String(oscPort), juce::dontSendNotification);
    addrEdit.setText(oscAddress, juce::dontSendNotification);
    addAndMakeVisible(hostEdit);
    addAndMakeVisible(portEdit);
    addAndMakeVisible(addrEdit);

    oscEnableToggle.onClick = [this] { handleOscEnableToggleClicked(); };
    addAndMakeVisible(oscEnableToggle);

    // Update connection if user edits fields while enabled
    hostEdit.onFocusLost = [this] { reconnectOscIfEnabled(); };
    portEdit.onFocusLost = [this] { reconnectOscIfEnabled(); };
    addrEdit.onFocusLost = [this] { reconnectOscIfEnabled(); };

    setButtonsEnabledState();
}

void MainComponent::setupAudioPlayer()
{
    formatManager.registerBasicFormats();
    setAudioChannels(0, 2);

    // Initialize frequency bands arrays
    lastFrequencyBands = { 0.0f, 0.0f, 0.0f };
    smoothedFrequencyBands = { 0.0f, 0.0f, 0.0f };
}

void MainComponent::setupFilters()
{
    updateFilterCoefficients();
}

// AJUSTAR PARAMETROS DE GRAVES / MEDIOS / AGUDOS
void MainComponent::updateFilterCoefficients()
{
    if (currentSampleRate <= 0.0)
        return;

    // Bass filter: low-pass around 250Hz
    bassFilterL.setCoefficients(juce::IIRCoefficients::makeLowPass(currentSampleRate, 250.0));
    bassFilterR.setCoefficients(juce::IIRCoefficients::makeLowPass(currentSampleRate, 250.0));

    // Mid filter: band-pass around 250Hz - 2kHz
    midFilterL.setCoefficients(juce::IIRCoefficients::makeBandPass(currentSampleRate, 1000.0, 1.0));
    midFilterR.setCoefficients(juce::IIRCoefficients::makeBandPass(currentSampleRate, 1000.0, 1.0));

    // Treble filter: high-pass above 2kHz
    trebleFilterL.setCoefficients(juce::IIRCoefficients::makeHighPass(currentSampleRate, 2000.0));
    trebleFilterR.setCoefficients(juce::IIRCoefficients::makeHighPass(currentSampleRate, 2000.0));
}

//==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    transport.prepareToPlay(samplesPerBlockExpected, sampleRate);

    // Reset filters
    bassFilterL.reset();
    bassFilterR.reset();
    midFilterL.reset();
    midFilterR.reset();
    trebleFilterL.reset();
    trebleFilterR.reset();

    setupFilters();

    // Initialize frequency bands arrays
    {
        const juce::SpinLock::ScopedLockType sl(bandsLock);
        lastFrequencyBands.clearQuick();
        smoothedFrequencyBands.clearQuick();
        lastFrequencyBands = { 0.0f, 0.0f, 0.0f };
        smoothedFrequencyBands = { 0.0f, 0.0f, 0.0f };
    }
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // 1) All zero if no source ------------------
    if (readerSource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        // Also reset frequency bands to zero when no source
        const juce::SpinLock::ScopedLockType sl(bandsLock);
        for (int i = 0; i < smoothedFrequencyBands.size(); ++i)
            smoothedFrequencyBands.set(i, 0.0f);
        lastFrequencyBands = smoothedFrequencyBands;
        return;
    }

    transport.getNextAudioBlock(bufferToFill);

    // 2) Get audio buffer data -------------------
    auto* buffer = bufferToFill.buffer;
    if (buffer == nullptr || bufferToFill.numSamples <= 0)
        return;

    const int numChans = buffer->getNumChannels();
    const int n = bufferToFill.numSamples;
    const int start = bufferToFill.startSample;

    // CREAR BUFFER TEMPORALES PARA LAS SEÑALES FILTRADAS
    juce::AudioBuffer<float> bassBuffer(numChans, n);
    juce::AudioBuffer<float> midBuffer(numChans, n);
    juce::AudioBuffer<float> trebleBuffer(numChans, n);

    // COPIA LA SEÑAL ORIGINAL A LOS BUFFERES FILTRADOS
    for (int ch = 0; ch < numChans; ++ch)
    {
        bassBuffer.copyFrom(ch, 0, *buffer, ch, start, n);
        midBuffer.copyFrom(ch, 0, *buffer, ch, start, n);
        trebleBuffer.copyFrom(ch, 0, *buffer, ch, start, n);
    }

    // APLICAR FILTROS
    for (int ch = 0; ch < numChans; ++ch)
    {
        float* bassData = bassBuffer.getWritePointer(ch);
        float* midData = midBuffer.getWritePointer(ch);
        float* trebleData = trebleBuffer.getWritePointer(ch);

        for (int i = 0; i < n; ++i)
        {
            if (ch == 0)
            {
                bassData[i] = bassFilterL.processSingleSampleRaw(bassData[i]);
                midData[i] = midFilterL.processSingleSampleRaw(midData[i]);
                trebleData[i] = trebleFilterL.processSingleSampleRaw(trebleData[i]);
            }
            else
            {
                bassData[i] = bassFilterR.processSingleSampleRaw(bassData[i]);
                midData[i] = midFilterR.processSingleSampleRaw(midData[i]);
                trebleData[i] = trebleFilterR.processSingleSampleRaw(trebleData[i]);
            }
        }
    }

    // CALCULAR RMS PARA CADA BANDA DE FRENCUENCIA
    juce::Array<float> instantBands;
    instantBands.insertMultiple(0, 0.0f, 3); // [graves, medios, agudos]

    // GRAVES RMS
    double bassSumSquares = 0.0;
    for (int ch = 0; ch < numChans; ++ch)
    {
        const float* data = bassBuffer.getReadPointer(ch);
        for (int i = 0; i < n; ++i)
        {
            const float s = data[i];
            bassSumSquares += (double)s * (double)s;
        }
    }
    auto bassRms = std::sqrt((float)(bassSumSquares / (double)(n * numChans)));
    instantBands.set(0, bassRms);

    // MEDIOS RMS
    double midSumSquares = 0.0;
    for (int ch = 0; ch < numChans; ++ch)
    {
        const float* data = midBuffer.getReadPointer(ch);
        for (int i = 0; i < n; ++i)
        {
            const float s = data[i];
            midSumSquares += (double)s * (double)s;
        }
    }
    auto midRms = std::sqrt((float)(midSumSquares / (double)(n * numChans)));
    instantBands.set(1, midRms);

    // AGUDOS RMS
    double trebleSumSquares = 0.0;
    for (int ch = 0; ch < numChans; ++ch)
    {
        const float* data = trebleBuffer.getReadPointer(ch);
        for (int i = 0; i < n; ++i)
        {
            const float s = data[i];
            trebleSumSquares += (double)s * (double)s;
        }
    }
    auto trebleRms = std::sqrt((float)(trebleSumSquares / (double)(n * numChans)));
    instantBands.set(2, trebleRms);

    // SMOOTHING PARA CADA BANDA
    {
        const juce::SpinLock::ScopedLockType sl(bandsLock);

        // Graves smoothing
        const float bassA = juce::jlimit(0.0f, 1.0f, bassSmoothingAlpha);
        const float bassB = 1.0f - bassA;
        const float bassSm = bassB * instantBands[0] + bassA * smoothedFrequencyBands[0];
        smoothedFrequencyBands.set(0, bassSm);

        // Medios smoothing
        const float midA = juce::jlimit(0.0f, 1.0f, midSmoothingAlpha);
        const float midB = 1.0f - midA;
        const float midSm = midB * instantBands[1] + midA * smoothedFrequencyBands[1];
        smoothedFrequencyBands.set(1, midSm);

        // Agudos smoothing
        const float trebleA = juce::jlimit(0.0f, 1.0f, trebleSmoothingAlpha);
        const float trebleB = 1.0f - trebleA;
        const float trebleSm = trebleB * instantBands[2] + trebleA * smoothedFrequencyBands[2];
        smoothedFrequencyBands.set(2, trebleSm);

        lastFrequencyBands = smoothedFrequencyBands;
    }
}

void MainComponent::releaseResources()
{
    transport.releaseResources();
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    // Draw frequency band bars
    auto bounds = getLocalBounds().reduced(20);
    auto bandValues = getLatestFrequencyBands();

    const juce::String bandNames[] = { "GRAVES", "MEDIOS", "AGUDOS" };
    const juce::Colour bandColors[] = {
        juce::Colours::blue,
        juce::Colours::red,
        juce::Colours::yellow
    };

    const int numBands = juce::jmax(1, bandValues.size());
    auto barsArea = bounds;
    const int gap = 20;
    const int barWidth = (barsArea.getWidth() - (gap * (numBands - 1))) / numBands;

    // Draw frequency band bars and numeric labels
    g.setFont(juce::Font(juce::FontOptions(14.0f)));
    for (int i = 0; i < numBands; ++i)
    {   
        const float value = juce::jlimit(0.0f, 1.0f, bandValues[i]);
        auto h = juce::roundToInt((float)barsArea.getHeight() * value);

        // Bar meter
        auto bar = juce::Rectangle<int>(i * (barWidth + gap) + barsArea.getX(),
            barsArea.getBottom() - h,
            barWidth,
            h);

        // Bar with band-specific color
        g.setColour(bandColors[i]);
        g.fillRect(bar);

        // Band name label above the bar
        juce::String labelText = bandNames[i] + "\n" + juce::String(value, 2);

        // Place the label above the top of the bar
        const int labelHeight = 36;
        auto labelBounds = juce::Rectangle<int>(bar.getX(),
            bar.getY() - labelHeight - 5,
            bar.getWidth(),
            labelHeight);

        // Draw label with contrasting colour and centered
        g.setColour(juce::Colours::white);
        g.drawFittedText(labelText, labelBounds, juce::Justification::centred, 2);
    }
}

void MainComponent::resized()
{
    // Simple horizontal layout for the buttons and slider row
    auto area = getLocalBounds().reduced(20);
    auto buttonHeight = 32;
    auto row = area.removeFromTop(buttonHeight);

    loadButton.setBounds(row.removeFromLeft(120));
    row.removeFromLeft(10);
    playButton.setBounds(row.removeFromLeft(120));
    row.removeFromLeft(10);
    stopButton.setBounds(row.removeFromLeft(120));

    // Rows for frequency band smoothing controls
    auto bassRow = area.removeFromTop(28);
    bassSmoothingLabel.setBounds(bassRow.removeFromLeft(100));
    bassRow.removeFromLeft(8);
    bassSmoothingSlider.setBounds(bassRow.removeFromLeft(juce::jmax(200, bassRow.getWidth() / 2)));

    auto midRow = area.removeFromTop(28);
    midSmoothingLabel.setBounds(midRow.removeFromLeft(100));
    midRow.removeFromLeft(8);
    midSmoothingSlider.setBounds(midRow.removeFromLeft(juce::jmax(200, midRow.getWidth() / 2)));

    auto trebleRow = area.removeFromTop(28);
    trebleSmoothingLabel.setBounds(trebleRow.removeFromLeft(100));
    trebleRow.removeFromLeft(8);
    trebleSmoothingSlider.setBounds(trebleRow.removeFromLeft(juce::jmax(200, trebleRow.getWidth() / 2)));

    // OSC minimal row(s)
    auto oscRow1 = area.removeFromTop(26);
    hostLabel.setBounds(oscRow1.removeFromLeft(50));
    oscRow1.removeFromLeft(6);
    hostEdit.setBounds(oscRow1.removeFromLeft(160));
    oscRow1.removeFromLeft(12);
    portLabel.setBounds(oscRow1.removeFromLeft(40));
    oscRow1.removeFromLeft(6);
    portEdit.setBounds(oscRow1.removeFromLeft(80));
    oscRow1.removeFromLeft(12);
    addrLabel.setBounds(oscRow1.removeFromLeft(70));
    oscRow1.removeFromLeft(6);
    addrEdit.setBounds(oscRow1.removeFromLeft(180));
    oscRow1.removeFromLeft(12);
    oscEnableToggle.setBounds(oscRow1.removeFromLeft(120));
}

//==============================================================================
void MainComponent::buttonClicked(juce::Button* button)
{
    if (button == &loadButton) chooseAndLoadFile();
    if (button == &playButton) { transport.start(); setButtonsEnabledState(); }
    if (button == &stopButton) { transport.stop();  setButtonsEnabledState(); }
}

void MainComponent::setButtonsEnabledState()
{
    const bool hasFile = (readerSource != nullptr);
    const bool isPlaying = transport.isPlaying();

    playButton.setEnabled(hasFile && !isPlaying);
    stopButton.setEnabled(hasFile && isPlaying);

    // Drive UI updates while playing; stop when not.
    if (isPlaying)
    {
        if (!isTimerRunning())
            startTimerHz(30);
    }
    else
    {
        stopTimer();
        // One last repaint to show zeroed meters if stopped
        repaint();
    }
}

juce::Array<float> MainComponent::getLatestFrequencyBands() const
{
    const juce::SpinLock::ScopedLockType sl(bandsLock);
    return lastFrequencyBands; // returns a copy
}

void MainComponent::timerCallback()
{
    // Poll transport state transition: if playback stopped externally, update buttons/timer
    if (!transport.isPlaying())
    {
        // If we reached the end of the file, rewind so Play works again
        const double len = transport.getLengthInSeconds();
        if (len > 0.0 && transport.getCurrentPosition() >= len - 1e-6)
            transport.setPosition(0.0);

        setButtonsEnabledState();
        return;
    }

    // Trigger repaint to update meters
    repaint();

    /// Send OSC (from message thread) if enabled
    if (oscEnableToggle.getToggleState() && oscConnected)
    {
        auto values = getLatestFrequencyBands();
        sendFrequencyBandsOverOsc(values);
    }
}

void MainComponent::chooseAndLoadFile()
{
    // Use async FileChooser so GUI stays responsive
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
    // Stop current playback and detach current source
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

    oscConnected = oscSender.connect(oscHost, oscPort);
}

void MainComponent::disconnectOsc()
{
    if (oscConnected)
    {
        oscSender.disconnect();
        oscConnected = false;
    }
}

void MainComponent::sendFrequencyBandsOverOsc(const juce::Array<float>& values)
{
    if (!oscConnected)
        return;

    // Build message: /address <float bass> <float mid> <float treble>
    juce::OSCMessage msg(oscAddress.isEmpty() ? "/frequencyBands" : oscAddress);

    for (auto v : values)
        msg.addFloat32(juce::jlimit(0.0f, 1.0f, v));

    // Best-effort send; ignore failures here
    (void)oscSender.send(msg);
}

void MainComponent::reconnectOscIfEnabled()
{
    if (oscEnableToggle.getToggleState())
    {
        oscHost = hostEdit.getText().trim();
        oscPort = portEdit.getText().getIntValue();
        oscAddress = addrEdit.getText().trim();
        if (oscAddress.isEmpty())
            oscAddress = "/frequencyBands";

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
        if (oscAddress.isEmpty())
            oscAddress = "/frequencyBands";

        updateOscConnection();

        if (!oscConnected)
            oscEnableToggle.setToggleState(false, juce::dontSendNotification);
    }
    else
    {
        disconnectOsc();
    }
}