#pragma once

#include <JuceHeader.h>

//==============================================================================
// A simple analog-style synth: oscillator + ADSR + state-variable low-pass filter
class MainComponent  : public juce::AudioAppComponent,
                       public juce::MidiKeyboardStateListener,
                       private juce::ComboBox::Listener,
                       private juce::Slider::Listener
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

    // MidiKeyboardStateListener
    void handleNoteOn  (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

private:
    //==============================================================================
    // UI
    juce::ComboBox waveformBox;
    juce::Label waveformLabel;

    // Removed freqSlider/freqLabel

    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;

    juce::Slider cutoffSlider, resonanceSlider;
    juce::Label cutoffLabel, resonanceLabel;

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboardComponent { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    // DSP
    juce::dsp::Oscillator<float> osc;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Gain<float> outputGain;
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;

    juce::dsp::ProcessSpec spec {};
    
    juce::SmoothedValue<float> velocityGain;

    // State
    std::atomic<float> targetFrequencyHz { 440.0f };
    std::atomic<int> currentWaveform { 0 }; // 0: Sine, 1: Saw, 2: Square

    std::atomic<float> cutoffHz { 20000.0f };
    std::atomic<float> resonance { 0.7f };

    std::atomic<int> activeNote { -1 };

    // Helpers
    void setWaveform (int index);
    void updateAdsrParamsFromUI();
    void updateFilterFromUI();
    void startNote (int midiNoteNumber, float velocity);
    void stopNote (int midiNoteNumber);

    // Listeners
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;
    void sliderValueChanged (juce::Slider* slider) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
