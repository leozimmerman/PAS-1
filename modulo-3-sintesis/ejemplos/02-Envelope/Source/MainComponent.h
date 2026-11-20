#pragma once

#include <JuceHeader.h>

// La ventana de la app. Es un AudioAppComponent: un componente gráfico que además produce audio.
// También es un Timer, para mostrar en qué etapa está la envolvente.
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
    // ---- Oscilador (la sierra del ejemplo 01) ----
    std::atomic<float> frequency { 220.0f }; // en Hz
    std::atomic<float> volume    { 0.2f };   // de 0 a 1 (std::atomic, ver ejemplo 02 del módulo 2)

    double sr = 44100.0;
    float phase = 0.0f;

    juce::Slider   frequencySlider;
    juce::Label    frequencyLabel { {}, "Frecuencia" };
    juce::Slider   volumeSlider;
    juce::Label    volumeLabel    { {}, "Volumen" };
    // ---- Fin del oscilador ----

    // ==== Envolvente ADSR ====

    // Parámetros (los cambia la interfaz, los usa el audio)
    std::atomic<float> attack  { 0.05f };  // segundos para subir de 0 a 1
    std::atomic<float> decay   { 0.2f };   // segundos para bajar de 1 al sustain
    std::atomic<float> sustain { 0.6f };   // nivel (de 0 a 1) mientras la nota sigue apretada
    std::atomic<float> release { 0.5f };   // segundos para bajar del nivel actual a 0

    std::atomic<bool> gate { false };      // true mientras el botón Nota está apretado

    // Las etapas de la envolvente. Un enum le pone nombre a unos números: Idle = 0, Attack = 1...
    enum Stage { Idle, Attack, Decay, Sustain, Release };

    // Estado de la envolvente (solo lo usa el hilo de audio)
    Stage stage = Idle;
    float envelope = 0.0f;     // el valor actual: multiplica a cada muestra
    float releaseStep = 0.0f;  // cuánto baja por muestra en el release
    bool  noteIsOn = false;    // para darnos cuenta cuándo cambia el gate

    float nextEnvelopeValue();

    // Para mostrar la envolvente (el audio escribe, el timer lee)
    std::atomic<int>   shownStage    { Idle };
    std::atomic<float> shownEnvelope { 0.0f };

    juce::TextButton noteButton { "Nota (mantener apretado)" };
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Label  attackLabel  { {}, "Attack" };
    juce::Label  decayLabel   { {}, "Decay" };
    juce::Label  sustainLabel { {}, "Sustain" };
    juce::Label  releaseLabel { {}, "Release" };
    juce::Label  stageLabel;

    juce::Rectangle<int> meterArea;   // dónde se dibuja la barra de la envolvente

    void setupSlider (juce::Slider& slider, juce::Label& label, double min, double max,
                      std::atomic<float>& parameter, const juce::String& suffix);
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
