#include "MainComponent.h"

//==============================================================================
// Utility: map MIDI note to Hz
static inline float midiToHz (int midiNote) noexcept
{
    return 440.0f * std::pow (2.0f, (midiNote - 69) / 12.0f);
}

//==============================================================================
MainComponent::MainComponent()
{
    setSize (1000, 700);
    setAudioChannels (0, 2);  // Sin entrada, salida estéreo

    // ============================================================================
    // MÓDULO: Configuración de UI - Synth
    // ============================================================================
    waveformLabel.setText ("Waveform", juce::dontSendNotification);
    addAndMakeVisible (waveformLabel);
    waveformBox.addItem ("Sine",   1);
    waveformBox.addItem ("Saw",    2);
    waveformBox.addItem ("Square", 3);
    waveformBox.setSelectedId (1, juce::dontSendNotification);
    waveformBox.addListener (this);
    addAndMakeVisible (waveformBox);

    attackLabel.setText ("A", juce::dontSendNotification);
    decayLabel.setText ("D", juce::dontSendNotification);
    sustainLabel.setText ("S", juce::dontSendNotification);
    releaseLabel.setText ("R", juce::dontSendNotification);
    addAndMakeVisible (attackLabel);
    addAndMakeVisible (decayLabel);
    addAndMakeVisible (sustainLabel);
    addAndMakeVisible (releaseLabel);

    attackSlider.setRange (0.001, 2.0, 0.0001);
    decaySlider.setRange  (0.001, 2.0, 0.0001);
    sustainSlider.setRange(0.0,   1.0, 0.0001);
    releaseSlider.setRange(0.001, 2.0, 0.0001);
    attackSlider.setValue (0.01);
    decaySlider.setValue  (0.2);
    sustainSlider.setValue(0.8);
    releaseSlider.setValue(0.3);
    for (auto* s : { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider })
    {
        s->addListener (this);
        addAndMakeVisible (*s);
    }

    cutoffLabel.setText ("Cutoff", juce::dontSendNotification);
    resonanceLabel.setText ("Reso", juce::dontSendNotification);
    addAndMakeVisible (cutoffLabel);
    addAndMakeVisible (resonanceLabel);

    cutoffSlider.setRange (20.0, 20000.0, 0.01);
    cutoffSlider.setSkewFactorFromMidPoint (1000.0);
    cutoffSlider.setValue (cutoffSlider.getMaximum(), juce::dontSendNotification);
    cutoffHz.store ((float) cutoffSlider.getMaximum());

    resonanceSlider.setRange (0.1, 2.0, 0.001);
    resonanceSlider.setValue (juce::jmax (resonance.load(), (float) resonanceSlider.getMinimum()));
    
    for (auto* s : { &cutoffSlider, &resonanceSlider })
    {
        s->addListener (this);
        addAndMakeVisible (*s);
    }

    addAndMakeVisible (keyboardComponent);
    keyboardState.addListener (this);

    // ============================================================================
    // MÓDULO: Configuración de UI - Delay
    // ============================================================================
    delayTimeSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    delayTimeSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    delayTimeSlider.setRange (1.0, 2000.0, 1.0);  // ms
    delayTimeSlider.setValue (delayTimeMs);
    delayTimeSlider.addListener (this);
    addAndMakeVisible (delayTimeSlider);

    delayTimeLabel.attachToComponent (&delayTimeSlider, false);
    delayTimeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (delayTimeLabel);

    feedbackSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    feedbackSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    feedbackSlider.setRange (0.0, 0.95, 0.001);
    feedbackSlider.setValue (feedback);
    feedbackSlider.addListener (this);
    addAndMakeVisible (feedbackSlider);

    feedbackLabel.attachToComponent (&feedbackSlider, false);
    feedbackLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (feedbackLabel);

    wetDrySlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    wetDrySlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    wetDrySlider.setRange (0.0, 1.0, 0.001);
    wetDrySlider.setValue (wetDryMix);
    wetDrySlider.addListener (this);
    addAndMakeVisible (wetDrySlider);

    wetDryLabel.attachToComponent (&wetDrySlider, false);
    wetDryLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (wetDryLabel);

    // DSP setup defaults
    setWaveform (0);
    outputGain.setGainLinear (0.2f); // prevent loudness
}

MainComponent::~MainComponent()
{
    keyboardState.removeListener (this);
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused (samplesPerBlockExpected);
    currentSampleRate = sampleRate;

    // ============================================================================
    // MÓDULO: Preparación del Synth
    // ============================================================================
    // Stereo spec for outputGain (matches device)
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlockExpected);
    spec.numChannels = 2;

    // Mono spec for osc and filter (we generate/process mono then duplicate to stereo)
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    osc.prepare (monoSpec);
    filter.reset();
    filter.prepare (monoSpec);
    outputGain.prepare (spec);
    velocityGain.reset (sampleRate, 0.02); // 20 ms smoothing

    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    updateAdsrParamsFromUI();
    adsr.setSampleRate (sampleRate);
    adsr.reset();

    updateFilterFromUI();
    osc.setFrequency (targetFrequencyHz.load());

    // ============================================================================
    // MÓDULO: Preparación del Delay Estéreo
    // ============================================================================
    prepareDelayState();
    
    // Asegurar que delaySamples coincide con delayTimeMs actual
    const int newDelaySamples = (int) std::round ((delayTimeMs * 0.001) * currentSampleRate);
    if (maxDelaySamples > 0)
        delaySamples = juce::jlimit (1, juce::jmax (1, maxDelaySamples - 1), newDelaySamples);
    else
        delaySamples = juce::jmax (1, newDelaySamples);
}

void MainComponent::prepareDelayState()
{
    // Máximo delay de 2 segundos
    const float maxDelaySeconds = 2.0f;
    maxDelaySamples = (int) std::ceil (maxDelaySeconds * currentSampleRate);

    // Inicializar buffers de delay para ambos canales
    delayBufferL.assign ((size_t) maxDelaySamples, 0.0f);
    delayBufferR.assign ((size_t) maxDelaySamples, 0.0f);
    writePosL = 0;
    writePosR = 0;

    // Asegurar que delaySamples está en rango válido
    delaySamples = juce::jlimit (1, juce::jmax (1, maxDelaySamples - 1), delaySamples);
}

void MainComponent::processDelayStereo (juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() <= 0 || delayBufferL.empty() || delayBufferR.empty())
        return;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const int delayBuffSize = (int) delayBufferL.size();

    // Procesar canal izquierdo (canal 0)
    if (numChannels > 0)
    {
        auto* dataL = buffer.getWritePointer (0);
        
        for (int i = 0; i < numSamples; ++i)
        {
            int readPosL = writePosL - delaySamples;
            if (readPosL < 0)
                readPosL += delayBuffSize;

            const float delayedL = delayBufferL[(size_t) readPosL];
            const float inL = dataL[i];

            // Escribir en buffer de delay: entrada + feedback del delay
            delayBufferL[(size_t) writePosL] = inL + feedback * delayedL;

            // Mezcla dry/wet
            const float dry = inL * (1.0f - wetDryMix);
            const float wet = delayedL * wetDryMix;
            dataL[i] = dry + wet;

            // Avanzar posición circular
            writePosL = (writePosL + 1) % delayBuffSize;
        }
    }

    // Procesar canal derecho (canal 1)
    if (numChannels > 1)
    {
        auto* dataR = buffer.getWritePointer (1);
        
        for (int i = 0; i < numSamples; ++i)
        {
            int readPosR = writePosR - delaySamples;
            if (readPosR < 0)
                readPosR += delayBuffSize;

            const float delayedR = delayBufferR[(size_t) readPosR];
            const float inR = dataR[i];

            // Escribir en buffer de delay: entrada + feedback del delay
            delayBufferR[(size_t) writePosR] = inR + feedback * delayedR;

            // Mezcla dry/wet
            const float dry = inR * (1.0f - wetDryMix);
            const float wet = delayedR * wetDryMix;
            dataR[i] = dry + wet;

            // Avanzar posición circular
            writePosR = (writePosR + 1) % delayBuffSize;
        }
    }
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* buffer = bufferToFill.buffer;
    if (buffer == nullptr)
        return;
    if (!osc.isInitialised())
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    auto numSamples = bufferToFill.numSamples;
    auto startSample = bufferToFill.startSample;

    // Limpiar buffer primero
    buffer->clear (startSample, numSamples);

    // ============================================================================
    // MÓDULO: Generación de Audio con Synth
    // ============================================================================
    // Preparar bloque temporal para generación mono
    juce::dsp::AudioBlock<float> audioBlock (*buffer);
    auto sub = audioBlock.getSubBlock ((size_t) startSample, (size_t) numSamples);

    // Crear bloque mono para síntesis, luego copiar a estéreo
    juce::HeapBlock<float> monoData;
    monoData.allocate ((size_t) numSamples, true);

    float* monoChans[] = { monoData.get() };
    juce::dsp::AudioBlock<float> monoBlock (monoChans, (size_t) 1, (size_t) numSamples);
    juce::dsp::ProcessContextReplacing<float> monoContext (monoBlock);

    // Generar oscilador
    osc.process (monoContext);

    // Aplicar ADSR al buffer mono
    {
        juce::AudioBuffer<float> monoAudioBuffer (monoChans, 1, (int) numSamples);
        adsr.applyEnvelopeToBuffer (monoAudioBuffer, 0, (int) numSamples);
    }

    // Actualizar parámetros del filtro
    filter.setCutoffFrequency (cutoffHz.load());
    filter.setResonance (juce::jmax (resonance.load(), (float) resonanceSlider.getMinimum()));

    // Procesar filtro (mono)
    filter.process (monoContext);
    
    // Aplicar ganancia de velocidad
    float* mono = monoBlock.getChannelPointer (0);
    float smoothVal = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        smoothVal = velocityGain.getNextValue();
        mono[i] *= smoothVal;
    }

    // Copiar mono a estéreo y aplicar ganancia de salida
    for (int ch = 0; ch < buffer->getNumChannels(); ++ch)
        buffer->copyFrom (ch, startSample, monoBlock.getChannelPointer (0), (int) monoBlock.getNumSamples());

    juce::dsp::ProcessContextReplacing<float> stereoContext (sub);
    outputGain.process (stereoContext);

    // ============================================================================
    // MÓDULO: Procesamiento con Delay Estéreo
    // ============================================================================
    processDelayStereo (*buffer);
}

void MainComponent::releaseResources()
{
    // Limpiar buffers de delay
    delayBufferL.clear();
    delayBufferR.clear();
    writePosL = 0;
    writePosR = 0;
    maxDelaySamples = 0;
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    // ============================================================================
    // MÓDULO: Layout - Synth Controls
    // ============================================================================
    // Top row: waveform
    {
        auto topRow = area.removeFromTop (36);
        const int gap = 10;

        auto wLabel = topRow.removeFromLeft (90);
        waveformLabel.setBounds (wLabel);

        auto wBox = topRow.removeFromLeft (200);
        waveformBox.setBounds (wBox);
    }

    area.removeFromTop (8);

    // ADSR row: 4 columns
    {
        auto adsrRow = area.removeFromTop (100);
        const int labelH = 18;
        const int gap = 6;

        auto colWidth = adsrRow.getWidth() / 4;

        auto layoutCol = [&] (juce::Rectangle<int> col, juce::Label& label, juce::Slider& slider)
        {
            auto labelArea = col.removeFromTop (labelH);
            label.setBounds (labelArea);
            col.removeFromTop (gap);
            slider.setBounds (col);
        };

        layoutCol (adsrRow.removeFromLeft (colWidth).reduced (4), attackLabel,  attackSlider);
        layoutCol (adsrRow.removeFromLeft (colWidth).reduced (4), decayLabel,   decaySlider);
        layoutCol (adsrRow.removeFromLeft (colWidth).reduced (4), sustainLabel, sustainSlider);
        layoutCol (adsrRow.removeFromLeft (colWidth).reduced (4), releaseLabel, releaseSlider);
    }

    area.removeFromTop (8);

    // Filter row: 2 columns
    {
        auto filterRow = area.removeFromTop (100);
        const int labelH = 18;
        const int gap = 6;

        auto colWidth = filterRow.getWidth() / 2;

        auto layoutCol = [&] (juce::Rectangle<int> col, juce::Label& label, juce::Slider& slider)
        {
            auto labelArea = col.removeFromTop (labelH);
            label.setBounds (labelArea);
            col.removeFromTop (gap);
            slider.setBounds (col);
        };

        layoutCol (filterRow.removeFromLeft (colWidth).reduced (4), cutoffLabel,    cutoffSlider);
        layoutCol (filterRow.removeFromLeft (colWidth).reduced (4), resonanceLabel, resonanceSlider);
    }

    area.removeFromTop (8);

    // ============================================================================
    // MÓDULO: Layout - Delay Controls
    // ============================================================================
    // Delay row: 3 rotary knobs
    {
        auto delayRow = area.removeFromTop (200);
        auto numKnobs = 3;
        auto knobWidth = delayRow.getWidth() / numKnobs;

        auto placeKnob = [] (juce::Component& c, juce::Rectangle<int> r)
        {
            c.setBounds (r.reduced (10));
        };

        auto delayCol1 = delayRow.removeFromLeft (knobWidth);
        placeKnob (delayTimeSlider, delayCol1);

        auto delayCol2 = delayRow.removeFromLeft (knobWidth);
        placeKnob (feedbackSlider, delayCol2);

        auto delayCol3 = delayRow.removeFromLeft (knobWidth);
        placeKnob (wetDrySlider, delayCol3);
    }

    area.removeFromTop (8);

    // Keyboard at bottom
    keyboardComponent.setBounds (area);
}

//==============================================================================
void MainComponent::handleNoteOn (juce::MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float velocity)
{
    startNote (midiNoteNumber, velocity);
}

void MainComponent::handleNoteOff (juce::MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float /*velocity*/)
{
    stopNote (midiNoteNumber);
}

//==============================================================================
// UI listeners
void MainComponent::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &waveformBox)
    {
        const int idx = waveformBox.getSelectedId() - 1; // 0-based
        currentWaveform.store (juce::jlimit (0, 2, idx));
        setWaveform (currentWaveform.load());
    }
}

void MainComponent::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &attackSlider || slider == &decaySlider || slider == &sustainSlider || slider == &releaseSlider)
    {
        updateAdsrParamsFromUI();
    }
    else if (slider == &cutoffSlider || slider == &resonanceSlider)
    {
        updateFilterFromUI();
    }
    else if (slider == &delayTimeSlider)
    {
        delayTimeMs = (float) delayTimeSlider.getValue();
        const int newDelaySamples = (int) std::round ((delayTimeMs * 0.001) * currentSampleRate);
        
        if (maxDelaySamples > 0)
            delaySamples = juce::jlimit (1, juce::jmax (1, maxDelaySamples - 1), newDelaySamples);
        else
            delaySamples = juce::jmax (1, newDelaySamples);
    }
    else if (slider == &feedbackSlider)
    {
        feedback = (float) feedbackSlider.getValue();
    }
    else if (slider == &wetDrySlider)
    {
        wetDryMix = (float) wetDrySlider.getValue();
    }
}

//==============================================================================
// Internal helpers
void MainComponent::setWaveform (int index)
{
    switch (index)
    {
        case 0: // Sine
            osc.initialise ([] (float x) { return std::sin (x); }, 128);
            break;
        case 1: // Saw
            osc.initialise ([] (float x) {
                const float v = juce::jmap (x, -juce::MathConstants<float>::pi, juce::MathConstants<float>::pi, -1.0f, 1.0f);
                return v;
            }, 128);
            break;
        case 2: // Square
            osc.initialise ([] (float x) { return x < 0.0f ? -1.0f : 1.0f; }, 128);
            break;
        default:
            osc.initialise ([] (float x) { return std::sin (x); }, 128);
            break;
    }
}

void MainComponent::updateAdsrParamsFromUI()
{
    adsrParams.attack  = (float) attackSlider.getValue();
    adsrParams.decay   = (float) decaySlider.getValue();
    adsrParams.sustain = (float) sustainSlider.getValue();
    adsrParams.release = (float) releaseSlider.getValue();
    adsr.setParameters (adsrParams);
}

void MainComponent::updateFilterFromUI()
{
    cutoffHz.store ((float) cutoffSlider.getValue());
    const float minReso = (float) resonanceSlider.getMinimum();
    const float reso = (float) resonanceSlider.getValue();
    resonance.store (juce::jmax (reso, minReso));
}

void MainComponent::startNote (int midiNoteNumber, float velocity)
{
    const float freq = midiToHz (midiNoteNumber);
    targetFrequencyHz.store (freq);
    osc.setFrequency (freq);
    
    const bool hadActive = (activeNote.load() != -1);
    if (! hadActive) {
        adsr.noteOn();
    }
    
    activeNote.store (midiNoteNumber);
    velocityGain.setTargetValue (velocity);
}

void MainComponent::stopNote (int midiNoteNumber)
{
    if (activeNote.load() == midiNoteNumber)
    {
        adsr.noteOff();
        activeNote.store (-1);
    }
}
