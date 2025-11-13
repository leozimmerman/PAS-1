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

void MainComponent::setupGuiComponents()
{
    addAndMakeVisible (loadButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);

    loadButton.onClick = [this] { chooseAndLoadFile(); };
    playButton.onClick = [this] { transport.start(); setButtonsEnabledState(); };
    stopButton.onClick = [this] { transport.stop();  setButtonsEnabledState(); };

    setButtonsEnabledState();
}

void MainComponent::setupAudioPlayer()
{
    // Register audio formats we can read
    formatManager.registerBasicFormats();
    
    // Listen for transport state changes (start/stop/end-of-stream)
    juce::MessageManagerLock mmLock; // ensure we're on the message thread for listener ops
    transport.addChangeListener (this); /// Revisar doc
    
    setAudioChannels (0, 2);
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused (samplesPerBlockExpected);
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
}

void MainComponent::releaseResources()
{
    transport.releaseResources();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    // Simple horizontal layout for the three buttons
    auto area = getLocalBounds().reduced(20);
    auto buttonHeight = 32;
    auto row = area.removeFromTop (buttonHeight);

    loadButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (10);
    playButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (10);
    stopButton.setBounds (row.removeFromLeft (120));
}

//==============================================================================
void MainComponent::buttonClicked (juce::Button* button)
{
    // Not used because we used onClick lambdas, but kept for completeness
    if (button == &loadButton) chooseAndLoadFile();
    if (button == &playButton)
    {
        transport.start();
        setButtonsEnabledState();
    }
    if (button == &stopButton)
    {
        transport.stop();
        setButtonsEnabledState();
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
        auto url = fc.getURLResult(); 
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

    // Set the source; pass the file's sample rate
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
