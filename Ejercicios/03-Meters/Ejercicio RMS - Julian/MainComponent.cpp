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
}

void MainComponent::setupGuiComponents()
{
    addAndMakeVisible(loadButton);
    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);

    smoothingLabel.setText("Smoothing", juce::dontSendNotification);
    smoothingLabel.setJustificationType(juce::Justification::centredLeft);
    smoothingLabel.setColour(juce::Label::textColourId, juce::Colours::black); 
    addAndMakeVisible(smoothingLabel);

    smoothingSlider.setRange(0.0, 1.0, 0.001);
    // Invert mapping: slider shows "smoothing amount", alpha is "snappiness".
    smoothingSlider.setValue(rmsSmoothingAlpha, juce::dontSendNotification);
    smoothingSlider.setTextValueSuffix("");
    smoothingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    smoothingSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::black); 
    smoothingSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white); 
    smoothingSlider.onValueChange = [this]
        {
            const double sliderVal = smoothingSlider.getValue();
            rmsSmoothingAlpha = (float)(sliderVal);
        };
    addAndMakeVisible(smoothingSlider);

    // Noise control setup
    noiseLabel.setText("Noise", juce::dontSendNotification);
    noiseLabel.setJustificationType(juce::Justification::centredLeft);
    noiseLabel.setColour(juce::Label::textColourId, juce::Colours::black); 
    addAndMakeVisible(noiseLabel);

    noiseSlider.setRange(0.0, 50.0, 0.1);
    noiseSlider.setValue(noiseAmount, juce::dontSendNotification);
    noiseSlider.setTextValueSuffix("");
    noiseSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    noiseSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::black); 
    noiseSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white); 
    noiseSlider.onValueChange = [this]
        {
            noiseAmount = (float)noiseSlider.getValue();
        };
    addAndMakeVisible(noiseSlider);

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

    setButtonsEnabledState();
}

void MainComponent::setupAudioPlayer()
{
    formatManager.registerBasicFormats();
    setAudioChannels(0, 2); // 
}

//==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused(samplesPerBlockExpected);
    transport.prepareToPlay(samplesPerBlockExpected, sampleRate);

    // Prepare RMS arrays to current output channels (from device)
    int numOutChans = 1;
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        numOutChans = juce::jmax(1, dev->getActiveOutputChannels().countNumberOfSetBits());

    {
        const juce::SpinLock::ScopedLockType sl(rmsLock);
        lastRms.clearQuick();
        smoothedRms.clearQuick();
        lastRms.insertMultiple(0, 0.0f, numOutChans);
        smoothedRms.insertMultiple(0, 0.0f, numOutChans);
    }
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // 1) All zero if no source ------------------
    if (readerSource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        // Also reset RMS to zero when no source
        const juce::SpinLock::ScopedLockType sl(rmsLock);
        for (int c = 0; c < smoothedRms.size(); ++c)
            smoothedRms.set(c, 0.0f);
        lastRms = smoothedRms;
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

    // ) Convert to mono and duplicate in both channels
    if (numChans >= 2)
    {
        auto* leftChannel = buffer->getWritePointer(0, start);
        auto* rightChannel = buffer->getWritePointer(1, start);

        // Mix down to mono and write to both channels
        for (int i = 0; i < n; ++i)
        {
            // Mix left and right to create mono signal
            const float monoSample = (leftChannel[i] + rightChannel[i]) * 0.5f;

            // Write the same mono signal to both channels
            leftChannel[i] = monoSample;
            rightChannel[i] = monoSample;
        }
    }

    // 3) Calculate instant RMS
    juce::Array<float> instantRms;
    instantRms.insertMultiple(0, 0.0f, numChans);

    for (int ch = 0; ch < numChans; ++ch)
    {
        const float* data = buffer->getReadPointer(ch, start);

        double sumSquares = 0.0;
        for (int i = 0; i < n; ++i) // n = bufferSize
        {
            const float s = data[i];
            sumSquares += (double)s * (double)s;
        }
        auto sumSquaresDiv = sumSquares / (double)n;
        const float rms = std::sqrt((float)sumSquaresDiv);
        instantRms.set(ch, rms);
    }

    // Exponential smoothing and publishs
    {
        const juce::SpinLock::ScopedLockType sl(rmsLock); // Lock
        const float a = juce::jlimit(0.0f, 1.0f, rmsSmoothingAlpha);
        const float b = 1.0f - a;

        for (int ch = 0; ch < smoothedRms.size(); ++ch)
        {
            const float sm = b * instantRms[ch] + a * smoothedRms[ch];
            smoothedRms.set(ch, sm);
            lastRms.set(ch, sm);
        }
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
    g.fillAll(juce::Colours::whitesmoke);

    // Get RMS values and calculate peak
    auto rmsValues = getLatestRms();
    if (rmsValues.isEmpty())
        return;

    // Use the maximum RMS value from all channels
    float peakRms = 0.0f;
    for (const auto& rms : rmsValues)
        peakRms = juce::jmax(peakRms, rms);

    // Setup for bars
    const int totalBars = 140;
    const int centerBar = totalBars / 2;
    
    auto bounds = getLocalBounds().reduced(20);
    auto barsArea = bounds;
    const int gap = 2;
    const int barWidth = (barsArea.getWidth() - (gap * (totalBars - 1))) / totalBars;

    // Calculate how many bars should be lit based on peak RMS
    const float maxRmsForGradient = 0.08f;
    const float normalizedPeak = juce::jlimit(0.0f, 1.0f, peakRms / maxRmsForGradient);
    const int barsToLight = juce::roundToInt(normalizedPeak * centerBar);

    // Define the gradient colors
    const juce::Colour color1(0xff355c7d); // #355c7d (dark blue)
    const juce::Colour color2(0xff6c5b7b); // #6c5b7b (purple)
    const juce::Colour color3(0xffc06c84); // #c06c84 (pink)

    // Draw the bars with noise
    for (int i = 0; i < totalBars; ++i)
    {
        // Calculate distance from center
        int distanceFromCenter = std::abs(i - (centerBar - 1));
        
        // Determine if this bar should be lit
        bool isLit = distanceFromCenter < barsToLight;
        
        // Bar dimensions
        auto barHeight = isLit ? juce::roundToInt((float)barsArea.getHeight() * 0.6f) : 20;
        
        auto barX = i * (barWidth + gap) + barsArea.getX();
        auto barY = barsArea.getBottom() - barHeight;

        // Color calculation
        juce::Colour barColour;
        if (isLit)
        {
            // Calculate normalized position (0.0 = center, 1.0 = edge)
            float normalizedDistance = (float)distanceFromCenter / (float)centerBar;
            
            // Create three-color gradient
            if (normalizedDistance <= 0.5f)
            {
                // Interpolate between color1 (center) and color2 (middle)
                float ratio = normalizedDistance * 2.0f; // 0.0 to 1.0
                barColour = color1.interpolatedWith(color2, ratio);
            }
            else
            {
                // Interpolate between color2 (middle) and color3 (edge)
                float ratio = (normalizedDistance - 0.5f) * 2.0f; // 0.0 to 1.0
                barColour = color2.interpolatedWith(color3, ratio);
            }
        }
        else
        {
            // Unlit bars are dark gray
            barColour = juce::Colours::whitesmoke;
        }

        // Create noisy shape instead of simple rectangle
        if (noiseAmount > 0.0f && barHeight > 20)
        {
            juce::Path noisyBar;
            
            // Start from bottom-left
            float startX = (float)barX;
            float startY = (float)(barY + barHeight);
            noisyBar.startNewSubPath(startX, startY);
            
            // Create noisy edges
            const int noisePoints = juce::jmax(4, barWidth / 2); // Number of noise points per side
            
            // Left edge (bottom to top) - add noise to X
            for (int p = 1; p < noisePoints; ++p)
            {
                float progress = (float)p / (float)noisePoints;
                float y = startY - (progress * barHeight);
                float noiseOffset = noiseGenerator.nextFloat() * noiseAmount - (noiseAmount * 0.5f);
                float x = startX + noiseOffset;
                noisyBar.lineTo(x, y);
            }
            
            // Top edge (left to right) - add noise to Y
            float topY = (float)barY;
            for (int p = 0; p <= barWidth; p += 2)
            {
                float x = startX + (float)p;
                float noiseOffset = noiseGenerator.nextFloat() * noiseAmount - (noiseAmount * 0.5f);
                float y = topY + noiseOffset;
                noisyBar.lineTo(x, y);
            }
            
            // Right edge (top to bottom) - add noise to X
            float endX = startX + (float)barWidth;
            for (int p = noisePoints - 1; p > 0; --p)
            {
                float progress = (float)p / (float)noisePoints;
                float y = (float)barY + ((1.0f - progress) * barHeight);
                float noiseOffset = noiseGenerator.nextFloat() * noiseAmount - (noiseAmount * 0.5f);
                float x = endX + noiseOffset;
                noisyBar.lineTo(x, y);
            }
            
            // Close the path back to start
            noisyBar.closeSubPath();
            
            // Draw the noisy bar
            g.setColour(barColour);
            g.fillPath(noisyBar);
        }
        else
        {
            // Draw simple rectangle for small bars or no noise
            auto bar = juce::Rectangle<int>(barX, barY, barWidth, barHeight);
            g.setColour(barColour);
            g.fillRect(bar);
        }
    }

    // Optional: Draw peak RMS value in center
    g.setFont(juce::Font(juce::FontOptions(16.0f)));
    g.setColour(juce::Colours::black);
    juce::String peakText = juce::String(peakRms, 3);
    g.drawFittedText(peakText, bounds.removeFromTop(30), juce::Justification::centred, 1);
}

void MainComponent::resized()
{
    // Simple horizontal layout for the buttons and slider rows
    auto area = getLocalBounds().reduced(20);
    auto buttonHeight = 32;
    auto row = area.removeFromTop(buttonHeight);

    loadButton.setBounds(row.removeFromLeft(120));
    row.removeFromLeft(10);
    playButton.setBounds(row.removeFromLeft(120));
    row.removeFromLeft(10);
    stopButton.setBounds(row.removeFromLeft(120));

    // Next row for smoothing control
    auto controlRow = area.removeFromTop(28);
    smoothingLabel.setBounds(controlRow.removeFromLeft(100));
    controlRow.removeFromLeft(8);
    smoothingSlider.setBounds(controlRow.removeFromLeft(juce::jmax(200, controlRow.getWidth() / 3)));

    // Next row for noise control
    controlRow.removeFromLeft(10);
    noiseLabel.setBounds(controlRow.removeFromLeft(100));
    controlRow.removeFromLeft(8);
    noiseSlider.setBounds(controlRow.removeFromLeft(juce::jmax(200, controlRow.getWidth() / 2)));
}

//==============================================================================
void MainComponent::buttonClicked(juce::Button* button)
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

    playButton.setEnabled(hasFile && !isPlaying);
    stopButton.setEnabled(hasFile && isPlaying);

    // Drive UI updates while playing; stop when not.
    if (isPlaying)
    {
        if (!isTimerRunning())
            startTimerHz(30); // Test different values
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
    const juce::SpinLock::ScopedLockType sl(rmsLock);
    return lastRms; // returns a copy
}

void MainComponent::timerCallback()
{
    // Poll transport state transition: if playback stopped externally, update buttons/timer
    /// Test commenting this code and check what happens when file ends playing
    if (!transport.isPlaying())
    {
        // If we reached the end of the file, rewind so Play works again
        const double len = transport.getLengthInSeconds();
        if (len > 0.0 && transport.getCurrentPosition() >= len - 1e-6)
            transport.setPosition(0.0);

        setButtonsEnabledState();// setButtonsEnabledState will stopTimer and repaint
        return;
    }

    // Trigger repaint to update meters and DBG output
    repaint(); // Test commenting
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
            auto url = fc.getURLResult(); // Works for local files and sandboxed URLs (iOS/macOS)
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

    // AudioFormatReader: Reads samples from an audio file stream.
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(std::move(inputStream)));
    if (reader == nullptr)
        return;

    // Capture the file's sample rate from the reader before transferring ownership
    const double fileSampleRate = reader->sampleRate;

    // Create the reader source (takes ownership of reader)
    readerSource.reset(new juce::AudioFormatReaderSource(reader.release(), true));

    // Set the source; pass the file's sample rate so Transport can resample if needed
    transport.setSource(readerSource.get(), 0, nullptr, fileSampleRate);

    // Reset position to start
    transport.setPosition(0.0);

    setButtonsEnabledState();
}