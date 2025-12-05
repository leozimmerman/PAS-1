#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class ArpeggiatorPluginAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    ArpeggiatorPluginAudioProcessorEditor (ArpeggiatorPluginAudioProcessor&);
    ~ArpeggiatorPluginAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ArpeggiatorPluginAudioProcessor& audioProcessor;

    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    juce::Label divisionLabel;
    juce::Label directionLabel;
    juce::Label infoLabel;     // texto con info (tempo, etc.)

    juce::ComboBox divisionBox;
    juce::ComboBox directionBox;

    std::unique_ptr<ComboBoxAttachment> divisionAttachment;
    std::unique_ptr<ComboBoxAttachment> directionAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArpeggiatorPluginAudioProcessorEditor)
};
