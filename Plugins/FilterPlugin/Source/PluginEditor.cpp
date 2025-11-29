#include "PluginEditor.h"

//==============================================================================

FilterPluginAudioProcessorEditor::FilterPluginAudioProcessorEditor (FilterPluginAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p)
{
    setSize (400, 160);

    // Slider de cutoff
    cutoffSlider.setSliderStyle (juce::Slider::SliderStyle::LinearHorizontal);
    cutoffSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    cutoffSlider.setRange (20.0, 20000.0, 0.01);
    cutoffSlider.setSkewFactorFromMidPoint (1000.0);

    cutoffSlider.setValue ((double) processor.getCutoffHz(), juce::dontSendNotification);
    cutoffSlider.onValueChange = [this]
    {
        processor.setCutoffHz ((float) cutoffSlider.getValue());
    };

    cutoffLabel.setJustificationType (juce::Justification::centredLeft);
    cutoffLabel.attachToComponent (&cutoffSlider, true);

    addAndMakeVisible (cutoffSlider);
    addAndMakeVisible (cutoffLabel);

    filterTypeBox.addItem ("Low-Pass", 1);
    filterTypeBox.addItem ("High-Pass", 2);

    filterTypeBox.onChange = [this]
    {
        const int sel = filterTypeBox.getSelectedId();
        processor.setFilterType (sel == 2
                                   ? FilterPluginAudioProcessor::FilterType::HighPass
                                   : FilterPluginAudioProcessor::FilterType::LowPass);
    };

    auto currentType = processor.getFilterType();
    filterTypeBox.setSelectedId (currentType == FilterPluginAudioProcessor::FilterType::HighPass ? 2 : 1,
                                 juce::dontSendNotification);

    filterTypeLabel.setJustificationType (juce::Justification::centredLeft);
    filterTypeLabel.attachToComponent (&filterTypeBox, true);

    addAndMakeVisible (filterTypeBox);
    addAndMakeVisible (filterTypeLabel);
}

//==============================================================================

FilterPluginAudioProcessorEditor::~FilterPluginAudioProcessorEditor() = default;

//==============================================================================

void FilterPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void FilterPluginAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);

    auto typeRow = area.removeFromTop (30);
    typeRow.removeFromLeft (110);
    filterTypeBox.setBounds (typeRow.removeFromLeft (180));

    area.removeFromTop (10);

    auto cutoffRow = area.removeFromTop (40);
    cutoffRow.removeFromLeft (110);
    cutoffSlider.setBounds (cutoffRow);
}
