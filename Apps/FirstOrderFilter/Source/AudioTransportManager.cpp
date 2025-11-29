#include "AudioTransportManager.h"

AudioTransportManager::AudioTransportManager()
{
    formatManager.registerBasicFormats();
}

AudioTransportManager::~AudioTransportManager()
{
    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();
}

void AudioTransportManager::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    transport.prepareToPlay (samplesPerBlockExpected, sampleRate);
}

void AudioTransportManager::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (readerSource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    transport.getNextAudioBlock (bufferToFill);
}

void AudioTransportManager::releaseResources()
{
    transport.releaseResources();
}

bool AudioTransportManager::loadURL (const juce::URL& url)
{
    stop();
    transport.setSource (nullptr);
    readerSource.reset();

    std::unique_ptr<juce::InputStream> inputStream (url.createInputStream (false));
    if (inputStream == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (std::move (inputStream)));
    if (reader == nullptr)
        return false;

    const double fileSampleRate = reader->sampleRate;

    readerSource.reset (new juce::AudioFormatReaderSource (reader.release(), true));
    transport.setSource (readerSource.get(), 0, nullptr, fileSampleRate);
    transport.setPosition (0.0);

    return true;
}

void AudioTransportManager::start()
{
    transport.start();
}

void AudioTransportManager::stop()
{
    transport.stop();
}

void AudioTransportManager::setPosition (double seconds)
{
    transport.setPosition (seconds);
}

bool AudioTransportManager::isPlaying() const
{
    return transport.isPlaying();
}

bool AudioTransportManager::hasStreamFinished() const
{
    return transport.hasStreamFinished();
}

bool AudioTransportManager::hasFileLoaded() const
{
    return readerSource != nullptr;
}

void AudioTransportManager::addChangeListener (juce::ChangeListener* listener)
{
    transport.addChangeListener(listener);
}

void AudioTransportManager::removeChangeListener (juce::ChangeListener* listener)
{
    transport.removeChangeListener(listener);
}

void AudioTransportManager::chooseAndLoadFile()
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
