#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class FilterPluginAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    FilterPluginAudioProcessorEditor (FilterPluginAudioProcessor&);
    ~FilterPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FilterPluginAudioProcessor& processor;

    juce::Slider cutoffSlider;
    juce::Label  cutoffLabel { {}, "Cutoff (Hz)" };

    juce::ComboBox filterTypeBox;
    juce::Label    filterTypeLabel { {}, "Filter Type" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterPluginAudioProcessorEditor)
};
