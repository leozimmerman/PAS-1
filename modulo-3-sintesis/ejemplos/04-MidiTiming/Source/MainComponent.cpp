#include "MainComponent.h"

MainComponent::MainComponent()
{
    // ---- Oscilador ----
    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.setValue (volume);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    volumeSlider.onValueChange = [this] { volume = (float) volumeSlider.getValue(); };
    addAndMakeVisible (volumeSlider);
    addAndMakeVisible (volumeLabel);

    // ---- Envolvente ----
    setupSlider (attackSlider,  attackLabel,  0.001, 2.0, attack,  " s");
    setupSlider (decaySlider,   decayLabel,   0.001, 2.0, decay,   " s");
    setupSlider (sustainSlider, sustainLabel, 0.0,   1.0, sustain, "");
    setupSlider (releaseSlider, releaseLabel, 0.001, 4.0, release, " s");

    // ==== MIDI ====
    addAndMakeVisible (keyboardComponent);
    addAndMakeVisible (midiLabel);
    midiLabel.setText ("Teclado: clic con el mouse o teclas A S D F...", juce::dontSendNotification);

    setSize (700, 440);
    setAudioChannels (0, 2); // 0 entradas, 2 salidas (estéreo)

    // Entrada MIDI de hardware: habilitamos todos los dispositivos MIDI conectados y le pedimos
    // al deviceManager que mande sus mensajes al collector ("" = de todos los dispositivos).
    // Va después de setAudioChannels, que llama a prepareToPlay (ahí el collector conoce el sample rate).
    // Si conectás un teclado con la app abierta, hay que volver a abrirla.
    for (auto& input : juce::MidiInput::getAvailableDevices())
        deviceManager.setMidiInputDeviceEnabled (input.identifier, true);

    deviceManager.addMidiInputDeviceCallback ("", &midiCollector);

    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    deviceManager.removeMidiInputDeviceCallback ("", &midiCollector);
    shutdownAudio();
}

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
    midiCollector.reset (sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* buffer = bufferToFill.buffer;

    // ==== 1. Juntar los mensajes MIDI de este bloque ====
    juce::MidiBuffer midi;

    // Los del teclado de hardware...
    midiCollector.removeNextBlockOfMessages (midi, bufferToFill.numSamples);

    // ...y los del teclado en pantalla (el "true" los agrega a midi; además, las notas
    // que llegan del hardware se marcan en el teclado de la pantalla).
    keyboardState.processNextMidiBuffer (midi, bufferToFill.startSample, bufferToFill.numSamples, true);

    // ==== 2. Sintetizar, atendiendo cada mensaje en su muestra ====
    // Partimos el bloque en tramos: calculamos el audio hasta el próximo mensaje, lo atendemos
    // y seguimos (ver "Timing de los mensajes MIDI", más abajo).
    // Los mensajes de un MidiBuffer vienen ordenados por samplePosition.
    int position = 0;   // hasta qué muestra del bloque ya calculamos

    for (const auto metadata : midi)
    {
        int messagePosition = metadata.samplePosition;

        // El tramo desde donde íbamos hasta el mensaje...
        renderSamples (*buffer, bufferToFill.startSample + position, messagePosition - position);

        // ...el mensaje (note on / note off), justo en su muestra...
        handleMidiMessage (metadata.getMessage());

        position = messagePosition;
    }

    // ...y el último tramo, desde el último mensaje hasta el final del bloque.
    renderSamples (*buffer, bufferToFill.startSample + position, bufferToFill.numSamples - position);
}

//==============================================================================
// ==== Timing de los mensajes MIDI ====
//
// Cada mensaje del bloque trae en qué muestra ocurre (samplePosition). Hay tres maneras
// de atenderlos:
//
//   1. Todos al principio del bloque (ejemplo 03): lo más simple, pero cada nota arranca en
//      la muestra 0 aunque haya llegado a mitad del bloque. Con un buffer de 512 a 48 kHz el
//      error es de hasta 10 ms, distinto en cada nota: el ritmo de un arpegio queda irregular.
//
//   2. Revisar todos los mensajes en cada muestra: exacto, pero recorre la lista de mensajes
//      512 veces por bloque, aunque no haya ninguno.
//
//   3. Partir el bloque en tramos (la que usamos): calcular el audio hasta el próximo mensaje,
//      atenderlo y seguir. Es exacto y cada mensaje se mira una sola vez.
//
//        bloque:  |-------------x--------------------x----------|
//                 0            200                  430        512
//        tramos:  [   0..199   ][      200..429      ][ 430..511 ]
//                               ^ note on            ^ note off
//
//      juce::Synthesiser hace esto mismo por dentro, y además reparte las notas entre
//      varias voces para tocar acordes (polifonía).
//        https://docs.juce.com/master/classSynthesiser.html
//
// Ninguna de las tres saca la latencia del buffer: el bloque se calcula entero y recién
// después suena. Con 512 muestras a 48 kHz, todo suena unos 10 ms tarde. Esa latencia se
// achica bajando el tamaño del buffer en la configuración de audio.

void MainComponent::renderSamples (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    float vol = volume;
    float f   = frequency;   // dentro de un tramo no cambia: los mensajes están entre tramos

    for (int i = 0; i < numSamples; ++i)
    {
        // Seno (ver ejemplo 01): sin (2 * pi * fase)
        float sample = std::sin (juce::MathConstants<float>::twoPi * phase);

        // La envolvente y la velocity controlan el volumen de cada muestra.
        sample = sample * nextEnvelopeValue() * velocityGain;

        int index = startSample + i;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample (channel, index, sample * vol);

        phase += f / (float) sr;
        if (phase >= 1.0f)
            phase -= 1.0f;
    }
}

void MainComponent::releaseResources()
{
}

//==============================================================================
// ==== MIDI ====
//
// Un mensaje de nota trae tres datos:
//   - si es note on (se apretó una tecla) o note off (se soltó);
//   - el número de nota, de 0 a 127 (60 = Do central, 69 = La 440);
//   - la velocity, de 0 a 127: qué tan fuerte se apretó la tecla.
//
// Es un sinte monofónico: suena una sola nota a la vez. Si apretás otra, la nueva reemplaza
// a la anterior. Para tocar acordes haría falta una voz por nota (juce::Synthesiser).

void MainComponent::handleMidiMessage (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
    {
        currentNote = message.getNoteNumber();

        // Número de nota -> frecuencia:  f = 440 * 2^((nota - 69) / 12)
        // Cada 12 notas (una octava) la frecuencia se duplica; la nota 69 es el La de 440 Hz.
        //   https://en.wikipedia.org/wiki/MIDI_tuning_standard
        frequency = 440.0f * std::pow (2.0f, (currentNote - 69) / 12.0f);

        // Velocity -> amplitud: de 0..127 a 0..1
        velocityGain = message.getVelocity() / 127.0f;

        stage = Attack;  // la envolvente arranca de nuevo (desde el valor que tenga)
    }
    else if (message.isNoteOff())
    {
        // Solo soltamos si es la nota que está sonando (si no, soltar una tecla vieja cortaría la nueva).
        if (message.getNoteNumber() == currentNote)
        {
            stage = Release;
            releaseStep = envelope / (release * (float) sr);
            currentNote = -1;
        }
    }
    else
    {
        return; // otros mensajes (pedal, pitch bend, controles...) no los usamos
    }

    lastWasNoteOn = message.isNoteOn();
    lastNote      = message.getNoteNumber();
    lastVelocity  = message.getVelocity();
}

//==============================================================================
// ---- Envolvente ADSR (la del ejemplo 02) ----

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
        envelope = sustain;
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

//==============================================================================
// Interfaz gráfica

void MainComponent::timerCallback()
{
    int note = lastNote;
    if (note < 0)
        return; // todavía no llegó ninguna nota

    // getMidiNoteName da el nombre de la nota: 60 -> "C3" (en JUCE el Do central es C3)
    midiLabel.setText (juce::String (lastWasNoteOn ? "Note on " : "Note off")
                           + "   nota " + juce::String (note)
                           + " (" + juce::MidiMessage::getMidiNoteName (note, true, true, 3) + ")"
                           + "   velocity " + juce::String (lastVelocity.load()),
                       juce::dontSendNotification);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (20);

    auto volumeRow = area.removeFromTop (30);
    volumeLabel.setBounds (volumeRow.removeFromLeft (100));
    volumeSlider.setBounds (volumeRow);
    area.removeFromTop (20);

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

    // ==== MIDI ====
    midiLabel.setBounds (area.removeFromTop (25));
    area.removeFromTop (5);
    keyboardComponent.setBounds (area.removeFromTop (100));
}
