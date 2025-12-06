#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
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

    // Access latest RMS values (per-channel). Thread-safe snapshot.
    juce::Array<float> getLatestRms() const;

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

    // Smoothing control
    juce::Slider smoothingSlider;
    juce::Label  smoothingLabel;

    // Noise control
    juce::Slider noiseSlider;
    juce::Label  noiseLabel;

    // Random generator for noise
    juce::Random noiseGenerator;

    // Button::Listener
    void buttonClicked(juce::Button* button) override;

    // Helpers
    void chooseAndLoadFile();
    void loadURL(const juce::URL& url);
    void setButtonsEnabledState();

    // Metering: latest RMS per channel (smoothed), protected by a lock for cross-thread access
    mutable juce::SpinLock rmsLock;
    juce::Array<float> lastRms;
    juce::Array<float> smoothedRms;
    float rmsSmoothingAlpha = 0.2f;
    float noiseAmount = 0.0f; // Noise control variable

    // Timer: drive UI meter updates
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};