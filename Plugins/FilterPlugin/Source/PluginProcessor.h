#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class FilterPluginAudioProcessor  : public juce::AudioProcessor
{
public:
    enum class FilterType { LowPass, HighPass };

    //==============================================================================
    FilterPluginAudioProcessor();
    ~FilterPluginAudioProcessor() override;

    //==============================================================================
    const juce::String getName() const override              { return "FilterPlugin"; }

    bool acceptsMidi() const override                        { return false; }
    bool producesMidi() const override                       { return false; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 0.0; }

    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock (juce::AudioBuffer<float>&,  juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    //==============================================================================
    bool hasEditor() const override                          { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // API para el editor (UI)
    void setCutoffHz (float newCutoff);
    float getCutoffHz() const;

    void setFilterType (FilterType newType);
    FilterType getFilterType() const;

private:
    //==============================================================================
    // Estado del filtro
    double currentSampleRate { 44100.0 };
    std::atomic<float> cutoffHz { 2000.0f };
    std::atomic<FilterType> filterType { FilterType::LowPass };

    juce::Array<float> prevValues;   // y[n-1] por canal
    float a0 { 1.0f };
    float b1 { 0.0f };

    void ensureStateSize (int numChannels);
    void updateCoefficients();
    float processSampleLP (float x, int ch);
    float processSampleHP (float x, int ch);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterPluginAudioProcessor)
};
