#pragma once

#include <JuceHeader.h>

// PROJUCER needs to add juce_osc

class MainComponent : public juce::AudioAppComponent,
    private juce::Button::Listener,
    private juce::Timer
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    void setupGuiComponents();
    void setupAudioPlayer();

    // Access latest frequency band values (bass/mid/treble). Thread-safe snapshot.
    juce::Array<float> getLatestFrequencyBands() const;

private:
    //==============================================================================
    // Audio playback members
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    // Simple UI
    juce::TextButton loadButton{ "Load..." };
    juce::TextButton playButton{ "Play" };
    juce::TextButton stopButton{ "Stop" };

    // Frequency bands control
    juce::Slider bassSmoothingSlider;
    juce::Label  bassSmoothingLabel;
    juce::Slider midSmoothingSlider;
    juce::Label  midSmoothingLabel;
    juce::Slider trebleSmoothingSlider;
    juce::Label  trebleSmoothingLabel;

    /// OSC GUI
    juce::Label hostLabel{ {}, "Host" };
    juce::TextEditor hostEdit;
    juce::Label portLabel{ {}, "Port" };
    juce::TextEditor portEdit;
    juce::Label addrLabel{ {}, "Address" };
    juce::TextEditor addrEdit;
    juce::ToggleButton oscEnableToggle{ "Send OSC" };

    /// OSC sender components
    juce::OSCSender oscSender;
    juce::String oscHost{ "127.0.0.1" };
    int oscPort{ 9000 };
    juce::String oscAddress{ "/frequencyBands" };
    bool oscConnected{ false };

    // Button::Listener
    void buttonClicked(juce::Button* button) override;

    // Helpers
    void chooseAndLoadFile();
    void loadURL(const juce::URL& url);
    void setButtonsEnabledState();

    /// OSC helpers
    void updateOscConnection();
    void disconnectOsc();
    void sendFrequencyBandsOverOsc(const juce::Array<float>& values);
    void reconnectOscIfEnabled();
    void handleOscEnableToggleClicked(); // extracted handler

    // Frequency band analysis
    mutable juce::SpinLock bandsLock;
    juce::Array<float> lastFrequencyBands; // [bass, mid, treble]
    juce::Array<float> smoothedFrequencyBands;

    // Filters for frequency bands
    juce::IIRFilter bassFilterL, bassFilterR;
    juce::IIRFilter midFilterL, midFilterR;
    juce::IIRFilter trebleFilterL, trebleFilterR;

    // Smoothing factors for each band
    float bassSmoothingAlpha = 0.3f;
    float midSmoothingAlpha = 0.3f;
    float trebleSmoothingAlpha = 0.3f;

    double currentSampleRate = 44100.0;

    // Timer: drive UI meter updates
    void timerCallback() override;

    // Filter setup
    void setupFilters();
    void updateFilterCoefficients();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};