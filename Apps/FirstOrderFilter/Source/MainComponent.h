#pragma once

#include <JuceHeader.h>

//==============================================================================
// This component lives inside our window, and this is where you should put all
// your controls and content.
class MainComponent  : public juce::AudioAppComponent,
                       private juce::Button::Listener,
                       private juce::ChangeListener
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

    // New filter UI
    juce::Slider cutoffSlider;
    juce::Label  cutoffLabel { {}, "Cutoff (Hz)" };

    juce::ComboBox filterTypeBox;
    juce::Label    filterTypeLabel { {}, "Filter Type" };

    // Button::Listener
    void buttonClicked (juce::Button* button) override;

    // ChangeListener (to observe transport state changes)
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    // Helpers
    void chooseAndLoadFile();
    void loadURL (const juce::URL& url);
    void setButtonsEnabledState();

    //==============================================================================
    // Simple first-order filter (no JUCE filter classes)
    enum class FilterType { LowPass, HighPass };

    // runtime parameters
    double currentSampleRate { 44100.0 };
    std::atomic<float> cutoffHz { 2000.0f };   // default cutoff
    std::atomic<FilterType> filterType { FilterType::LowPass };

    // per-channel state (z^-1)
    juce::Array<float> z1; // previous output (for LP) / previous input-output combo (we'll keep one state form)

    // coefficients
    float a0 { 1.0f };
    float b1 { 0.0f };

    void updateCoefficients();
    inline float processSampleLP (float x, int ch);
    inline float processSampleHP (float x, int ch);

    // Ensure z1 size matches channel count
    void ensureStateSize (int numChannels);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
