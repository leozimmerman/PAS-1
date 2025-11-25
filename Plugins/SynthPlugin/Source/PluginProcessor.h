#pragma once

#include <JuceHeader.h>

/**
 - Necesita modulo juce_dsp
 - Plugin MIDI Input → Enabled
 - Plugin is a Synth → Enabled
 */

//==============================================================================
// Un simple sinte analógico-style: osc + ADSR + filtro LP
// 
class SynthPluginProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    SynthPluginProcessor();
    ~SynthPluginProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #if ! JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock; // para la versión double si la querés luego

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    //==============================================================================
    const juce::String getName() const override              { return JucePlugin_Name; }
    bool acceptsMidi() const override                        { return true; }
    bool producesMidi() const override                       { return false; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 0.0; }

    //==============================================================================
    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // MIDI virtual keyboard state compartido con el editor
    juce::MidiKeyboardState keyboardState;

    juce::AudioProcessorValueTreeState apvts;

    // Métodos internos (ya existentes)
    void setWaveform (int index); // 0: sine, 1: saw, 2: square
    void setAdsr (float attack, float decay, float sustain, float release);
    void setFilter (float cutoff, float reso);

private:
    //==============================================================================
    // DSP
    juce::dsp::Oscillator<float> osc;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Gain<float> outputGain;
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;

    juce::dsp::ProcessSpec spec {};
    juce::SmoothedValue<float> velocityGain;

    // Estado
    std::atomic<float> targetFrequencyHz { 440.0f };
    std::atomic<int>   currentWaveform   { 0 };      // 0: Sine, 1: Saw, 2: Square

    std::atomic<float> cutoffHz { 20000.0f };
    std::atomic<float> resonance { 0.7f };

    std::atomic<int> activeNote { -1 };

    // Helpers internos
    void startNote (int midiNoteNumber, float velocity);
    void stopNote (int midiNoteNumber);

    void updateFilterFromAtomics();

    static float midiToHz (int midiNote) noexcept;

    // Layout de parámetros para APVTS
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthPluginProcessor)
};
