#include "MainComponent.h"

MainComponent::MainComponent()
{
    // ==== Controles ====
    playButton.onClick = [this] { playing = playButton.getToggleState(); };
    addAndMakeVisible (playButton);

    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.setValue (volume);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    volumeSlider.onValueChange = [this] { volume = (float) volumeSlider.getValue(); };
    addAndMakeVisible (volumeSlider);
    addAndMakeVisible (volumeLabel);

    lfoRateSlider.setRange (0.1, 20.0, 0.1);
    lfoRateSlider.setValue (lfoRate);
    lfoRateSlider.setTextValueSuffix (" Hz");
    lfoRateSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    lfoRateSlider.onValueChange = [this] { lfoRate = (float) lfoRateSlider.getValue(); };
    addAndMakeVisible (lfoRateSlider);
    addAndMakeVisible (lfoRateLabel);

    amplitudeDepthSlider.setRange (0.0, 1.0, 0.01);
    amplitudeDepthSlider.setValue (amplitudeDepth);
    amplitudeDepthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    amplitudeDepthSlider.onValueChange = [this] { amplitudeDepth = (float) amplitudeDepthSlider.getValue(); };
    addAndMakeVisible (amplitudeDepthSlider);
    addAndMakeVisible (amplitudeDepthLabel);

    frequencyDepthSlider.setRange (0.0, 100.0, 1.0);
    frequencyDepthSlider.setValue (frequencyDepth);
    frequencyDepthSlider.setTextValueSuffix (" Hz");
    frequencyDepthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    frequencyDepthSlider.onValueChange = [this] { frequencyDepth = (float) frequencyDepthSlider.getValue(); };
    addAndMakeVisible (frequencyDepthSlider);
    addAndMakeVisible (frequencyDepthLabel);

    setSize (600, 260);
    setAudioChannels (0, 2); // 0 entradas, 2 salidas (estéreo)
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

//==============================================================================
// Audio

void MainComponent::prepareToPlay (int, double sampleRate)
{
    sr = sampleRate;
    phase = 0.0f;
    lfoPhase = 0.0f;
}

//==============================================================================
// ==== Modulación con un LFO ====
//
// Un LFO (Low Frequency Oscillator) es un oscilador como el del ejemplo 01, pero lento
// (de 0.1 a 20 Hz): no lo escuchamos como sonido, sino que lo usamos para mover un parámetro.
//   https://en.wikipedia.org/wiki/Low-frequency_oscillation
//
// El LFO es un seno que va de -1 a 1. Con él movemos dos cosas de la onda cuadrada:
//
//   Amplitud (tremolo, AM): el volumen sube y baja.
//     amplitud = 1 - profundidad * (0.5 + 0.5 * lfo)
//     (0.5 + 0.5 * lfo) pasa el LFO de [-1, 1] a [0, 1]. Con profundidad 0 la amplitud queda
//     en 1 (sin modulación); con profundidad 1 va de 1 a 0.
//     https://en.wikipedia.org/wiki/Tremolo
//
//   Frecuencia (vibrato, FM): la altura sube y baja alrededor de la nota.
//     frecuencia = 220 + profundidad * lfo
//     Con profundidad 10 Hz, la frecuencia va de 210 a 230 Hz.
//     https://en.wikipedia.org/wiki/Vibrato
//
// Probá subir la velocidad del LFO por encima de 20 Hz (cambiando el rango del slider):
// el tremolo y el vibrato dejan de oírse como "movimiento" y aparecen tonos nuevos.
// Eso ya es síntesis AM y FM.

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* buffer = bufferToFill.buffer;

    // Leemos los parámetros una vez por bloque.
    float vol       = playing ? volume.load() : 0.0f; // apagado = silencio
    float rate      = lfoRate;
    float ampDepth  = amplitudeDepth;
    float freqDepth = frequencyDepth;

    for (int i = 0; i < bufferToFill.numSamples; ++i)
    {
        // ==== El LFO: un seno lento, de -1 a 1 (lo usan los pasos 1 y 3) ====
        float lfo = std::sin (juce::MathConstants<float>::twoPi * lfoPhase);

        lfoPhase += rate / (float) sr;
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        // ==== 1. Calcular la frecuencia (vibrato) ====
        float f = frequency + freqDepth * lfo;

        // ==== 2. Calcular la onda cuadrada (ver ejemplo 01) ====
        float sample = phase < 0.5f ? 1.0f : -1.0f;

        phase += f / (float) sr;
        if (phase >= 1.0f)
            phase -= 1.0f;

        // ==== 3. Calcular la ganancia (tremolo) ====
        float amplitude = 1.0f - ampDepth * (0.5f + 0.5f * lfo);

        // ==== 4. Escribir la muestra en todos los canales ====
        int index = bufferToFill.startSample + i;
        for (int channel = 0; channel < buffer->getNumChannels(); ++channel)
            buffer->setSample (channel, index, sample * amplitude * vol);
    }
}

void MainComponent::releaseResources()
{
}

//==============================================================================
// Interfaz gráfica

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (20);

    playButton.setBounds (area.removeFromTop (30).removeFromLeft (100));
    area.removeFromTop (10);

    juce::Slider* sliders[] = { &volumeSlider, &lfoRateSlider, &amplitudeDepthSlider, &frequencyDepthSlider };
    juce::Label*  labels[]  = { &volumeLabel,  &lfoRateLabel,  &amplitudeDepthLabel,  &frequencyDepthLabel };
    for (int i = 0; i < 4; ++i)
    {
        auto row = area.removeFromTop (30);
        labels[i]->setBounds (row.removeFromLeft (120));
        sliders[i]->setBounds (row);
        area.removeFromTop (10);
    }
}
