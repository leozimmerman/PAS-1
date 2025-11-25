#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Editor: UI del sinte (waveform, ADSR, filtro, teclado MIDI)
class SimpleSynthAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                         private juce::ComboBox::Listener,
                                         private juce::Slider::Listener
{
public:
    SimpleSynthAudioProcessorEditor (SimpleSynthAudioProcessor&);
    ~SimpleSynthAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SimpleSynthAudioProcessor& processor;

    // UI
    juce::ComboBox waveformBox;
    juce::Label   waveformLabel;

    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Label  attackLabel, decayLabel, sustainLabel, releaseLabel;

    juce::Slider cutoffSlider, resonanceSlider;
    juce::Label  cutoffLabel, resonanceLabel;

    juce::MidiKeyboardComponent keyboardComponent;

    // Listeners
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;
    void sliderValueChanged (juce::Slider* slider) override;

    // Helpers
    void updateAdsrFromUI();
    void updateFilterFromUI();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleSynthAudioProcessorEditor)
};
