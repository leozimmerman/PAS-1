#include "PluginProcessor.h"
#include "PluginEditor.h"

float SynthPluginProcessor::midiToHz (int midiNote) noexcept
{
    return 440.0f * std::pow (2.0f, (midiNote - 69) / 12.0f);
}

//==============================================================================
SynthPluginProcessor::SynthPluginProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    : AudioProcessor (BusesProperties()),
#endif
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

SynthPluginProcessor::~SynthPluginProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SynthPluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    using namespace juce;

    // Waveform: 0 = Sine, 1 = Saw, 2 = Square
    params.push_back (std::make_unique<AudioParameterChoice>(
        ParameterID { "WAVEFORM", 1 },      // <-- versionHint = 1
        "Waveform",
        StringArray { "Sine", "Saw", "Square" },
        0));

    // ADSR
    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { "ATTACK", 1 },
        "Attack",
        NormalisableRange<float> (0.001f, 2.0f, 0.0001f, 0.3f),
        0.01f));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { "DECAY", 1 },
        "Decay",
        NormalisableRange<float> (0.001f, 2.0f, 0.0001f, 0.3f),
        0.2f));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { "SUSTAIN", 1 },
        "Sustain",
        NormalisableRange<float> (0.0f, 1.0f, 0.0001f, 1.0f),
        0.8f));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { "RELEASE", 1 },
        "Release",
        NormalisableRange<float> (0.001f, 2.0f, 0.0001f, 0.3f),
        0.3f));

    // Filtro
    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { "CUTOFF", 1 },
        "Cutoff",
        NormalisableRange<float> (20.0f, 20000.0f, 0.01f, 0.4f),
        20000.0f));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { "RESONANCE", 1 },
        "Resonance",
        NormalisableRange<float> (0.1f, 2.0f, 0.001f, 0.5f),
        0.7f));

    return { params.begin(), params.end() };
}


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

    velocityGain.reset (sampleRate, 0.02);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    adsr.setSampleRate (sampleRate);
    adsr.reset();

    // Valores iniciales desde APVTS
    auto* waveParam   = apvts.getRawParameterValue ("WAVEFORM");
    auto* attackParam = apvts.getRawParameterValue ("ATTACK");
    auto* decayParam  = apvts.getRawParameterValue ("DECAY");
    auto* sustainParam= apvts.getRawParameterValue ("SUSTAIN");
    auto* releaseParam= apvts.getRawParameterValue ("RELEASE");
    auto* cutoffParam = apvts.getRawParameterValue ("CUTOFF");
    auto* resoParam   = apvts.getRawParameterValue ("RESONANCE");

    setWaveform ((int) std::round (waveParam->load()));
    setAdsr (attackParam->load(),
             decayParam->load(),
             sustainParam->load(),
             releaseParam->load());
    setFilter (cutoffParam->load(), resoParam->load());

    outputGain.setGainLinear (0.2f); // si querés, esto también puede ser un parámetro
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
void SynthPluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    // --- Actualizar parámetros desde APVTS ---
    auto* waveParam   = apvts.getRawParameterValue ("WAVEFORM");
    auto* attackParam = apvts.getRawParameterValue ("ATTACK");
    auto* decayParam  = apvts.getRawParameterValue ("DECAY");
    auto* sustainParam= apvts.getRawParameterValue ("SUSTAIN");
    auto* releaseParam= apvts.getRawParameterValue ("RELEASE");
    auto* cutoffParam = apvts.getRawParameterValue ("CUTOFF");
    auto* resoParam   = apvts.getRawParameterValue ("RESONANCE");

    const int waveIndex = (int) std::round (waveParam->load());
    if (waveIndex != currentWaveform.load())
        setWaveform (waveIndex);

    setAdsr (attackParam->load(),
             decayParam->load(),
             sustainParam->load(),
             releaseParam->load());

    setFilter (cutoffParam->load(), resoParam->load());

    // --- MIDI (igual que antes) ---
    keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
            startNote (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            stopNote (msg.getNoteNumber());
    }

    // --- Generación de audio (igual que ya tenías) ---
    buffer.clear();

    juce::dsp::AudioBlock<float> audioBlock (buffer);
    auto sub = audioBlock;

    juce::HeapBlock<float> monoData;
    monoData.allocate ((size_t) numSamples, true);

    float* monoChans[] = { monoData.get() };
    juce::dsp::AudioBlock<float> monoBlock (monoChans, (size_t) 1, (size_t) numSamples);
    juce::dsp::ProcessContextReplacing<float> monoContext (monoBlock);

    osc.process (monoContext);

    {
        juce::AudioBuffer<float> monoAudioBuffer (monoChans, 1, numSamples);
        adsr.applyEnvelopeToBuffer (monoAudioBuffer, 0, numSamples);
    }

    updateFilterFromAtomics();
    filter.process (monoContext);

    float* mono = monoBlock.getChannelPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        const float smoothVal = velocityGain.getNextValue();
        mono[i] *= smoothVal;
    }

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        buffer.copyFrom (ch, 0, mono, numSamples);

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
void SynthPluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SynthPluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthPluginProcessor();
}
