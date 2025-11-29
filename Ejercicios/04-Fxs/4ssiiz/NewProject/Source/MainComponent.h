#pragma once

#include <JuceHeader.h>

//==============================================================================
// MÓDULO: Synth + Delay - Generador de Audio con Procesamiento
//==============================================================================
// MainComponent combina:
// - AnalogSynth: Generador de audio (oscilador, ADSR, filtro) con teclado MIDI
// - DelayApp: Procesador de delay estéreo para procesar la señal generada
// 
// Flujo: Synth genera señal → Delay procesa → Salida al SO
//==============================================================================

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
    // MÓDULO: Componentes de Interfaz - Synth
    //==============================================================================
    juce::ComboBox waveformBox;
    juce::Label waveformLabel;

    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;

    juce::Slider cutoffSlider, resonanceSlider;
    juce::Label cutoffLabel, resonanceLabel;

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboardComponent { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    //==============================================================================
    // MÓDULO: Componentes de Interfaz - Delay
    //==============================================================================
    juce::Slider delayTimeSlider;
    juce::Label  delayTimeLabel  { {}, "Delay Time (ms)" };

    juce::Slider feedbackSlider;
    juce::Label  feedbackLabel   { {}, "Feedback" };

    juce::Slider wetDrySlider;
    juce::Label  wetDryLabel     { {}, "Wet/Dry" };

    //==============================================================================
    // MÓDULO: DSP - Synth (Generador de Audio)
    //==============================================================================
    juce::dsp::Oscillator<float> osc;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Gain<float> outputGain;
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;

    juce::dsp::ProcessSpec spec {};
    juce::SmoothedValue<float> velocityGain;

    // Estado del synth
    std::atomic<float> targetFrequencyHz { 440.0f };
    std::atomic<int> currentWaveform { 0 }; // 0: Sine, 1: Saw, 2: Square
    std::atomic<float> cutoffHz { 20000.0f };
    std::atomic<float> resonance { 0.7f };
    std::atomic<int> activeNote { -1 };

    //==============================================================================
    // MÓDULO: DSP - Delay Estéreo (Procesador de Audio)
    //==============================================================================
    // Buffers de delay independientes para cada canal
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int writePosL = 0;
    int writePosR = 0;
    int delaySamples = 22050;  // default (0.5s @ 44.1kHz)
    int maxDelaySamples = 0;
    double currentSampleRate = 44100.0;

    // Parámetros de delay
    float delayTimeMs = 400.0f;
    float feedback = 0.35f;
    float wetDryMix = 0.5f;  // 0.0 = solo dry, 1.0 = solo wet

    //==============================================================================
    // MÓDULO: Funciones Helper - Synth
    //==============================================================================
    void setWaveform (int index);
    void updateAdsrParamsFromUI();
    void updateFilterFromUI();
    void startNote (int midiNoteNumber, float velocity);
    void stopNote (int midiNoteNumber);

    //==============================================================================
    // MÓDULO: Funciones Helper - Delay
    //==============================================================================
    void prepareDelayState();
    void processDelayStereo (juce::AudioBuffer<float>& buffer);

    //==============================================================================
    // MÓDULO: Listeners
    //==============================================================================
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;
    void sliderValueChanged (juce::Slider* slider) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
