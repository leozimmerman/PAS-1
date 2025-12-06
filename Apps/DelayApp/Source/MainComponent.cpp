#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize(800, 400);

    // Register audio formats we can read
    formatManager.registerBasicFormats();

    // Transport UI
    addAndMakeVisible(loadButton);
    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);

    loadButton.onClick = [this]
        {
            chooseAndLoadFile();
        };
    playButton.onClick = [this]
        {
            transport.start();
            setButtonsEnabledState();
        };
    stopButton.onClick = [this]
        {
            transport.stop();
            setButtonsEnabledState();
        };

    // Ranges and defaults
    mixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); // Inicializo el slider que agregué
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    mixSlider.setRange(0.0, 1.0, 0.01);
    mixSlider.setValue(0.5f);
    /*
    delayTimeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    delayTimeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    delayTimeSlider.setRange(1.0, 2000.0, 1.0); // ms
    delayTimeSlider.setValue(delayTimeMs);
    */
    depthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    depthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    depthSlider.setRange(0.0, 20.0, 0.01);
    depthSlider.setValue(depthMs);

    rateSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    rateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    rateSlider.setRange(0.01, 10.0, 0.01);
    rateSlider.setValue(lfoRateHz);

    feedbackSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    feedbackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    feedbackSlider.setRange(0.0, 0.95, 0.001);
    feedbackSlider.setValue(feedback);

    // Labels
    mixLabel.attachToComponent(&mixSlider, false);
    //delayTimeLabel.attachToComponent(&delayTimeSlider, false);
    depthLabel.attachToComponent(&depthSlider, false);
    rateLabel.attachToComponent(&rateSlider, false);
    feedbackLabel.attachToComponent(&feedbackSlider, false);

    mixLabel.setJustificationType(juce::Justification::centred);
    //delayTimeLabel.setJustificationType(juce::Justification::centred);
    depthLabel.setJustificationType(juce::Justification::centred);
    rateLabel.setJustificationType(juce::Justification::centred);
    feedbackLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(mixSlider); // Agrego
    //addAndMakeVisible(delayTimeSlider);
    addAndMakeVisible(depthSlider); // Agrego
    addAndMakeVisible(rateSlider); // Agrego
    addAndMakeVisible(feedbackSlider);
    addAndMakeVisible(mixLabel); // Agrego
    //dAndMakeVisible(delayTimeLabel);
    addAndMakeVisible(depthLabel); // Agrego
    addAndMakeVisible(rateLabel); // Agrego
    addAndMakeVisible(feedbackLabel);

    // Slider callbacks

    mixSlider.onValueChange = [this] // El slider que agregué
        {
            wetMix = (float)mixSlider.getValue(); // El slider controla el valor de wet
            dryMix = 1.0f - wetMix; // los dos suman 1
        };
    /*
    delayTimeSlider.onValueChange = [this]
        {
            delayTimeMs = (float)delayTimeSlider.getValue();
            // Map ms to samples and clamp to current max
            const int newDelaySamples = (int)std::round((delayTimeMs * 0.001) * currentSampleRate);
            DBG("Delay time changed: " << delayTimeMs << " ms -> " << newDelaySamples << " samples");

            if (maxDelaySamples > 0) // maxDelaySeconds is set to 2 seconds.
                delaySamples = juce::jlimit(1, juce::jmax(1, maxDelaySamples - 1), newDelaySamples);
            else
                delaySamples = juce::jmax(1, newDelaySamples);
        };
     */
    depthSlider.onValueChange = [this]
        {
            depthMs = (float)depthSlider.getValue();
        };

    rateSlider.onValueChange = [this]
        {
            lfoRateHz = (float)rateSlider.getValue();

            if (currentSampleRate > 0)
                lfoIncrement = juce::MathConstants<float>::twoPi * (lfoRateHz / currentSampleRate);
        };

    feedbackSlider.onValueChange = [this]
        {
            feedback = (float)feedbackSlider.getValue();
        };

    setButtonsEnabledState();

    juce::MessageManagerLock mmLock;
    transport.addChangeListener(this);

    setAudioChannels(0, 2);
}

MainComponent::~MainComponent()
{
    {
        juce::MessageManagerLock mmLock; // remove listener safely
        transport.removeChangeListener(this);
    }

    // This shuts down the audio device and clears the audio source.
    shutdownAudio();

    // Ensure transport is stopped and reader released before destruction
    transport.stop();
    transport.setSource(nullptr);
    readerSource.reset();
}

//==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused(samplesPerBlockExpected);
    currentSampleRate = sampleRate;
    transport.prepareToPlay(samplesPerBlockExpected, sampleRate);

    // Prepare delay state
    prepareDelayState();

    // Ensure delaySamples matches current delayTimeMs
    //delayTimeSlider.onValueChange();
    // Calculo el paso del LFO en radianes por muestra.
    lfoIncrement = juce::MathConstants<float>::twoPi * (lfoRateHz / currentSampleRate);
    lfoPhase = 0.0f;
}
/*
float MainComponent::processLFO()
{
    // Triangular unipolar 0..1..0..1..
    float value;

    if (lfoPhase < juce::MathConstants<float>::pi)
        value = lfoPhase / juce::MathConstants<float>::pi;               // Subida 0→1
    else
        value = 2.0f - (lfoPhase / juce::MathConstants<float>::pi);      // Bajada 1→0

    // Avanza fase
    lfoPhase += lfoIncrement;
    if (lfoPhase >= juce::MathConstants<float>::twoPi)
        lfoPhase -= juce::MathConstants<float>::twoPi;

    return value; // Unipolar 0..1
}
*/
// El código comentado arriba es para que el lfo sea triangular
float MainComponent::processLFO()
{
    // LFO senoidal
    // el seno da valores en (-1,1)
    // le sumo 1 y da valores en (0,2)
    // lo multiplico por 0.5 y queda en (0,1)
    float value = 0.5f * (1.0f + std::sin(lfoPhase));

    // incremento la fase
    lfoPhase += lfoIncrement;
    if (lfoPhase >= juce::MathConstants<float>::twoPi)
        lfoPhase -= juce::MathConstants<float>::twoPi;

    return value;
}


void MainComponent::prepareDelayState()
{
    // Choose a safe maximum delay time (in seconds)
    const float maxDelaySeconds = 2.0f; // 2 seconds max delay
    // maxDelaySamples = (int)std::ceil(maxDelaySeconds * currentSampleRate);

    // Esta técnica la saqué de la sección 14.3 (“An Efficient Circular Buffer Object”)
    // del libro Designing Audio Effect Plugins in C++ for AAX, AU, and VST3 with DSP Theory.
    // Uso el wrap por bitmask porque Pirkle explica que es más rápido que el operador % o el if
    // (pero requiere que el tamaño del buffer sea potencia de 2).
    // quiero que maxDelaySamples sea una potencia de 2
    int samplesNeeded = (int)std::ceil(maxDelaySeconds * currentSampleRate);
    // fuerzo a que sea la potencia de dos más cercana por encima del tamaño máximo deseado
    maxDelaySamples = juce::nextPowerOfTwo(samplesNeeded);


    delayBuffer.assign((size_t)maxDelaySamples, 0.0f);

    delayMask = maxDelaySamples - 1;

    writePos = 0;

}

void MainComponent::processFlangerChannel(juce::AudioBuffer<float>& buffer, int channelNum)
{
    if (buffer.getNumSamples() <= 0 || delayBuffer.empty())
        return;

    if (channelNum < 0 || channelNum >= buffer.getNumChannels())
        return;

    auto* data = buffer.getWritePointer(channelNum);
    const int numSamples = buffer.getNumSamples();
    // delayBuffSize es constante, definido en funcion del maximo de delay permitido (2s.)
    const int delayBuffSize = (int)delayBuffer.size();

    for (int i = 0; i < numSamples; ++i)
    {
        /*
        int readPos = writePos - delaySamples;
        if (readPos < 0)
            readPos += delayBuffSize;
        */
        //int readPos = (writePos - delaySamples) & delayMask; // se hace wire-AND con la máscara
        
        float lfoValue = processLFO(); // Obtengo el valor del LFO entre 0 y 1

        // Calculo delay actual modulado por el lfo
        float currentDelayMs = depthMs * lfoValue; // un valor entre 0 y Depth (en milisegundos)
        float delaySamples = (currentDelayMs * 0.001f) * currentSampleRate; // paso a muestras (pasando primero por segundos)

        // Calculo la posición de lectura con precisión flotante
        float readPosFloat = (float)writePos - delaySamples;

        // Ajustamos para wrap-around manual si es negativo
        if (readPosFloat < 0.0f)
            readPosFloat += (float)maxDelaySamples;

        // Índices enteros para interpolación
        int idxA = (int)readPosFloat;
        int idxB = (idxA + 1) & delayMask;

        // Parte fraccionaria
        float frac = readPosFloat - (float)idxA;

        // Interpolación lineal
        float delayed =
            delayBuffer[(size_t)idxA] * (1.0f - frac) +
            delayBuffer[(size_t)idxB] * frac;

        //const float in = data[i]; // read data from buffer
        float in = data[i];

        delayBuffer[(size_t)writePos] = in + feedback * delayed;

        // Output write to streaming AudioBuffer: dry + wet (fixed 50/50)
        // data[i] = in + delayed;
        data[i] = dryMix * in + wetMix * delayed; // Para poder controlar la mezcla dry/ wet
        //data[i] = delayed; /// Test only delay

        // advance circular index (0-88199)
        /*
        writePos = writePos + 1;
        if (writePos == delayBuffSize)
            writePos = 0;
        */
        // cambié el if por bitmask
        writePos = (writePos + 1) & delayMask;
    }
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Fill from transport, or clear if no source
    if (readerSource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    transport.getNextAudioBlock(bufferToFill);

    auto* buf = bufferToFill.buffer;

    if (buf == nullptr || bufferToFill.numSamples <= 0)
        return;

    // Procesás solo L
    processFlangerChannel(*buf, 0);

    // Si hay dos canales, copio L → R
    if (buf->getNumChannels() > 1)
        buf->copyFrom(1, 0, *buf, 0, 0, bufferToFill.numSamples);

}

void MainComponent::releaseResources()
{
    transport.releaseResources();

    // Clear delay buffers
    delayBuffer.clear();
    writePos = 0;
    maxDelaySamples = 0;
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    // Top row: buttons
    auto area = getLocalBounds().reduced(20);
    auto buttonHeight = 32;
    auto row = area.removeFromTop(buttonHeight);

    loadButton.setBounds(row.removeFromLeft(120));
    row.removeFromLeft(10);
    playButton.setBounds(row.removeFromLeft(120));
    row.removeFromLeft(10);
    stopButton.setBounds(row.removeFromLeft(120));

    area.removeFromTop(10);

    // Below: two rotary sliders in a row (time, feedback)
    auto controlsArea = area.removeFromTop(200);
    auto numKnobs = 4;
    auto knobWidth = controlsArea.getWidth() / numKnobs;

    auto placeKnob = [](juce::Slider& slider, juce::Label& label, juce::Rectangle<int> r)
        {
            r = r.reduced(10);

            // bajo el knob 50 px
            r.setY(r.getY() + 50);
            slider.setBounds(r);

            // el label 20 px por arriba del knob
            label.setBounds(
                r.getX(),
                r.getY() - 20,
                r.getWidth(),
                20
            );
        };

    juce::Rectangle<int> col;

    col = controlsArea.removeFromLeft(knobWidth);
    placeKnob(mixSlider, mixLabel, col);
    /*
    col = controlsArea.removeFromLeft(knobWidth);
    placeKnob(delayTimeSlider, col);
    */
    col = controlsArea.removeFromLeft(knobWidth);
    placeKnob(depthSlider, depthLabel, col);

    col = controlsArea.removeFromLeft(knobWidth);
    placeKnob(rateSlider, rateLabel, col);

    col = controlsArea.removeFromLeft(knobWidth);
    placeKnob(feedbackSlider, feedbackLabel, col);
}

void MainComponent::chooseAndLoadFile()
{
    auto chooser = std::make_shared<juce::FileChooser>("Select an audio file to play...",
        juce::File(),
        "*.wav;*.aiff;*.mp3;*.flac;*.ogg;*.m4a");
    auto flags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync(flags, [this, chooser](const juce::FileChooser& fc)
        {
            auto url = fc.getURLResult();
            if (url.isEmpty())
                return;

            loadURL(url);
        });
}

void MainComponent::loadURL(const juce::URL& url)
{
    transport.stop();
    transport.setSource(nullptr);
    readerSource.reset();
    auto inputStream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress));

    if (inputStream == nullptr)
        return;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(std::move(inputStream)));
    if (reader == nullptr)
        return;
    const double fileSampleRate = reader->sampleRate;
    readerSource.reset(new juce::AudioFormatReaderSource(reader.release(), true));
    transport.setSource(readerSource.get(), 0, nullptr, fileSampleRate);
    transport.setPosition(0.0);

    setButtonsEnabledState();
}

void MainComponent::setButtonsEnabledState()
{
    const bool hasFile = (readerSource != nullptr);
    const bool isPlaying = transport.isPlaying();

    playButton.setEnabled(hasFile && !isPlaying);
    stopButton.setEnabled(hasFile && isPlaying);
}

// Listen for end-of-stream and other state changes
void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &transport)
    {
        if (!transport.isPlaying() && transport.hasStreamFinished())
            transport.setPosition(0.0);
        setButtonsEnabledState();
    }
}