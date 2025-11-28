#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SynthPluginProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    SynthPluginProcessorEditor (SynthPluginProcessor&);
    ~SynthPluginProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SynthPluginProcessor& processor;
    // UI
    juce::ComboBox waveformBox;
    juce::Label   waveformLabel;

    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Label  attackLabel, decayLabel, sustainLabel, releaseLabel;

    juce::Slider cutoffSlider, resonanceSlider;
    juce::Label  cutoffLabel, resonanceLabel;

    juce::MidiKeyboardComponent keyboardComponent;

    // Version label
    juce::Label versionLabel;
    
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ComboBoxAttachment> waveformAttachment;
    std::unique_ptr<SliderAttachment>   attackAttachment, decayAttachment,
                                        sustainAttachment, releaseAttachment,
                                        cutoffAttachment, resonanceAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthPluginProcessorEditor)
};
