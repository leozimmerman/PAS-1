#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Utility: map MIDI note to Hz
float SynthPluginProcessor::midiToHz (int midiNote) noexcept
{
    return 440.0f * std::pow (2.0f, (midiNote - 69) / 12.0f);
}

//==============================================================================
SynthPluginProcessor::SynthPluginProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#endif
{
}

SynthPluginProcessor::~SynthPluginProcessor() = default;

//==============================================================================
void SynthPluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    osc.prepare (monoSpec);
    filter.reset();
    filter.prepare (monoSpec);
    outputGain.prepare (spec);

    // Smooth para velocity (20 ms)
    velocityGain.reset (sampleRate, 0.02);

    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    adsr.setSampleRate (sampleRate);
    adsr.reset();

    // Defaults similares a tu MainComponent
    outputGain.setGainLinear (0.2f);
    setWaveform (currentWaveform.load());
    setAdsr (0.01f, 0.2f, 0.8f, 0.3f);
    setFilter (cutoffHz.load(), resonance.load());

    // Frecuencia inicial
    osc.setFrequency (targetFrequencyHz.load());
}

void SynthPluginProcessor::releaseResources()
{
}

//==============================================================================
#if ! JucePlugin_PreferredChannelConfigurations
bool SynthPluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Solo mono o stereo, sin sidechain ni otras rarezas
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Si hay input, que tenga el mismo layout que el output
    if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;

    return true;
}
#endif

//==============================================================================
void SynthPluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    const int numSamples = buffer.getNumSamples();

    // Limpiar canales que sobren
    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    // Mezclar el MIDI del teclado on-screen con el MIDI del host
    keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);

    // Parsear MIDI para notas
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
            startNote (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            stopNote (msg.getNoteNumber());
    }

    // Generar audio (similar a tu getNextAudioBlock)
    buffer.clear();

    juce::dsp::AudioBlock<float> audioBlock (buffer);
    auto sub = audioBlock; // bloque completo

    // Mono temp buffer
    juce::HeapBlock<float> monoData;
    monoData.allocate ((size_t) numSamples, true);

    float* monoChans[] = { monoData.get() };
    juce::dsp::AudioBlock<float> monoBlock (monoChans, (size_t) 1, (size_t) numSamples);
    juce::dsp::ProcessContextReplacing<float> monoContext (monoBlock);

    // Osc
    osc.process (monoContext);

    // ADSR sobre la señal mono
    {
        juce::AudioBuffer<float> monoAudioBuffer (monoChans, 1, numSamples);
        adsr.applyEnvelopeToBuffer (monoAudioBuffer, 0, numSamples);
    }

    // Filtro
    updateFilterFromAtomics();
    filter.process (monoContext);

    // Velocity smoothing per-sample
    float* mono = monoBlock.getChannelPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        const float smoothVal = velocityGain.getNextValue();
        mono[i] *= smoothVal;
    }

    // Copiar mono a todos los canales de salida
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        buffer.copyFrom (ch, 0, mono, numSamples);

    // Ganancia de salida
    juce::dsp::ProcessContextReplacing<float> stereoContext (sub);
    outputGain.process (stereoContext);
}

//==============================================================================
// Comunicación con el editor

void SynthPluginProcessor::setWaveform (int index)
{
    currentWaveform.store (juce::jlimit (0, 2, index));

    switch (currentWaveform.load())
    {
        case 0: // Sine
            osc.initialise ([] (float x) { return std::sin (x); }, 128);
            break;
        case 1: // Saw
            osc.initialise ([] (float x)
            {
                const float v = juce::jmap (x,
                                            -juce::MathConstants<float>::pi,
                                             juce::MathConstants<float>::pi,
                                            -1.0f, 1.0f);
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

void SynthPluginProcessor::setAdsr (float attack, float decay,
                                         float sustain, float release)
{
    adsrParams.attack  = attack;
    adsrParams.decay   = decay;
    adsrParams.sustain = sustain;
    adsrParams.release = release;
    adsr.setParameters (adsrParams);
}

void SynthPluginProcessor::setFilter (float cutoff, float reso)
{
    cutoffHz.store (cutoff);
    const float minReso = 0.1f;
    resonance.store (juce::jmax (reso, minReso));

    updateFilterFromAtomics();
}

void SynthPluginProcessor::updateFilterFromAtomics()
{
    filter.setCutoffFrequency (cutoffHz.load());
    filter.setResonance (resonance.load());
}

//==============================================================================
// Nota on/off (portado de tu MainComponent)

void SynthPluginProcessor::startNote (int midiNoteNumber, float velocity)
{
    const float freq = midiToHz (midiNoteNumber);
    targetFrequencyHz.store (freq);
    osc.setFrequency (freq);

    const bool hadActive = (activeNote.load() != -1);
    if (! hadActive)
        adsr.noteOn();

    activeNote.store (midiNoteNumber);
    velocityGain.setTargetValue (velocity);
}

void SynthPluginProcessor::stopNote (int midiNoteNumber)
{
    if (activeNote.load() == midiNoteNumber)
    {
        adsr.noteOff();
        activeNote.store (-1);
    }
}

//==============================================================================
// Editor

juce::AudioProcessorEditor* SynthPluginProcessor::createEditor()
{
    return new SynthPluginProcessorEditor (*this);
}

//==============================================================================
// State (por ahora vacío, pero preparado para guardar parámetros)

void SynthPluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
    // TODO: guardar parámetros si los pasás a APVTS
}

void SynthPluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
    // TODO: restaurar parámetros
}
//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthPluginProcessor();
}
