#pragma once

#include <JuceHeader.h>

// La ventana de la app. Es un AudioAppComponent: un componente gráfico que además produce audio.
// Es el oscilador del ejemplo 01 (solo la onda cuadrada, en una nota fija) con un LFO
// que modula la amplitud y la frecuencia.
class MainComponent : public juce::AudioAppComponent
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
    // ==== Parámetros ====
    // Los cambia la interfaz y los usa el audio (std::atomic, ver ejemplo 02 del módulo 2).
    std::atomic<bool>  playing        { false };  // el botón Sonar
    std::atomic<float> volume         { 0.2f };   // de 0 a 1 (arranca bajo para cuidar los oídos)
    std::atomic<float> lfoRate        { 4.0f };   // velocidad del LFO, en Hz
    std::atomic<float> amplitudeDepth { 0.0f };   // cuánto mueve la amplitud: de 0 a 1
    std::atomic<float> frequencyDepth { 0.0f };   // cuánto mueve la frecuencia: en Hz

    // ==== Oscilador ====
    const float frequency = 220.0f;   // la nota fija (La 220)
    double sr = 44100.0;              // sample rate: lo sabemos recién en prepareToPlay
    float phase = 0.0f;               // fase del oscilador: de 0 a 1

    // ==== LFO ====
    float lfoPhase = 0.0f;            // fase del LFO: de 0 a 1

    // ==== Interfaz ====
    juce::ToggleButton playButton { "Sonar" };
    juce::Slider volumeSlider, lfoRateSlider, amplitudeDepthSlider, frequencyDepthSlider;
    juce::Label  volumeLabel         { {}, "Volumen" };
    juce::Label  lfoRateLabel        { {}, "Velocidad LFO" };
    juce::Label  amplitudeDepthLabel { {}, "Prof. amplitud" };
    juce::Label  frequencyDepthLabel { {}, "Prof. frecuencia" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
