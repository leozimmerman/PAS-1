#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================

FilterPluginAudioProcessor::FilterPluginAudioProcessor()
    : juce::AudioProcessor (
        BusesProperties()
            .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
            .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

FilterPluginAudioProcessor::~FilterPluginAudioProcessor() = default;

//==============================================================================

void FilterPluginAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = (sampleRate > 0.0 ? sampleRate : 44100.0);
    prevValues.clearQuick();
    updateCoefficients();
}

void FilterPluginAudioProcessor::releaseResources()
{
    prevValues.clear();
}

//==============================================================================

void FilterPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    ensureStateSize (numChannels);
    updateCoefficients();

    const auto type = filterType.load (std::memory_order_relaxed);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = buffer.getWritePointer (ch);

        if (type == FilterType::LowPass)
        {
            for (int n = 0; n < numSamples; ++n)
                data[n] = processSampleLP (data[n], ch);
        }
        else // HighPass
        {
            for (int n = 0; n < numSamples; ++n)
                data[n] = processSampleHP (data[n], ch);
        }
    }
}

void FilterPluginAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer,
                                               juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    // Convertimos a float, procesamos, y volvemos a double
    juce::AudioBuffer<float> floatBuffer (buffer.getNumChannels(), buffer.getNumSamples());

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            floatBuffer.setSample (ch, i, (float) buffer.getSample (ch, i));

    juce::MidiBuffer dummyMidi;
    processBlock (floatBuffer, dummyMidi);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (ch, i, (double) floatBuffer.getSample (ch, i));
}

//==============================================================================
// Parámetros accesibles desde el editor

void FilterPluginAudioProcessor::setCutoffHz (float newCutoff)
{
    cutoffHz.store (newCutoff, std::memory_order_relaxed);
}

float FilterPluginAudioProcessor::getCutoffHz() const
{
    return cutoffHz.load (std::memory_order_relaxed);
}

void FilterPluginAudioProcessor::setFilterType (FilterType newType)
{
    filterType.store (newType, std::memory_order_relaxed);
}

FilterPluginAudioProcessor::FilterType FilterPluginAudioProcessor::getFilterType() const
{
    return filterType.load (std::memory_order_relaxed);
}

//==============================================================================
// Implementación del filtro one-pole

void FilterPluginAudioProcessor::ensureStateSize (int numChannels)
{
    if (prevValues.size() < numChannels)
    {
        const int oldSize = prevValues.size();
        prevValues.resize (numChannels);

        for (int i = oldSize; i < numChannels; ++i)
            prevValues.setUnchecked (i, 0.0f);
    }
}

void FilterPluginAudioProcessor::updateCoefficients()
{
    auto fs = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
    auto fc = juce::jlimit (10.0f,
                            (float) (0.45 * fs),
                            cutoffHz.load (std::memory_order_relaxed));

    const double alpha = std::exp (-2.0 * juce::MathConstants<double>::pi
                                   * (double) fc / fs);

    a0 = (float) (1.0 - alpha);
    b1 = (float) alpha;
}

float FilterPluginAudioProcessor::processSampleLP (float x, int ch)
{
    const float prev = prevValues.getUnchecked (ch);
    float y = a0 * x + b1 * prev;
    prevValues.setUnchecked (ch, y);
    return y;
}

float FilterPluginAudioProcessor::processSampleHP (float x, int ch)
{
    float lp = processSampleLP (x, ch);
    return x - lp;
}

//==============================================================================

juce::AudioProcessorEditor* FilterPluginAudioProcessor::createEditor()
{
    return new FilterPluginAudioProcessorEditor (*this);
}

//==============================================================================
// Estado (cutoff + tipo de filtro)

void FilterPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream (destData, true);
    stream.writeFloat (getCutoffHz());
    stream.writeInt ((int) getFilterType());
}

void FilterPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream (data, (size_t) sizeInBytes, false);

    const float storedCutoff = stream.readFloat();
    const int storedType     = stream.readInt();

    setCutoffHz (storedCutoff);
    setFilterType (storedType == (int) FilterType::HighPass ? FilterType::HighPass
                                                            : FilterType::LowPass);
}
//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FilterPluginAudioProcessor();
}
