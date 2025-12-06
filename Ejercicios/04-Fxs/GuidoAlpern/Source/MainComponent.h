#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  :  public juce::AudioAppComponent,
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
    juce::ToggleButton skipToggle;

    // Delay parameter controls
    juce::Slider delayTimeSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label  delayTimeLabel  { {}, "Time (ms)" };

    juce::Slider feedbackSlider  { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label  feedbackLabel   { {}, "Feedback" };

    // ChangeListener (to observe transport state changes)
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    // Helpers
    void chooseAndLoadFile();
    void loadURL (const juce::URL& url);
    void setButtonsEnabledState();

    //==============================================================================
    // Simple mono delay state (channel 0 only)
    // Parameters
    float delayTimeMs   = 400.0f;  // delay time in milliseconds (mapped to delaySamples)
    float feedback      = 0.35f;   // 0..<1

    // Delay buffer (single channel)
    std::vector<float> delayBuffer;
    int writePos = 0;
    int delaySamples = 22050;      // default (0.5s @ 44.1kHz); clamped in prepare

    // Limits and rate
    int maxDelaySamples = 0;
    double currentSampleRate = 44100.0;

    void prepareDelayState();
    void processDelayChannel (juce::AudioBuffer<float>& buffer, int channelNum);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
