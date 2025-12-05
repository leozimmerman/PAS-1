#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Simple MIDI arpeggiator plugin.
*/
class ArpeggiatorPluginAudioProcessor  : public juce::AudioProcessor
{
public:
    // Dirección del arpegio
    enum class ArpDirection
    {
        Up,
        Down,
        UpDown,
        Random
    };

    using APVTS = juce::AudioProcessorValueTreeState;

    //==============================================================================
    ArpeggiatorPluginAudioProcessor();
    ~ArpeggiatorPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // APVTS para parámetros (visible al editor)
    APVTS apvts;
    static APVTS::ParameterLayout createParameterLayout();

    // Getters simples
    double      getBpm() const noexcept             { return bpm; }
    int         getDivisionValue() const noexcept   { return division; }
    ArpDirection getDirection() const noexcept      { return direction; }

private:
    //======================= Estructuras internas ================================
    struct HeldNote
    {
        int noteNumber = 0;
        int velocity   = 0;
        int channel    = 1;
    };

    std::vector<HeldNote> heldNotes;

    int   currentNoteIndex   = -1; // índice de heldNotes
    int   currentNote        = -1; // nota actual sonando (-1 = ninguna)
    int   currentChannel     = 1;
    int   currentVelocity    = 0;

    // Timing
    double currentSampleRate   = 44100.0;
    double bpm                 = 120.0;   // valor por defecto
    int    division            = 16;      // 4=negra, 8=corchea, 16=semicorchea, 32=fusa

    int    samplesPerStep       = 1;
    int    samplesUntilNextStep = 1;

    ArpDirection direction      = ArpDirection::Up;
    bool         goingUp        = true;   // para modo UpDown
    juce::Random rng;

    //======================= Helpers de arpegiador ==============================
    void updateTimingFromHost();
    void updateTimingFromBpm();

    int  getNextIndex();
    void sortHeldNotes();

    void turnOffCurrentNote (juce::MidiBuffer& midiOut, int samplePos);
    void noteOnReceived  (int noteNumber, int velocity, int channel,
                          juce::MidiBuffer& midiOut, int samplePos);
    void noteOffReceived (int noteNumber, int channel,
                          juce::MidiBuffer& midiOut, int samplePos);

    static int         divisionFromIndex (int index);
    static ArpDirection directionFromIndex (int index);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArpeggiatorPluginAudioProcessor)
};
