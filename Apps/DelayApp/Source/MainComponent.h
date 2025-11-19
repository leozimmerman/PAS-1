#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
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

    // Delay parameter controls
    juce::Slider delayTimeSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label  delayTimeLabel  { {}, "Time (ms)" };

    juce::Slider feedbackSlider  { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label  feedbackLabel   { {}, "Feedback" };

    juce::Slider wetSlider       { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label  wetLabel        { {}, "Wet" };

    juce::Slider drySlider       { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label  dryLabel        { {}, "Dry" };

    // Button::Listener
    void buttonClicked (juce::Button* button) override;

    // ChangeListener (to observe transport state changes)
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    // Helpers
    void chooseAndLoadFile();
    void loadURL (const juce::URL& url);
    void setButtonsEnabledState();

    //==============================================================================
    // Very simple delay effect (no JUCE delay classes)
    // Parameters
    float delayTimeMs   = 400.0f;  // delay time in milliseconds
    float feedback      = 0.35f;   // 0..<1
    float wet           = 0.35f;   // 0..1
    float dry           = 1.0f - wet;

    // Delay buffers (one per channel)
    std::vector<std::vector<float>> delayBufferPerChannel;
    std::vector<int> writePositions;
    int maxDelaySamples = 0;
    double currentSampleRate = 44100.0;

    void resetDelayState();
    void processDelay (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
