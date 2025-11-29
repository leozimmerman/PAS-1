#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent : public juce::AudioAppComponent,
    private juce::ChangeListener
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    //==============================================================================
    // Audio playback members
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    // Simple UI
    juce::TextButton loadButton{ "Load..." };
    juce::TextButton playButton{ "Play" };
    juce::TextButton stopButton{ "Stop" };

    // Delay parameter controls
    juce::Slider mixSlider; // Agrego un slider para la mezcla dry/wet
    juce::Label  mixLabel{ {}, "Dry/Wet" };

    /*
    juce::Slider delayTimeSlider{ juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label  delayTimeLabel{ {}, "Time (ms)" };
    */
    // Reemplazo el slider de delayTime (delay fijo) por uno de Depth,
    // porque ahora el delay se modula con el LFO y solo controlamos
    // la profundidad máxima de esa variación.
    juce::Slider depthSlider{ juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label  depthLabel{ {}, "Depth (ms)" };

    // Agrego un slider para controlar la velocidad del LFO que modula el delay.
    // Valores típicos de flanger: 0.01–10 Hz.
    juce::Slider rateSlider{ juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label  rateLabel{ {}, "Rate (Hz)" };

    juce::Slider feedbackSlider{ juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label  feedbackLabel{ {}, "Feedback" };

    // ChangeListener (to observe transport state changes)
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Helpers
    void chooseAndLoadFile();
    void loadURL(const juce::URL& url);
    void setButtonsEnabledState();

    //==============================================================================
    // Parameters (controlados por sliders)
    float wetMix = 0.5f; // Agregado: wetMix y dryMix para la mezcla dry/wet
    float dryMix = 0.5f;
    //float delayTimeMs = 400.0f;  // delay time in milliseconds (mapped to delaySamples)
    float depthMs = 5.0f;  // 0–20 ms
    float lfoRateHz = 0.5f;  // 0.01–10 Hz
    float feedback = 0.35f;   // 0..<1

    float lfoPhase = 0.0f;
    float lfoIncrement = 0.0f;




    

    // Delay buffer (single channel)
    std::vector<float> delayBuffer;
    int writePos = 0;
    //int delaySamples = 22050;      // default (0.5s @ 44.1kHz); clamped in prepare

    // Limits and rate
    int maxDelaySamples = 0;

    // para el wire-AND-ing
    int delayMask = 0;

    double currentSampleRate = 44100.0;

    void prepareDelayState();
    void processFlangerChannel(juce::AudioBuffer<float>& buffer, int channelNum);
    float processLFO();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};