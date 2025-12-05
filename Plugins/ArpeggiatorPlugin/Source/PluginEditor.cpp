#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ArpeggiatorPluginAudioProcessorEditor::ArpeggiatorPluginAudioProcessorEditor (ArpeggiatorPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    using namespace juce;

    setSize (340, 140);

    // Labels
    divisionLabel.setText ("Division", dontSendNotification);
    divisionLabel.setJustificationType (Justification::centredRight);
    addAndMakeVisible (divisionLabel);

    directionLabel.setText ("Direction", dontSendNotification);
    directionLabel.setJustificationType (Justification::centredRight);
    addAndMakeVisible (directionLabel);

    infoLabel.setText ("Host-synced arpeggiator", dontSendNotification);
    infoLabel.setJustificationType (Justification::centred);
    addAndMakeVisible (infoLabel);

    // Division ComboBox
    divisionBox.addItem ("1/4",  1);
    divisionBox.addItem ("1/8",  2);
    divisionBox.addItem ("1/16", 3);
    divisionBox.addItem ("1/32", 4);
    addAndMakeVisible (divisionBox);

    // Direction ComboBox
    directionBox.addItem ("Up",      1);
    directionBox.addItem ("Down",    2);
    directionBox.addItem ("UpDown",  3);
    directionBox.addItem ("Random",  4);
    addAndMakeVisible (directionBox);

    // Attachments (conectan UI <-> APVTS)
    divisionAttachment  = std::make_unique<ComboBoxAttachment> (audioProcessor.apvts, "DIVISION",  divisionBox);
    directionAttachment = std::make_unique<ComboBoxAttachment> (audioProcessor.apvts, "DIRECTION", directionBox);
}

ArpeggiatorPluginAudioProcessorEditor::~ArpeggiatorPluginAudioProcessorEditor() = default;

//==============================================================================
void ArpeggiatorPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;

    g.fillAll (Colours::black);

    g.setColour (Colours::white);
    g.setFont (16.0f);

    g.drawFittedText ("MIDI Arpeggiator", getLocalBounds().removeFromTop (24),
                      Justification::centred, 1);

    auto bpmText = "BPM (host): " + String (audioProcessor.getBpm(), 1);
    infoLabel.setText (bpmText, dontSendNotification);
}

void ArpeggiatorPluginAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto titleArea = area.removeFromTop (24);
    juce::ignoreUnused (titleArea);

    auto infoArea = area.removeFromBottom (24);
    infoLabel.setBounds (infoArea);

    const int rowHeight  = 28;
    const int labelWidth = 80;

    auto leftCol  = area.removeFromLeft (labelWidth + 8);
    auto rightCol = area;

    // División
    auto row1 = rightCol.removeFromTop (rowHeight);
    divisionLabel.setBounds (leftCol.removeFromTop (rowHeight));
    divisionBox.setBounds   (row1.reduced (4, 2));

    // Dirección
    auto row2 = rightCol.removeFromTop (rowHeight);
    directionLabel.setBounds (leftCol.removeFromTop (rowHeight));
    directionBox.setBounds   (row2.reduced (4, 2));
}
