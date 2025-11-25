#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
SynthPluginProcessorEditor::SynthPluginProcessorEditor (SynthPluginProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p),
      keyboardComponent (processor.keyboardState,
                         juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setSize (900, 500);

    // --- Waveform -------------------------------------------------------------
    waveformLabel.setText ("Waveform", juce::dontSendNotification);
    addAndMakeVisible (waveformLabel);

    waveformBox.addItem ("Sine",   1);
    waveformBox.addItem ("Saw",    2);
    waveformBox.addItem ("Square", 3);
    waveformBox.setSelectedId (1, juce::dontSendNotification); // sine por defecto
    waveformBox.addListener (this);
    addAndMakeVisible (waveformBox);

    // --- ADSR -----------------------------------------------------------------
    attackLabel.setText ("A", juce::dontSendNotification);
    decayLabel.setText  ("D", juce::dontSendNotification);
    sustainLabel.setText("S", juce::dontSendNotification);
    releaseLabel.setText("R", juce::dontSendNotification);

    addAndMakeVisible (attackLabel);
    addAndMakeVisible (decayLabel);
    addAndMakeVisible (sustainLabel);
    addAndMakeVisible (releaseLabel);

    attackSlider.setRange (0.001, 2.0, 0.0001);
    decaySlider.setRange  (0.001, 2.0, 0.0001);
    sustainSlider.setRange(0.0,   1.0, 0.0001);
    releaseSlider.setRange(0.001, 2.0, 0.0001);

    attackSlider.setValue (0.01);
    decaySlider.setValue  (0.2);
    sustainSlider.setValue(0.8);
    releaseSlider.setValue(0.3);

    for (auto* s : { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider })
    {
        s->addListener (this);
        addAndMakeVisible (*s);
    }

    // --- Filtro ---------------------------------------------------------------
    cutoffLabel.setText ("Cutoff", juce::dontSendNotification);
    resonanceLabel.setText ("Reso", juce::dontSendNotification);
    addAndMakeVisible (cutoffLabel);
    addAndMakeVisible (resonanceLabel);

    cutoffSlider.setRange (20.0, 20000.0, 0.01);
    cutoffSlider.setSkewFactorFromMidPoint (1000.0);
    cutoffSlider.setValue (cutoffSlider.getMaximum(), juce::dontSendNotification);

    resonanceSlider.setRange (0.1, 2.0, 0.001);
    resonanceSlider.setValue (0.7f); // valor inicial razonable

    for (auto* s : { &cutoffSlider, &resonanceSlider })
    {
        s->addListener (this);
        addAndMakeVisible (*s);
    }

    // --- Teclado MIDI ---------------------------------------------------------
    addAndMakeVisible (keyboardComponent);

    // Enviar los valores iniciales al processor
    processor.setWaveform (0);
    updateAdsrFromUI();
    updateFilterFromUI();
}

SynthPluginProcessorEditor::~SynthPluginProcessorEditor() = default;

//==============================================================================
void SynthPluginProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void SynthPluginProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    // Top row: waveform label + combo
    {
        auto topRow = area.removeFromTop (36);
        const int gap = 10;

        auto wLabel = topRow.removeFromLeft (90);
        waveformLabel.setBounds (wLabel);

        auto wBox = topRow.removeFromLeft (200);
        waveformBox.setBounds (wBox);

        topRow.removeFromLeft (gap);
    }

    area.removeFromTop (8); // spacer

    // ADSR row
    {
        auto adsrRow = area.removeFromTop (100);
        const int labelH = 18;
        const int gap = 6;

        auto colWidth = adsrRow.getWidth() / 4;

        auto layoutCol = [&] (juce::Rectangle<int> col,
                              juce::Label& label, juce::Slider& slider)
        {
            auto labelArea = col.removeFromTop (labelH);
            label.setBounds (labelArea);
            col.removeFromTop (gap);
            slider.setBounds (col);
        };

        layoutCol (adsrRow.removeFromLeft (colWidth).reduced (4), attackLabel,  attackSlider);
        layoutCol (adsrRow.removeFromLeft (colWidth).reduced (4), decayLabel,   decaySlider);
        layoutCol (adsrRow.removeFromLeft (colWidth).reduced (4), sustainLabel, sustainSlider);
        layoutCol (adsrRow.removeFromLeft (colWidth).reduced (4), releaseLabel, releaseSlider);
    }

    area.removeFromTop (8); // spacer

    // Filter row
    {
        auto filterRow = area.removeFromTop (100);
        const int labelH = 18;
        const int gap = 6;

        auto colWidth = filterRow.getWidth() / 2;

        auto layoutCol = [&] (juce::Rectangle<int> col,
                              juce::Label& label, juce::Slider& slider)
        {
            auto labelArea = col.removeFromTop (labelH);
            label.setBounds (labelArea);
            col.removeFromTop (gap);
            slider.setBounds (col);
        };

        layoutCol (filterRow.removeFromLeft (colWidth).reduced (4),
                   cutoffLabel,    cutoffSlider);
        layoutCol (filterRow.removeFromLeft (colWidth).reduced (4),
                   resonanceLabel, resonanceSlider);
    }

    area.removeFromTop (8); // spacer

    keyboardComponent.setBounds (area);
}

//==============================================================================
// Listeners

void SynthPluginProcessorEditor::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &waveformBox)
    {
        const int idx = waveformBox.getSelectedId() - 1; // 0-based
        processor.setWaveform (idx);
    }
}

void SynthPluginProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &attackSlider || slider == &decaySlider
        || slider == &sustainSlider || slider == &releaseSlider)
    {
        updateAdsrFromUI();
    }
    else if (slider == &cutoffSlider || slider == &resonanceSlider)
    {
        updateFilterFromUI();
    }
}

void SynthPluginProcessorEditor::updateAdsrFromUI()
{
    const float a = (float) attackSlider.getValue();
    const float d = (float) decaySlider.getValue();
    const float s = (float) sustainSlider.getValue();
    const float r = (float) releaseSlider.getValue();

    processor.setAdsr (a, d, s, r);
}

void SynthPluginProcessorEditor::updateFilterFromUI()
{
    const float cutoff = (float) cutoffSlider.getValue();
    const float reso   = (float) resonanceSlider.getValue();

    processor.setFilter (cutoff, reso);
}
