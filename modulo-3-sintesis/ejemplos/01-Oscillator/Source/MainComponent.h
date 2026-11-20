#pragma once

#include <JuceHeader.h>

// La ventana de la app. Es un AudioAppComponent: un componente gráfico que además produce audio.
// Acá no hay archivo: cada muestra la calculamos nosotros (síntesis).
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
    // ==== Opciones ====
    // Un enum le pone nombre a cada opción: es más claro escribir  saw  que  1.
    // Están en el mismo orden que en los ComboBox.
    enum Waveform { sine, saw, square, triangle, noise };
    enum Mode     { formula, table };

    // ==== Parámetros ====
    // Los cambia la interfaz y los usa el audio (std::atomic, ver ejemplo 02 del módulo 2).
    std::atomic<bool>  playing     { false };   // el botón Sonar
    std::atomic<int>   waveform    { sine };
    std::atomic<int>   mode        { formula };
    std::atomic<bool>  interpolate { true };    // solo en modo Tabla
    std::atomic<float> frequency   { 220.0f };  // en Hz
    std::atomic<float> volume      { 0.2f };    // de 0 a 1 (arranca bajo para cuidar los oídos)

    // ==== Oscilador ====
    double sr = 44100.0;   // sample rate: lo sabemos recién en prepareToPlay
    float phase = 0.0f;    // en qué parte del ciclo estamos: de 0 a 1

    // ==== 1. Fórmula: calcular cada muestra ====
    float formulaSample (int shape, float phaseValue);
    juce::Random random;   // para el ruido

    // ==== 2. Tabla: leer muestras ya calculadas ====
    // Un ciclo de cada forma, calculado una sola vez (en el constructor) y guardado en memoria.
    // Probá cambiar tableSize a 16 o 64 y comparar con y sin interpolación.
    static const int tableSize = 2048;
    std::vector<float> sineTable, sawTable, squareTable, triangleTable;

    void  fillTable (std::vector<float>& tableToFill, int shape);
    float tableSample (int shape, float phaseValue);
    float readTable (const std::vector<float>& tableToRead, float phaseValue);

    // ==== Interfaz ====
    juce::ToggleButton playButton        { "Sonar" };
    juce::ComboBox     waveformBox;
    juce::Label        waveformLabel     { {}, "Forma" };
    juce::ComboBox     modeBox;
    juce::Label        modeLabel         { {}, "Muestras" };
    juce::ToggleButton interpolateButton { "Interpolar" };
    juce::Slider       frequencySlider;
    juce::Label        frequencyLabel    { {}, "Frecuencia" };
    juce::Slider       volumeSlider;
    juce::Label        volumeLabel       { {}, "Volumen" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
