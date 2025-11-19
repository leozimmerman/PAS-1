#pragma once

#include <JuceHeader.h>

// Encapsulates file loading and playback via AudioTransportSource
class AudioTransportManager
{
public:
    AudioTransportManager();
    ~AudioTransportManager();

    // lifecycle with device
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill);
    void releaseResources();

    // loading
    bool loadURL (const juce::URL& url);

    // transport controls
    void start();
    void stop();
    void setPosition (double seconds);
    bool isPlaying() const;
    bool hasStreamFinished() const;
    bool hasFileLoaded() const;

    // allow external listeners to observe transport changes
    void addChangeListener (juce::ChangeListener* listener);
    void removeChangeListener (juce::ChangeListener* listener);
    
    juce::AudioTransportSource* getTransport() { return &transport; }

private:
    // forward transport state changes to our own broadcaster so MainComponent can listen via manager
    void transportChangeCallback (juce::ChangeBroadcaster*);

    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTransportManager)
};
