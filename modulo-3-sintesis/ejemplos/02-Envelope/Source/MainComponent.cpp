#include "MainComponent.h"

MainComponent::MainComponent()
{
    // ---- Oscilador ----
    frequencySlider.setRange (20.0, 10000.0, 1.0);
    frequencySlider.setSkewFactorFromMidPoint (440.0);
    frequencySlider.setValue (frequency);
    frequencySlider.setTextValueSuffix (" Hz");
    frequencySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    frequencySlider.onValueChange = [this] { frequency = (float) frequencySlider.getValue(); };
    addAndMakeVisible (frequencySlider);
    addAndMakeVisible (frequencyLabel);

    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.setValue (volume);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    volumeSlider.onValueChange = [this] { volume = (float) volumeSlider.getValue(); };
    addAndMakeVisible (volumeSlider);
    addAndMakeVisible (volumeLabel);

    // ==== Envolvente ADSR ====

    // El botón Nota funciona como la tecla de un teclado: suena mientras está apretado.
    // onStateChange se llama al apretarlo y al soltarlo; isDown() dice si está apretado.
    noteButton.onStateChange = [this] { gate = noteButton.isDown(); };
    addAndMakeVisible (noteButton);

    // Los tiempos arrancan en 0.001 s (y no en 0) para no dividir por cero.
    setupSlider (attackSlider,  attackLabel,  0.001, 2.0, attack,  " s");
    setupSlider (decaySlider,   decayLabel,   0.001, 2.0, decay,   " s");
    setupSlider (sustainSlider, sustainLabel, 0.0,   1.0, sustain, "");
    setupSlider (releaseSlider, releaseLabel, 0.001, 4.0, release, " s");

    addAndMakeVisible (stageLabel);

    setSize (600, 440);
    setAudioChannels (0, 2); // 0 entradas, 2 salidas (estéreo)
    startTimerHz (30);       // 30 veces por segundo: actualizar la etapa y la barra
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

// Configura un slider de la envolvente: rango, valor inicial y qué parámetro cambia.
void MainComponent::setupSlider (juce::Slider& slider, juce::Label& label, double min, double max,
                                 std::atomic<float>& parameter, const juce::String& suffix)
{
    slider.setRange (min, max, 0.001);
    slider.setValue (parameter);
    slider.setTextValueSuffix (suffix);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    slider.onValueChange = [&slider, &parameter] { parameter = (float) slider.getValue(); };
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

//==============================================================================
// Audio

void MainComponent::prepareToPlay (int, double sampleRate)
{
    sr = sampleRate;
    phase = 0.0f;
    stage = Idle;
    envelope = 0.0f;
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* buffer = bufferToFill.buffer;

    float f   = frequency;
    float vol = volume;

    // ==== ¿Cambió el gate? ====
    // Lo revisamos una vez por bloque: al apretar empieza el attack, al soltar empieza el release.
    bool g = gate;
    if (g && ! noteIsOn)
    {
        stage = Attack;           // arranca desde el valor que tenga (si estaba sonando, no salta a 0)
        noteIsOn = true;
    }
    else if (! g && noteIsOn)
    {
        stage = Release;
        releaseStep = envelope / (release * (float) sr);   // baja desde donde esté hasta 0
        noteIsOn = false;
    }

    for (int i = 0; i < bufferToFill.numSamples; ++i)
    {
        // Sierra (ver ejemplo 01): sube en línea recta de -1 a 1 en cada ciclo.
        float sample = 2.0f * phase - 1.0f;

        // La envolvente controla el volumen de cada muestra.
        sample = sample * nextEnvelopeValue();

        int index = bufferToFill.startSample + i;
        for (int channel = 0; channel < buffer->getNumChannels(); ++channel)
            buffer->setSample (channel, index, sample * vol);

        phase += f / (float) sr;
        if (phase >= 1.0f)
            phase -= 1.0f;
    }

    shownStage = stage;
    shownEnvelope = envelope;
}

void MainComponent::releaseResources()
{
}

//==============================================================================
// ==== Envolvente ADSR ====
//
// La envolvente es un número entre 0 y 1 que multiplica a cada muestra. Pasa por cinco etapas:
//
//   1 |    /\
//     |   /  \
//   S |  /    \___________
//     | /                 \
//   0 |/                   \_______
//     |-A--|-D-|---- S ----|--R--|
//     ^ apretás            ^ soltás
//
//   Idle:    no suena (0).
//   Attack:  sube en línea recta hasta 1, en "attack" segundos.
//   Decay:   baja hasta el nivel de sustain, en "decay" segundos.
//   Sustain: se queda en el nivel de sustain mientras la nota siga apretada.
//   Release: al soltar, baja hasta 0 en "release" segundos.
//   https://en.wikipedia.org/wiki/Envelope_(music)
//
// Es una "máquina de estados": en cada muestra hacemos lo que corresponde a la etapa actual
// y, cuando termina, pasamos a la siguiente.
//
// Para subir de 0 a 1 en "attack" segundos hacen falta attack * sr muestras,
// así que en cada muestra sube 1 / (attack * sr).

float MainComponent::nextEnvelopeValue()
{
    if (stage == Attack)
    {
        envelope += 1.0f / (attack * (float) sr);
        if (envelope >= 1.0f)
        {
            envelope = 1.0f;
            stage = Decay;
        }
    }
    else if (stage == Decay)
    {
        envelope -= (1.0f - sustain) / (decay * (float) sr);
        if (envelope <= sustain)
        {
            envelope = sustain;
            stage = Sustain;
        }
    }
    else if (stage == Sustain)
    {
        envelope = sustain;   // si movés el slider mientras suena, sigue al slider
    }
    else if (stage == Release)
    {
        envelope -= releaseStep;
        if (envelope <= 0.0f)
        {
            envelope = 0.0f;
            stage = Idle;
        }
    }
    else // Idle
    {
        envelope = 0.0f;
    }

    return envelope;
}

// JUCE trae una envolvente lista, juce::ADSR, que hace lo mismo (con tiempos en segundos y
// sustain de 0 a 1): noteOn() al apretar, noteOff() al soltar y getNextSample() en cada muestra.
//   https://docs.juce.com/master/classADSR.html

//==============================================================================
// Interfaz gráfica

void MainComponent::timerCallback()
{
    const char* names[] = { "Idle", "Attack", "Decay", "Sustain", "Release" };
    float value = shownEnvelope;
    stageLabel.setText (juce::String ("Etapa: ") + names[shownStage.load()]
                            + "   Envolvente: " + juce::String (value, 2),
                        juce::dontSendNotification);
    repaint (meterArea);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // Barra de la envolvente: el ancho es proporcional al valor (de 0 a 1)
    g.setColour (juce::Colours::darkgrey);
    g.fillRect (meterArea);
    g.setColour (juce::Colours::orange);
    g.fillRect (meterArea.withWidth ((int) (meterArea.getWidth() * shownEnvelope.load())));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (20);

    auto frequencyRow = area.removeFromTop (30);
    frequencyLabel.setBounds (frequencyRow.removeFromLeft (100));
    frequencySlider.setBounds (frequencyRow);
    area.removeFromTop (10);

    auto volumeRow = area.removeFromTop (30);
    volumeLabel.setBounds (volumeRow.removeFromLeft (100));
    volumeSlider.setBounds (volumeRow);
    area.removeFromTop (20);

    // ==== Envolvente ====
    juce::Slider* sliders[] = { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider };
    juce::Label*  labels[]  = { &attackLabel,  &decayLabel,  &sustainLabel,  &releaseLabel };
    for (int i = 0; i < 4; ++i)
    {
        auto row = area.removeFromTop (30);
        labels[i]->setBounds (row.removeFromLeft (100));
        sliders[i]->setBounds (row);
        area.removeFromTop (5);
    }
    area.removeFromTop (10);

    noteButton.setBounds (area.removeFromTop (40));
    area.removeFromTop (10);
    stageLabel.setBounds (area.removeFromTop (25));
    area.removeFromTop (5);
    meterArea = area.removeFromTop (20);
}
