#include "MainComponent.h"

//==============================================================================
// Utility: map MIDI note to Hz
static inline float midiToHz (int midiNote) noexcept
{
    return 440.0f * std::pow (2.0f, (midiNote - 69) / 12.0f);
}

//==============================================================================
MainComponent::MainComponent()
{
    setSize (900, 500);
    setAudioChannels (0, 2);


    // UI setup
    waveformLabel.setText ("Waveform", juce::dontSendNotification);
    addAndMakeVisible (waveformLabel);
    waveformBox.addItem ("Sine",   1);
    waveformBox.addItem ("Saw",    2);
    waveformBox.addItem ("Square", 3);
    waveformBox.setSelectedId (1, juce::dontSendNotification);
    waveformBox.addListener (this);
    addAndMakeVisible (waveformBox);


    attackLabel.setText ("A", juce::dontSendNotification);
    decayLabel.setText ("D", juce::dontSendNotification);
    sustainLabel.setText ("S", juce::dontSendNotification);
    releaseLabel.setText ("R", juce::dontSendNotification);
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

    cutoffLabel.setText ("Cutoff", juce::dontSendNotification);
    resonanceLabel.setText ("Reso", juce::dontSendNotification);
    addAndMakeVisible (cutoffLabel);
    addAndMakeVisible (resonanceLabel);

    cutoffSlider.setRange (20.0, 20000.0, 0.01);
    cutoffSlider.setSkewFactorFromMidPoint (1000.0);
    // Ensure the filter starts at the maximum cutoff value
    cutoffSlider.setValue (cutoffSlider.getMaximum(), juce::dontSendNotification);
    
    cutoffHz.store ((float) cutoffSlider.getMaximum());

    resonanceSlider.setRange (0.1, 2.0, 0.001);
    // Ensure initial resonance is at least the slider's minimum
    resonanceSlider.setValue (juce::jmax (resonance.load(), (float) resonanceSlider.getMinimum()));
    
    for (auto* s : { &cutoffSlider, &resonanceSlider })
    {
        s->addListener (this);
        addAndMakeVisible (*s);
    }

    addAndMakeVisible (keyboardComponent);
    keyboardState.addListener (this);

    // DSP setup defaults
    setWaveform (0);
    outputGain.setGainLinear (0.2f); // prevent loudness
}

MainComponent::~MainComponent()
{
    keyboardState.removeListener (this);
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused (samplesPerBlockExpected);

    // Stereo spec for outputGain (matches device)
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlockExpected);
    spec.numChannels = 2;

    // Mono spec for osc and filter (we generate/process mono then duplicate to stereo)
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    osc.prepare (monoSpec);
    filter.reset();
    filter.prepare (monoSpec);
    outputGain.prepare (spec);
    velocityGain.reset (sampleRate, 0.02); // 20 ms smoothing

    // Fixed filter type; we don't change it per block
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    updateAdsrParamsFromUI();
    adsr.setSampleRate (sampleRate);
    adsr.reset();

    // Reflect current UI values into the DSP (cutoff is already at max from constructor)
    updateFilterFromUI();

    // Initialize oscillator frequency to default target (440 Hz) until a MIDI note is played
    osc.setFrequency (targetFrequencyHz.load());
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* buffer = bufferToFill.buffer;
    if (buffer == nullptr)
        return;
    if (!osc.isInitialised())
        return;


    auto numSamples = bufferToFill.numSamples;
    auto startSample = bufferToFill.startSample;

    // Clear buffer first
    buffer->clear (startSample, numSamples);

    // Prepare a temp block for mono generation
    juce::dsp::AudioBlock<float> audioBlock (*buffer);
    auto sub = audioBlock.getSubBlock ((size_t) startSample, (size_t) numSamples);

    // Create a mono block for synthesis, then copy to stereo
    juce::HeapBlock<float> monoData;
    monoData.allocate ((size_t) numSamples, true);

    // Construct AudioBlock from channel pointer array to satisfy constructor overloads
    float* monoChans[] = { monoData.get() };
    juce::dsp::AudioBlock<float> monoBlock (monoChans, (size_t) 1, (size_t) numSamples);
    juce::dsp::ProcessContextReplacing<float> monoContext (monoBlock);


    // Generate oscillator
    osc.process (monoContext);

    // Apply ADSR to the mono buffer by wrapping monoData in a temporary AudioBuffer<float>
    {
        juce::AudioBuffer<float> monoAudioBuffer (monoChans, 1, (int) numSamples);
        adsr.applyEnvelopeToBuffer (monoAudioBuffer, 0, (int) numSamples);
    }

    // Update filter parameters (cutoff/resonance) atomically
    filter.setCutoffFrequency (cutoffHz.load());
    filter.setResonance (juce::jmax (resonance.load(), (float) resonanceSlider.getMinimum()));
    // type set once in prepareToPlay

    // Process filter (mono)
    filter.process (monoContext);
    
    float* mono = monoBlock.getChannelPointer (0);
    float smoothVal = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        smoothVal = velocityGain.getNextValue();
        mono[i] *= smoothVal;
    }

    // Copy mono to stereo and apply output gain
    for (int ch = 0; ch < buffer->getNumChannels(); ++ch)
        buffer->copyFrom (ch, startSample, monoBlock.getChannelPointer (0), (int) monoBlock.getNumSamples());

    juce::dsp::ProcessContextReplacing<float> stereoContext (sub);
    outputGain.process (stereoContext);
}

void MainComponent::releaseResources()
{
    // Nothing heavy to release beyond DSP components
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    // Top row: waveform label + combo + legato
    {
        auto topRow = area.removeFromTop (36);
        const int gap = 10;

        auto wLabel = topRow.removeFromLeft (90);
        waveformLabel.setBounds (wLabel);

        auto wBox = topRow.removeFromLeft (200);
        waveformBox.setBounds (wBox);

        topRow.removeFromLeft (gap);
    }

    area.removeFromTop (8); // small spacer

    // ADSR row: 4 columns, label above slider
    {
        auto adsrRow = area.removeFromTop (100);
        const int labelH = 18;
        const int gap = 6;

        auto colWidth = adsrRow.getWidth() / 4;

        auto layoutCol = [&] (juce::Rectangle<int> col, juce::Label& label, juce::Slider& slider)
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

    // Filter row: 2 columns, label above slider
    {
        auto filterRow = area.removeFromTop (100);
        const int labelH = 18;
        const int gap = 6;

        auto colWidth = filterRow.getWidth() / 2;

        auto layoutCol = [&] (juce::Rectangle<int> col, juce::Label& label, juce::Slider& slider)
        {
            auto labelArea = col.removeFromTop (labelH);
            label.setBounds (labelArea);
            col.removeFromTop (gap);
            slider.setBounds (col);
        };

        layoutCol (filterRow.removeFromLeft (colWidth).reduced (4), cutoffLabel,    cutoffSlider);
        layoutCol (filterRow.removeFromLeft (colWidth).reduced (4), resonanceLabel, resonanceSlider);
    }

    area.removeFromTop (8); // spacer

    keyboardComponent.setBounds (area);
}

//==============================================================================
void MainComponent::handleNoteOn (juce::MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float velocity)
{
    startNote (midiNoteNumber, velocity);
}

void MainComponent::handleNoteOff (juce::MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float /*velocity*/)
{
    stopNote (midiNoteNumber);
}

//==============================================================================
// UI listeners
void MainComponent::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &waveformBox)
    {
        const int idx = waveformBox.getSelectedId() - 1; // 0-based
        currentWaveform.store (juce::jlimit (0, 2, idx));
        setWaveform (currentWaveform.load());
    }
}

void MainComponent::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &attackSlider || slider == &decaySlider || slider == &sustainSlider || slider == &releaseSlider)
    {
        updateAdsrParamsFromUI();
    }
    else if (slider == &cutoffSlider || slider == &resonanceSlider)
    {
        updateFilterFromUI();
    }
}

//==============================================================================
// Internal helpers
void MainComponent::setWaveform (int index)
{
    switch (index)
    {
        //Oscillator has a periodic input function (-pi..pi).
        case 0: // Sine
            osc.initialise ([] (float x) { return std::sin (x); }, 128);
            break;
        case 1: // Saw
            osc.initialise ([] (float x) {
                // Normalized saw (-1..1)
                const float v = juce::jmap (x, -juce::MathConstants<float>::pi, juce::MathConstants<float>::pi, -1.0f, 1.0f);
                return v;
            }, 128);
            break;
        case 2: // Square (-1..1)
            osc.initialise ([] (float x) { return x < 0.0f ? -1.0f : 1.0f; }, 128);
            break;
        default:
            osc.initialise ([] (float x) { return std::sin (x); }, 128);
            break;
    }
}

void MainComponent::updateAdsrParamsFromUI()
{
    adsrParams.attack  = (float) attackSlider.getValue();
    adsrParams.decay   = (float) decaySlider.getValue();
    adsrParams.sustain = (float) sustainSlider.getValue();
    adsrParams.release = (float) releaseSlider.getValue();
    adsr.setParameters (adsrParams);
}

void MainComponent::updateFilterFromUI()
{
    cutoffHz.store ((float) cutoffSlider.getValue());
    // Clamp resonance to be at least the slider minimum
    const float minReso = (float) resonanceSlider.getMinimum();
    const float reso = (float) resonanceSlider.getValue();
    resonance.store (juce::jmax (reso, minReso));
}

void MainComponent::startNote (int midiNoteNumber, float velocity)
{
    const float freq = midiToHz (midiNoteNumber);
    targetFrequencyHz.store (freq);
    osc.setFrequency (freq);
    
    const bool hadActive = (activeNote.load() != -1);
    if (! hadActive) {
        adsr.noteOn();
    }
    
    activeNote.store (midiNoteNumber);
    velocityGain.setTargetValue (velocity);
}

void MainComponent::stopNote (int midiNoteNumber)
{
    if (activeNote.load() == midiNoteNumber)
    {
        adsr.noteOff();
        activeNote.store (-1);
    }
}
