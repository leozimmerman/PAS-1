#pragma once

#include <JuceHeader.h>

class MainComponent  : public juce::AudioAppComponent,
                       private juce::Button::Listener,
                       private juce::ChangeListener
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    
    void setupGuiComponents();
    void setupAudioPlayer();

private:
    //==============================================================================
    // Audio playback members
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    // Simple UI
    juce::TextButton loadButton { "Load..." };
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };

    // Button::Listener
    void buttonClicked (juce::Button* button) override;

    // ChangeListener (to observe transport state changes)
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    // Helpers
    void chooseAndLoadFile();
    void loadURL (const juce::URL& url);
    void setButtonsEnabledState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
