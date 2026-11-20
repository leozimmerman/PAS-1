#include "MainComponent.h"

MainComponent::MainComponent()
{
    // ==== Tablas ====
    // No dependen del sample rate: se llenan una sola vez, al crear la ventana.
    fillTable (sineTable,     sine);
    fillTable (sawTable,      saw);
    fillTable (squareTable,   square);
    fillTable (triangleTable, triangle);

    // ==== Controles ====
    playButton.onClick = [this] { playing = playButton.getToggleState(); };
    addAndMakeVisible (playButton);

    // Los ids del ComboBox empiezan en 1: el id menos 1 es la opción del enum (sine = 0, saw = 1...).
    waveformBox.addItem ("Seno", 1);
    waveformBox.addItem ("Sierra", 2);
    waveformBox.addItem ("Cuadrada", 3);
    waveformBox.addItem ("Triangular", 4);
    waveformBox.addItem ("Ruido", 5);
    waveformBox.setSelectedId (1);
    waveformBox.onChange = [this] { waveform = waveformBox.getSelectedId() - 1; };
    addAndMakeVisible (waveformBox);
    addAndMakeVisible (waveformLabel);

    modeBox.addItem ("Formula", 1);
    modeBox.addItem ("Tabla", 2);
    modeBox.setSelectedId (1);
    modeBox.onChange = [this] { mode = modeBox.getSelectedId() - 1; };
    addAndMakeVisible (modeBox);
    addAndMakeVisible (modeLabel);

    interpolateButton.setToggleState (true, juce::dontSendNotification);
    interpolateButton.onClick = [this] { interpolate = interpolateButton.getToggleState(); };
    addAndMakeVisible (interpolateButton);

    frequencySlider.setRange (20.0, 10000.0, 1.0);
    frequencySlider.setSkewFactorFromMidPoint (440.0); // más recorrido para las frecuencias graves
    frequencySlider.setValue (frequency);
    frequencySlider.setTextValueSuffix (" Hz");
    frequencySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    frequencySlider.onValueChange = [this] { frequency = (float) frequencySlider.getValue(); };
    addAndMakeVisible (frequencySlider);
    addAndMakeVisible (frequencyLabel);

    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.setValue (volume);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    volumeSlider.onValueChange = [this] { volume = (float) volumeSlider.getValue(); };
    addAndMakeVisible (volumeSlider);
    addAndMakeVisible (volumeLabel);

    setSize (600, 300);
    setAudioChannels (0, 2); // 0 entradas, 2 salidas (estéreo)
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

//==============================================================================
// Audio

void MainComponent::prepareToPlay (int, double sampleRate)
{
    sr = sampleRate;
    phase = 0.0f;
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* buffer = bufferToFill.buffer;

    // Leemos los parámetros una vez por bloque.
    int   shape = waveform;
    int   how   = mode;
    float f     = frequency;
    float vol   = playing ? volume.load() : 0.0f; // apagado = silencio

    for (int i = 0; i < bufferToFill.numSamples; ++i)
    {
        // ==== 1. Obtener la muestra: con la fórmula o leyendo la tabla ====
        float sample = 0.0f;

        switch (how)
        {
            case formula:
                sample = formulaSample (shape, phase); break;
            case table:
                sample = tableSample (shape, phase);   break;
        }

        // ==== 2. Escribirla en todos los canales ====
        int index = bufferToFill.startSample + i;
        for (int channel = 0; channel < buffer->getNumChannels(); ++channel)
            buffer->setSample (channel, index, sample * vol);

        // ==== 3. Avanzar la fase (acumulador de fase) ====
        // La fase recorre el ciclo de 0 a 1. En cada muestra avanza  frecuencia / sr:
        //   con f = 441 Hz y sr = 44100, avanza 0.01 por muestra -> 100 muestras por ciclo
        //   -> 441 ciclos por segundo.
        // Al llegar a 1 vuelve a 0 (empieza otro ciclo).
        //   https://en.wikipedia.org/wiki/Numerically_controlled_oscillator  (phase accumulator)
        phase += f / (float) sr;
        if (phase >= 1.0f)
            phase -= 1.0f;
    }
}

void MainComponent::releaseResources()
{
}

//==============================================================================
// ==== 1. Fórmula: calcular cada muestra ====
//
// Para cada muestra evaluamos la fórmula de la forma de onda.
//   - Es exacta y no usa memoria.
//   - Pero se calcula 44100 veces por segundo, por cada oscilador. Algunas fórmulas son
//     caras: std::sin cuesta bastante más que una suma, y una forma hecha de muchos
//     armónicos (síntesis aditiva) tendría que sumar decenas de senos por muestra.
//
// Todas reciben la fase (de 0 a 1) y devuelven una muestra entre -1 y 1. Un ciclo de cada una:
//
//   Seno             Sierra           Cuadrada         Triangular
//      .--.               /|           +----+           \      /
//     /    \             / |           |    |            \    /
//           \    /      /  |  /             |    |        \  /
//            '--'      /   | /              +----+         \/
//
// Aliasing: la sierra y la cuadrada tienen saltos bruscos, y un salto tiene armónicos
// infinitos. Los que pasan de sr/2 (la frecuencia de Nyquist) no se pueden representar y
// "rebotan" hacia abajo como frecuencias que no tendrían que estar. Probá la sierra y subí
// la frecuencia: en las frecuencias altas (de 3000 Hz para arriba) se escuchan tonos que
// bajan mientras el slider sube. Leer de una tabla no lo arregla: la tabla tiene la misma forma.
// El seno no tiene armónicos, así que no tiene aliasing.
//   https://en.wikipedia.org/wiki/Aliasing

float MainComponent::formulaSample (int shape, float phaseValue)
{
    switch (shape)
    {
        case sine:      // sin (2 * pi * fase)                  https://en.wikipedia.org/wiki/Sine_wave
            return std::sin (juce::MathConstants<float>::twoPi * phaseValue);

        case saw:       // sube en línea recta de -1 a 1        https://en.wikipedia.org/wiki/Sawtooth_wave
            return 2.0f * phaseValue - 1.0f;

        case square:    // 1 la primera mitad, -1 la segunda    https://en.wikipedia.org/wiki/Square_wave_(waveform)
            return phaseValue < 0.5f ? 1.0f : -1.0f;

        case triangle:  // baja de 1 a -1 y vuelve a subir      https://en.wikipedia.org/wiki/Triangle_wave
            return 4.0f * std::abs (phaseValue - 0.5f) - 1.0f;

        case noise:     // ruido blanco: un número al azar entre -1 y 1 (no depende de la fase)
            return random.nextFloat() * 2.0f - 1.0f;
    }

    return 0.0f;
}

//==============================================================================
// ==== 2. Tabla: leer muestras ya calculadas ====
//
// Calculamos UN ciclo una sola vez (con las mismas fórmulas de arriba), lo guardamos en
// memoria y después solo lo leemos.
//   - Leer es barato: dos lecturas, una resta, una multiplicación y una suma.
//   - Usa memoria: 2048 floats = 8 KB por forma de onda.
//   - La tabla puede tener CUALQUIER forma: una suma de armónicos, una forma dibujada a mano,
//     un ciclo de un sonido grabado. Leerla cuesta lo mismo sea cual sea la forma.
//     Es la base de la síntesis por tablas de onda (wavetable).
//     https://en.wikipedia.org/wiki/Wavetable_synthesis
//
// Precisión: la tabla solo tiene tableSize valores, y la fase casi nunca cae justo en uno.
//   Con fase = 0.3 y 2048 valores, la posición es 0.3 * 2048 = 614.4: entre el 614 y el 615.
//
//   - Sin interpolar: usamos el 614 y listo. La forma queda en escalones, y eso suena
//     como ruido o distorsión agregados. Con una tabla chica (16 o 64 valores) se escucha clarito.
//   - Interpolando: unimos el 614 y el 615 con una recta y tomamos el punto que está al 40%
//     (el .4) del camino. Con 2048 valores y interpolación, no se distingue de la fórmula.
//
//            tabla[615] *
//                      /
//      resultado ->  o        <- 40% del camino entre los dos
//                   /
//     tabla[614]  *
//                614  614.4  615
//
// Más tabla = más precisión, pero más memoria. Interpolar mejora mucho por muy poco cálculo.

// Llena una tabla con un ciclo de la forma de onda: la fórmula, calculada de antemano.
void MainComponent::fillTable (std::vector<float>& tableToFill, int shape)
{
    tableToFill.resize (tableSize);

    for (int i = 0; i < tableSize; ++i)
    {
        float phaseValue = (float) i / (float) tableSize; // de 0 a casi 1
        tableToFill[i] = formulaSample (shape, phaseValue);
    }
}

// Elige la tabla de la forma de onda y la lee.
float MainComponent::tableSample (int shape, float phaseValue)
{
    switch (shape)
    {
        case sine:
            return readTable (sineTable, phaseValue);
        case saw:
            return readTable (sawTable, phaseValue);
        case square:
            return readTable (squareTable, phaseValue);
        case triangle:
            return readTable (triangleTable, phaseValue);

        // El ruido no tiene tabla: un ciclo guardado se repetiría y dejaría de ser al azar.
        case noise:
            return formulaSample (noise, phaseValue);
    }

    return 0.0f;
}

// Lee la tabla en la posición que corresponde a la fase, con o sin interpolación.
float MainComponent::readTable (const std::vector<float>& tableToRead, float phaseValue)
{
    float position = phaseValue * tableSize;  // por ejemplo 614.4
    int index0 = (int) position;              // 614
    int index1 = index0 + 1;                  // 615
    if (index1 >= tableSize)
        index1 = 0;                           // después del último viene el primero (es un ciclo)

    if (! interpolate)
        return tableToRead[index0];           // sin interpolar: el valor de abajo tableToRead[614]

    // Interpolación lineal: y = y0 + frac * (y1 - y0)
    //   https://en.wikipedia.org/wiki/Linear_interpolation
    float frac = position - (float) index0;   // 0.4
    float y0 = tableToRead[index0];
    float y1 = tableToRead[index1];

    // Ejemplo: y0 = 0.0, y1 = 1.0, frac = 0.4
    //   0.0 + 0.4 * (1.0 - 0.0) = 0.4   -> el 40% del camino entre 0.0 y 1.0
    return y0 + frac * (y1 - y0);
}

//==============================================================================
// Interfaz gráfica

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (20);

    playButton.setBounds (area.removeFromTop (30).removeFromLeft (100));
    area.removeFromTop (10);

    auto waveformRow = area.removeFromTop (30);
    waveformLabel.setBounds (waveformRow.removeFromLeft (100));
    waveformBox.setBounds (waveformRow.removeFromLeft (150));
    area.removeFromTop (10);

    auto modeRow = area.removeFromTop (30);
    modeLabel.setBounds (modeRow.removeFromLeft (100));
    modeBox.setBounds (modeRow.removeFromLeft (150));
    modeRow.removeFromLeft (20);
    interpolateButton.setBounds (modeRow.removeFromLeft (120));
    area.removeFromTop (10);

    auto frequencyRow = area.removeFromTop (30);
    frequencyLabel.setBounds (frequencyRow.removeFromLeft (100));
    frequencySlider.setBounds (frequencyRow);
    area.removeFromTop (10);

    auto volumeRow = area.removeFromTop (30);
    volumeLabel.setBounds (volumeRow.removeFromLeft (100));
    volumeSlider.setBounds (volumeRow);
}
