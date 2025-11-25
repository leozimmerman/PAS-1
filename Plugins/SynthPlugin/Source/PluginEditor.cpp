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

    // --- Waveform ---
    waveformLabel.setText ("Waveform", juce::dontSendNotification);
    addAndMakeVisible (waveformLabel);

    waveformBox.addItem ("Sine",   1);
    waveformBox.addItem ("Saw",    2);
    waveformBox.addItem ("Square", 3);
    addAndMakeVisible (waveformBox);

    // --- ADSR ---
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

    for (auto* s : { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider })
        addAndMakeVisible (*s);

    // --- Filtro ---
    cutoffLabel.setText ("Cutoff", juce::dontSendNotification);
    resonanceLabel.setText ("Reso", juce::dontSendNotification);
    addAndMakeVisible (cutoffLabel);
    addAndMakeVisible (resonanceLabel);

    cutoffSlider.setRange (20.0, 20000.0, 0.01);
    cutoffSlider.setSkewFactorFromMidPoint (1000.0);
    resonanceSlider.setRange (0.1, 2.0, 0.001);

    addAndMakeVisible (cutoffSlider);
    addAndMakeVisible (resonanceSlider);

    // --- Teclado ----- Version label ---
   
    versionLabel.setText ("v.0.1", juce::dontSendNotification);
    versionLabel.setJustificationType (juce::Justification::centredRight);
    versionLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    versionLabel.setInterceptsMouseClicks (false, false);
    // Use FontOptions-based constructor to avoid deprecation warning
    versionLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    addAndMakeVisible (versionLabel);

    // -
    addAndMakeVisible (keyboardComponent);

    // === AQUÍ ES DONDE SE LINKEAN UI <-> APVTS ===
    waveformAttachment  = std::make_unique<ComboBoxAttachment> (processor.apvts, "WAVEFORM",  waveformBox);

    attackAttachment    = std::make_unique<SliderAttachment>   (processor.apvts, "ATTACK",    attackSlider);
    decayAttachment     = std::make_unique<SliderAttachment>   (processor.apvts, "DECAY",     decaySlider);
    sustainAttachment   = std::make_unique<SliderAttachment>   (processor.apvts, "SUSTAIN",   sustainSlider);
    releaseAttachment   = std::make_unique<SliderAttachment>   (processor.apvts, "RELEASE",   releaseSlider);

    cutoffAttachment    = std::make_unique<SliderAttachment>   (processor.apvts, "CUTOFF",    cutoffSlider);
    resonanceAttachment = std::make_unique<SliderAttachment>   (processor.apvts, "RESONANCE", resonanceSlider);

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
        
        auto versionBounds = topRow.removeFromRight (140);
        versionLabel.setBounds (versionBounds);
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
