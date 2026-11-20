#pragma once

#include <JuceHeader.h>

// La ventana de la app. Es un AudioAppComponent: un componente gráfico que además produce audio.
// También es un Timer, para mostrar el último mensaje MIDI.
class MainComponent : public juce::AudioAppComponent,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    // Audio (se llaman desde el hilo de audio)
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // Interfaz gráfica (se llaman desde el hilo de la interfaz)
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    // ---- Oscilador (el seno del ejemplo 01) ----
    std::atomic<float> volume   { 0.2f }; // de 0 a 1 (std::atomic, ver ejemplo 02 del módulo 2)

    double sr = 44100.0;
    float phase = 0.0f;
    float frequency = 440.0f;   // ahora la elige la nota MIDI (solo la usa el hilo de audio)

    juce::Slider   volumeSlider;
    juce::Label    volumeLabel   { {}, "Volumen" };
    // ---- Fin del oscilador ----

    // ---- Envolvente ADSR (la del ejemplo 02) ----
    std::atomic<float> attack  { 0.01f };
    std::atomic<float> decay   { 0.2f };
    std::atomic<float> sustain { 0.6f };
    std::atomic<float> release { 0.3f };

    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float envelope = 0.0f;
    float releaseStep = 0.0f;

    float nextEnvelopeValue();

    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Label  attackLabel  { {}, "Attack" };
    juce::Label  decayLabel   { {}, "Decay" };
    juce::Label  sustainLabel { {}, "Sustain" };
    juce::Label  releaseLabel { {}, "Release" };

    void setupSlider (juce::Slider& slider, juce::Label& label, double min, double max,
                      std::atomic<float>& parameter, const juce::String& suffix);
    // ---- Fin de la envolvente ----

    // ==== MIDI ====

    // Teclado en pantalla: keyboardState guarda qué teclas están apretadas
    // y keyboardComponent las dibuja y deja tocarlas con el mouse (o con las teclas A S D F...).
    juce::MidiKeyboardState     keyboardState;
    juce::MidiKeyboardComponent keyboardComponent { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    // Los mensajes de un teclado MIDI de verdad llegan por otro hilo, en cualquier momento.
    // El collector los junta y el hilo de audio los retira una vez por bloque.
    juce::MidiMessageCollector midiCollector;

    // Estado de la voz (solo lo usa el hilo de audio)
    int   currentNote = -1;     // la nota que está sonando (-1 = ninguna)
    float velocityGain = 0.0f;  // la velocity pasada a un número de 0 a 1

    void handleMidiMessage (const juce::MidiMessage& message);

    // Calcula numSamples muestras del sinte, a partir de startSample.
    void renderSamples (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    // Último mensaje, para mostrarlo (el audio escribe, el timer lee)
    std::atomic<bool> lastWasNoteOn { false };
    std::atomic<int>  lastNote      { -1 };
    std::atomic<int>  lastVelocity  { 0 };
    juce::Label midiLabel;

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
