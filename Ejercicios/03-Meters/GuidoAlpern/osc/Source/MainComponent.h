#pragma once

#include <JuceHeader.h>

// PROJUCER needs to add juce_osc

class MainComponent  : public juce::AudioAppComponent,
                       private juce::Button::Listener,
                       private juce::Timer
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    
    void setupGuiComponents();
    void setupAudioPlayer();

    // Access latest RMS values (per-channel). Thread-safe snapshot.
    juce::Array<float> getLatestRms() const;

private:
    //==============================================================================
    // Audio playback members
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    // Simple UI
    juce::TextButton loadButton { "Load..." };
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };

    // Smoothing control
    juce::Slider smoothingSlider;
    juce::Label  smoothingLabel;

    /// OSC GUI
    juce::Label hostLabel { {}, "Host" };
    juce::TextEditor hostEdit;
    juce::Label portLabel { {}, "Port" };
    juce::TextEditor portEdit;
    juce::Label addrLabel { {}, "Address" };
    juce::TextEditor addrEdit;
    juce::ToggleButton oscEnableToggle { "Send OSC" };

    /// OSC sender components
    juce::OSCSender oscSender;
    juce::String oscHost { "127.0.0.1" };
    int oscPort { 9000 };
    juce::String oscAddress { "/audio" }; // base address used to build explicit message names
    bool oscConnected { false };

    // Button::Listener
    void buttonClicked (juce::Button* button) override;

    // Helpers
    void chooseAndLoadFile();
    void loadURL (const juce::URL& url);
    void setButtonsEnabledState();

    /// OSC helpers
    void updateOscConnection();
    void disconnectOsc();
    void sendRmsOverOsc (const juce::Array<float>& values);
    void reconnectOscIfEnabled();
    void handleOscEnableToggleClicked(); // extracted handler

    // New: send combined audio features over OSC (RMS + peak + envelope)
    void sendAudioFeaturesOverOsc();

    // Metering: latest RMS per channel (smoothed), protected by a lock for cross-thread access
    mutable juce::SpinLock rmsLock;
    juce::Array<float> lastRms;
    juce::Array<float> smoothedRms;
    float rmsSmoothingAlpha = 0.2f;
    
    // New: simple audio features (peak + envelope) with a separate lock
    mutable juce::SpinLock audioFeatLock;
    float lastPeak { 0.0f };        // last block peak (0..1)
    float lastEnvelope { 0.0f };    // envelope follower state (0..1)
    float envelopeState { 0.0f };   // running state on audio thread
    float envelopeTauSec { 0.01f }; // time constant in seconds
    double currentSampleRate { 44100.0 };

    // Timer: drive UI meter updates
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
