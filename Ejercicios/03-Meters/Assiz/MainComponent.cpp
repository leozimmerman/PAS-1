#include "MainComponent.h"

//==============================================================================
// MÓDULO: Constructor y Destructor
//==============================================================================

MainComponent::MainComponent()
{
    setSize (800, 600);

    setupGuiComponents();
    setupAudioPlayer();
}

MainComponent::~MainComponent()
{
    // Stop UI timer first to avoid repaint after teardown
    stopTimer();

    // This shuts down the audio device and clears the audio source.
    shutdownAudio();

    // Ensure transport is stopped and reader released before destruction
    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();

    disconnectOsc();
}

//==============================================================================
// MÓDULO: Configuración de Componentes de Interfaz
//==============================================================================

void MainComponent::setupGuiComponents()
{
    // ============================================================================
    // MÓDULO: Configuración de Botones de Control
    // ============================================================================
    // Configura los botones principales para cargar y controlar la reproducción de audio
    
    addAndMakeVisible (loadButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (pauseButton);
    addAndMakeVisible (stopButton);

    // ============================================================================
    // MÓDULO: Configuración de Control de Suavizado
    // ============================================================================
    // El control de suavizado ajusta el factor alpha del filtro exponencial
    // que se aplica a los valores RMS para suavizar la visualización
    
    smoothingLabel.setText ("Smoothing", juce::dontSendNotification);
    smoothingLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (smoothingLabel);

    smoothingSlider.setRange (0.0, 1.0, 0.001);
    
    // Invert mapping: slider shows "smoothing amount", alpha is "snappiness".
    smoothingSlider.setValue (rmsSmoothingAlpha, juce::dontSendNotification);
    smoothingSlider.setTextValueSuffix ("");
    smoothingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    smoothingSlider.onValueChange = [this]
    {
        const double sliderVal = smoothingSlider.getValue();
        rmsSmoothingAlpha = (float) (sliderVal);
    };
    addAndMakeVisible (smoothingSlider);

    // ============================================================================
    // MÓDULO: Configuración de Controles de Umbrales para Segmentación de Colores
    // ============================================================================
    // Configura los sliders para establecer dinámicamente los umbrales de color
    // de los medidores. Los valores se expresan en dBFS (decibeles relativos a 
    // full scale).
    
    greenThresholdLabel.setText ("Green Threshold (dBFS)", juce::dontSendNotification);
    greenThresholdLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (greenThresholdLabel);

    greenThresholdSlider.setRange (-60.0, 0.0, 0.1);
    greenThresholdSlider.setValue (greenThresholdDb, juce::dontSendNotification);
    greenThresholdSlider.setTextValueSuffix (" dBFS");
    greenThresholdSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    greenThresholdSlider.onValueChange = [this]
    {
        greenThresholdDb = (float) greenThresholdSlider.getValue();
        repaint(); // Actualizar visualización cuando cambia el umbral
    };
    addAndMakeVisible (greenThresholdSlider);

    yellowThresholdLabel.setText ("Yellow Threshold (dBFS)", juce::dontSendNotification);
    yellowThresholdLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (yellowThresholdLabel);

    yellowThresholdSlider.setRange (-60.0, 0.0, 0.1);
    yellowThresholdSlider.setValue (yellowThresholdDb, juce::dontSendNotification);
    yellowThresholdSlider.setTextValueSuffix (" dBFS");
    yellowThresholdSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    yellowThresholdSlider.onValueChange = [this]
    {
        yellowThresholdDb = (float) yellowThresholdSlider.getValue();
        repaint(); // Actualizar visualización cuando cambia el umbral
    };
    addAndMakeVisible (yellowThresholdSlider);

    loadButton.onClick = [this] { chooseAndLoadFile(); };
    
    // Botón Play: inicia o reanuda la reproducción desde la posición actual
    playButton.onClick = [this]
    {
        transport.start();
        setButtonsEnabledState();
    };
    
    // Botón Pause: pausa la reproducción manteniendo la posición actual
    pauseButton.onClick = [this]
    {
        transport.stop();
        setButtonsEnabledState();
    };
    
    // Botón Stop: detiene la reproducción y rebobina al inicio
    stopButton.onClick = [this]
    {
        transport.stop();
        transport.setPosition (0.0);  // Rebobinar al inicio
        setButtonsEnabledState();
    };

    // Minimal OSC GUI
    hostLabel.setJustificationType (juce::Justification::centredLeft);
    portLabel.setJustificationType (juce::Justification::centredLeft);
    addrLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (hostLabel);
    addAndMakeVisible (portLabel);
    addAndMakeVisible (addrLabel);

    hostEdit.setText (oscHost, juce::dontSendNotification);
    portEdit.setInputRestrictions (0, "0123456789");
    portEdit.setText (juce::String (oscPort), juce::dontSendNotification);
    addrEdit.setText (oscAddress, juce::dontSendNotification);
    addAndMakeVisible (hostEdit);
    addAndMakeVisible (portEdit);
    addAndMakeVisible (addrEdit);

    oscEnableToggle.onClick = [this] { handleOscEnableToggleClicked(); };
    addAndMakeVisible (oscEnableToggle);

    // Update connection if user edits fields while enabled
    hostEdit.onFocusLost = [this] { reconnectOscIfEnabled(); };
    portEdit.onFocusLost = [this] { reconnectOscIfEnabled(); };
    addrEdit.onFocusLost = [this] { reconnectOscIfEnabled(); };

    setButtonsEnabledState();
}

//==============================================================================
// MÓDULO: Configuración del Sistema de Audio
//==============================================================================

void MainComponent::setupAudioPlayer()
{
    // Registra los formatos de audio básicos soportados por JUCE
    formatManager.registerBasicFormats();
    
    // Configura el sistema de audio: 0 canales de entrada, 2 canales de salida (estéreo)
    setAudioChannels (0, 2);
}

//==============================================================================
// MÓDULO: Preparación del Procesamiento de Audio
//==============================================================================

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // Prepara el transport source para la reproducción
    juce::ignoreUnused (samplesPerBlockExpected);
    transport.prepareToPlay (samplesPerBlockExpected, sampleRate);

    // ============================================================================
    // MÓDULO: Inicialización de Arrays de Medición RMS
    // ============================================================================
    // Prepara los arrays que almacenarán los valores RMS por canal.
    // El tamaño se ajusta según el número de canales de salida del dispositivo de audio.
    
    int numOutChans = 1;
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        numOutChans = juce::jmax (1, dev->getActiveOutputChannels().countNumberOfSetBits());

    {
        // Protección thread-safe para acceso concurrente desde el hilo de audio
        const juce::SpinLock::ScopedLockType sl (rmsLock);
        lastRms.clearQuick();
        smoothedRms.clearQuick();
        peakRms.clearQuick();
        lastRms.insertMultiple (0, 0.0f, numOutChans);
        smoothedRms.insertMultiple (0, 0.0f, numOutChans);
        peakRms.insertMultiple (0, 0.0f, numOutChans);
    }
}

//==============================================================================
// MÓDULO: Procesamiento de Audio en Tiempo Real
//==============================================================================

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    // ============================================================================
    // MÓDULO: Manejo de Estado Sin Fuente de Audio
    // ============================================================================
    // Si no hay archivo cargado, limpia el buffer y resetea los valores RMS
    
    if (readerSource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        // También resetea RMS a cero cuando no hay fuente
        const juce::SpinLock::ScopedLockType sl (rmsLock);
        for (int c = 0; c < smoothedRms.size(); ++c)
        {
            smoothedRms.set (c, 0.0f);
            peakRms.set (c, 0.0f);  // Resetear picos también
        }
        lastRms = smoothedRms;
        return;
    }

    // Obtiene el siguiente bloque de audio del transport source
    transport.getNextAudioBlock (bufferToFill);

    // ============================================================================
    // MÓDULO: Cálculo de RMS (Root Mean Square)
    // ============================================================================
    // Calcula el valor RMS instantáneo para cada canal de audio.
    // RMS = sqrt(sum(samples^2) / numSamples)
    // Este valor representa la energía promedio de la señal.
    
    auto* buffer = bufferToFill.buffer;
    if (buffer == nullptr || bufferToFill.numSamples <= 0)
        return;

    const int numChans = buffer->getNumChannels();
    const int n = bufferToFill.numSamples;
    const int start = bufferToFill.startSample;
    
    // Calcular RMS instantáneo para cada canal
    juce::Array<float> instantRms;
    instantRms.insertMultiple (0, 0.0f, numChans);

    for (int ch = 0; ch < numChans; ++ch)
    {
        const float* data = buffer->getReadPointer (ch, start);

        // Suma de cuadrados de las muestras
        double sumSquares = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float s = data[i];
            sumSquares += (double) s * (double) s;
        }
        
        // Promedio y raíz cuadrada
        auto sumSquaresDiv = sumSquares / (double) n;
        const float rms = std::sqrt ((float) sumSquaresDiv);
        instantRms.set (ch, rms);
    }

    // ============================================================================
    // MÓDULO: Suavizado Exponencial de Valores RMS
    // ============================================================================
    // Aplica un filtro de suavizado exponencial para evitar cambios bruscos
    // en la visualización. La fórmula es: smoothed = (1-alpha) * instant + alpha * previous
    // donde alpha es el factor de suavizado (0 = sin suavizado, 1 = máximo suavizado)
    
    {
        const juce::SpinLock::ScopedLockType sl (rmsLock); // Protección thread-safe
        const float a = juce::jlimit (0.0f, 1.0f, rmsSmoothingAlpha);
        const float b = 1.0f - a;

        for (int ch = 0; ch < smoothedRms.size(); ++ch)
        {
            const float sm = b * instantRms[ch] + a * smoothedRms[ch];
            smoothedRms.set (ch, sm);
            lastRms.set (ch, sm);
            
            // Actualizar pico máximo si el valor actual es mayor
            if (sm > peakRms[ch])
                peakRms.set (ch, sm);
        }
    }
}

//==============================================================================
// MÓDULO: Liberación de Recursos de Audio
//==============================================================================

void MainComponent::releaseResources()
{
    // Libera los recursos del transport source cuando se detiene el audio
    transport.releaseResources();
}

//==============================================================================
// MÓDULO: Funciones Helper para Visualización de Medidores
//==============================================================================

float MainComponent::rmsToDbFs (float rms) const
{
    // Convierte un valor RMS normalizado (0.0 a 1.0) a dBFS
    // dBFS = 20 * log10(rms)
    // Si rms es 0 o muy pequeño, retornamos -infinito (representado como -60 dBFS)
    if (rms <= 0.0f)
        return -60.0f;
    
    return 20.0f * std::log10 (rms);
}

float MainComponent::dbToVisualPos (float db) const
{
    // Convierte un valor dBFS a posición visual (0-1) usando una curva exponencial
    // La función mapea: dBFS → posición visual (0-1)
    // Usa una curva exponencial para distribuir mejor los valores y hacer
    // los segmentos más visibles y proporcionales a los umbrales configurados
    
    const float minDb = -60.0f;  // Rango mínimo de dBFS a mostrar
    
    if (db <= minDb) return 0.0f;
    if (db >= 0.0f) return 1.0f;
    
    // Mapeo exponencial: más sensible en el rango medio-alto
    const float normalized = (db - minDb) / (0.0f - minDb);  // 0 a 1
    // Aplicar curva exponencial para distribuir mejor (exponente < 1 hace la curva más suave)
    return std::pow (normalized, 0.7f);
}

//==============================================================================
// MÓDULO: Renderizado de la Interfaz Gráfica
//==============================================================================

void MainComponent::paint (juce::Graphics& g)
{
    // Fondo del componente
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // ============================================================================
    // MÓDULO: Renderizado de Medidores Horizontales con Segmentación de Colores
    // ============================================================================
    // Dibuja medidores horizontales que muestran el nivel RMS de cada canal.
    // Los medidores están segmentados en tres colores según los umbrales configurados:
    // - Verde: niveles bajos (hasta greenThresholdDb)
    // - Amarillo: niveles medios (hasta yellowThresholdDb)
    // - Rojo: niveles altos (por encima de yellowThresholdDb)
    
    auto bounds = getLocalBounds().reduced (20);
    auto rmsValues = getLatestRms();

    const int numBars = juce::jmax (1, rmsValues.size());
    
    // Calcular área disponible para los medidores (después de los controles)
    // Los medidores se dibujan horizontalmente, uno debajo del otro
    const int meterHeight = 40;  // Altura de cada medidor horizontal
    const int peakTextHeight = 18;  // Altura para el texto del pico máximo
    const int gap = 15;          // Espacio entre medidores
    const int totalMetersHeight = numBars * (meterHeight + peakTextHeight) + (numBars - 1) * gap;
    
    // Obtener el área disponible para medidores (parte inferior de la ventana)
    auto metersArea = bounds.removeFromBottom (totalMetersHeight);
    
    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    
    for (int i = 0; i < numBars; ++i)
    {
        const float rmsValue = juce::jlimit (0.0f, 1.0f, rmsValues[i]);
        const float dbValue = rmsToDbFs (rmsValue);  // Calcular dBFS para mostrar en etiqueta
        
        // Calcular posición y tamaño del medidor horizontal (incluye espacio para texto del pico)
        auto meterRowBounds = metersArea.removeFromTop (meterHeight + peakTextHeight);
        auto meterBounds = meterRowBounds.removeFromTop (meterHeight);
        
        // Dibujar fondo del medidor (gris oscuro)
        g.setColour (juce::Colours::darkgrey);
        g.fillRect (meterBounds);
        
        // ============================================================================
        // MÓDULO: Visualización con Segmentación Dinámica Basada en Umbrales
        // ============================================================================
        // El medidor se divide en tres segmentos cuyo tamaño se ajusta dinámicamente
        // según los umbrales configurados:
        // - Segmento verde: desde -inf hasta greenThresholdDb
        // - Segmento amarillo: desde greenThresholdDb hasta yellowThresholdDb
        // - Segmento rojo: desde yellowThresholdDb hasta 0 dBFS
        //
        // Los tamaños de los segmentos se calculan usando una función de mapeo que
        // convierte los valores dBFS a posiciones visuales proporcionales.
        
        const int totalMeterWidth = meterBounds.getWidth();
        
        // ============================================================================
        // MÓDULO: Cálculo de Tamaños de Segmentos Dinámicos
        // ============================================================================
        // Calcular posiciones visuales de los umbrales usando función de mapeo
        // que convierte valores dBFS a posiciones en el medidor (0-1)
        
        const float greenVisualPos = dbToVisualPos (greenThresholdDb);
        const float yellowVisualPos = dbToVisualPos (yellowThresholdDb);
        
        // Calcular anchos de cada segmento en píxeles
        const int greenWidth = juce::roundToInt ((float) totalMeterWidth * greenVisualPos);
        const int yellowWidth = juce::roundToInt ((float) totalMeterWidth * yellowVisualPos);
        
        // Dibujar los segmentos de fondo (marcadores visuales)
        // Esto ayuda a visualizar dónde están los umbrales
        auto backgroundMeter = meterBounds;
        
        // Segmento verde (fondo)
        if (greenWidth > 0)
        {
            auto greenBg = backgroundMeter.removeFromLeft (greenWidth);
            g.setColour (juce::Colours::green.withAlpha (0.2f));
            g.fillRect (greenBg);
        }
        
        // Segmento amarillo (fondo)
        const int yellowSegmentWidth = yellowWidth - greenWidth;
        if (yellowSegmentWidth > 0)
        {
            auto yellowBg = backgroundMeter.removeFromLeft (yellowSegmentWidth);
            g.setColour (juce::Colours::yellow.withAlpha (0.2f));
            g.fillRect (yellowBg);
        }
        
        // Segmento rojo (fondo)
        if (backgroundMeter.getWidth() > 0)
        {
            g.setColour (juce::Colours::red.withAlpha (0.2f));
            g.fillRect (backgroundMeter);
        }
        
        // ============================================================================
        // MÓDULO: Mapeo del Valor RMS Actual a Posición Visual
        // ============================================================================
        // Mapear el valor RMS actual a una posición en el medidor usando la misma
        // función de mapeo que usamos para los umbrales
        
        int meterWidth = 0;
        
        if (rmsValue > 0.0f)
        {
            // Calcular posición visual del valor actual
            const float visualPosition = dbToVisualPos (dbValue);
            meterWidth = juce::roundToInt ((float) totalMeterWidth * visualPosition);
        }
        
        // Dibujar el medidor actual con los colores apropiados según los segmentos
        if (meterWidth > 0)
        {
            auto currentMeter = meterBounds.withWidth (meterWidth);
            
            // Determinar qué segmentos están activos y dibujarlos
            if (meterWidth <= greenWidth)
            {
                // Solo segmento verde
                g.setColour (juce::Colours::green);
                g.fillRect (currentMeter);
            }
            else if (meterWidth <= yellowWidth)
            {
                // Segmento verde completo + parte del amarillo
                auto greenSegment = currentMeter.removeFromLeft (greenWidth);
                g.setColour (juce::Colours::green);
                g.fillRect (greenSegment);
                
                if (currentMeter.getWidth() > 0)
                {
                    g.setColour (juce::Colours::yellow);
                    g.fillRect (currentMeter);
                }
            }
            else
            {
                // Segmento verde completo + amarillo completo + parte del rojo
                auto greenSegment = currentMeter.removeFromLeft (greenWidth);
                g.setColour (juce::Colours::green);
                g.fillRect (greenSegment);
                
                const int remainingAfterGreen = currentMeter.getWidth();
                if (remainingAfterGreen > 0)
                {
                    const int yellowSegmentDrawWidth = juce::jmin (yellowSegmentWidth, remainingAfterGreen);
                    auto yellowSegment = currentMeter.removeFromLeft (yellowSegmentDrawWidth);
                    g.setColour (juce::Colours::yellow);
                    g.fillRect (yellowSegment);
                    
                    if (currentMeter.getWidth() > 0)
                    {
                        g.setColour (juce::Colours::red);
                        g.fillRect (currentMeter);
                    }
                }
            }
        }
        
        // Obtener picos máximos para mostrar
        auto peakValues = getPeakRms();
        const float peakValue = (i < peakValues.size()) ? peakValues[i] : 0.0f;
        const float peakDbValue = rmsToDbFs (peakValue);
        
        // Dibujar etiqueta con el valor actual en dBFS
        juce::String labelText = juce::String (dbValue, 1) + " dBFS";
        auto labelBounds = meterBounds.removeFromRight (100);
        
        g.setColour (juce::Colours::white);
        g.drawFittedText (labelText, labelBounds, juce::Justification::centredLeft, 1);
        
        // Etiqueta del canal
        juce::String channelLabel = "Ch " + juce::String (i + 1);
        auto channelLabelBounds = meterBounds.removeFromLeft (50);
        g.drawFittedText (channelLabel, channelLabelBounds, juce::Justification::centredLeft, 1);
        
        // Dibujar texto del pico máximo debajo del medidor
        if (peakValue > 0.0f)
        {
            // Usar CharPointer_UTF8 para manejar correctamente caracteres no-ASCII (á en "máximo")
            juce::String peakText = juce::String (juce::CharPointer_UTF8 (u8"Pico máximo: ")) 
                                  + juce::String (peakDbValue, 1) 
                                  + juce::String (" dBFS");
            auto peakLabelBounds = meterRowBounds;  // Usar el área reservada para el texto
            g.setColour (juce::Colours::lightgrey);
            g.drawFittedText (peakText, peakLabelBounds, juce::Justification::centredLeft, 1);
        }
        
        // Remover espacio entre medidores
        if (i < numBars - 1)
            metersArea.removeFromTop (gap);
    }
}

//==============================================================================
// MÓDULO: Gestión del Layout y Posicionamiento de Componentes
//==============================================================================

void MainComponent::resized()
{
    // ============================================================================
    // MÓDULO: Distribución de Componentes en la Interfaz
    // ============================================================================
    // Organiza todos los componentes de la interfaz en filas horizontales.
    // El layout se organiza de arriba hacia abajo:
    // 1. Botones de control de reproducción
    // 2. Control de suavizado (smoothing)
    // 3. Controles de umbrales de color
    // 4. Configuración OSC
    // 5. Medidores (se dibujan en paint, no se posicionan aquí)
    
    auto area = getLocalBounds().reduced (20);
    auto buttonHeight = 32;
    
    // Fila 1: Botones de control de reproducción
    auto row = area.removeFromTop (buttonHeight);
    loadButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (10);
    playButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (10);
    pauseButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (10);
    stopButton.setBounds (row.removeFromLeft (120));

    // Fila 2: Control de suavizado (smoothing)
    auto controlRow = area.removeFromTop (28);
    smoothingLabel.setBounds (controlRow.removeFromLeft (100));
    controlRow.removeFromLeft (8);
    smoothingSlider.setBounds (controlRow.removeFromLeft (juce::jmax (200, controlRow.getWidth() / 2)));

    // Fila 3: Controles de umbrales de color para los medidores
    auto thresholdRow1 = area.removeFromTop (28);
    greenThresholdLabel.setBounds (thresholdRow1.removeFromLeft (180));
    thresholdRow1.removeFromLeft (8);
    greenThresholdSlider.setBounds (thresholdRow1.removeFromLeft (200));
    
    auto thresholdRow2 = area.removeFromTop (28);
    yellowThresholdLabel.setBounds (thresholdRow2.removeFromLeft (180));
    thresholdRow2.removeFromLeft (8);
    yellowThresholdSlider.setBounds (thresholdRow2.removeFromLeft (200));

    // Fila 4: Configuración OSC
    auto oscRow1 = area.removeFromTop (26);
    hostLabel.setBounds (oscRow1.removeFromLeft (50));
    oscRow1.removeFromLeft (6);
    hostEdit.setBounds (oscRow1.removeFromLeft (160));
    oscRow1.removeFromLeft (12);
    portLabel.setBounds (oscRow1.removeFromLeft (40));
    oscRow1.removeFromLeft (6);
    portEdit.setBounds (oscRow1.removeFromLeft (80));
    oscRow1.removeFromLeft (12);
    addrLabel.setBounds (oscRow1.removeFromLeft (70));
    oscRow1.removeFromLeft (6);
    addrEdit.setBounds (oscRow1.removeFromLeft (180));
    oscRow1.removeFromLeft (12);
    oscEnableToggle.setBounds (oscRow1.removeFromLeft (120));
    
    // El área restante se usa para los medidores horizontales (dibujados en paint)
}

//==============================================================================
// MÓDULO: Manejo de Eventos de Interfaz
//==============================================================================

void MainComponent::buttonClicked (juce::Button* button)
{
    // Maneja los clics en los botones de control
    if (button == &loadButton) chooseAndLoadFile();
    if (button == &playButton) { transport.start(); setButtonsEnabledState(); }
    if (button == &pauseButton) { transport.stop(); setButtonsEnabledState(); }
    if (button == &stopButton) { transport.stop(); transport.setPosition (0.0); setButtonsEnabledState(); }
}

//==============================================================================
// MÓDULO: Gestión del Estado de los Botones y Timer
//==============================================================================

void MainComponent::setButtonsEnabledState()
{
    // Actualiza el estado habilitado/deshabilitado de los botones según el estado actual
    const bool hasFile = (readerSource != nullptr);
    const bool isPlaying = transport.isPlaying();

    // Play: habilitado cuando hay archivo y no está reproduciendo
    playButton.setEnabled (hasFile && !isPlaying);
    
    // Pause: habilitado cuando hay archivo y está reproduciendo
    pauseButton.setEnabled (hasFile && isPlaying);
    
    // Stop: habilitado cuando hay archivo (siempre disponible para rebobinar)
    stopButton.setEnabled (hasFile);
    
    // Controla el timer de actualización de UI: activo durante la reproducción
    if (isPlaying)
    {
        if (! isTimerRunning())
            startTimerHz (30); // Actualiza la UI a 30 Hz durante la reproducción
    }
    else
    {
        stopTimer();
        // Última actualización para mostrar medidores en cero cuando se detiene
        repaint();
    }
}

//==============================================================================
// MÓDULO: Acceso Thread-Safe a Valores RMS
//==============================================================================

juce::Array<float> MainComponent::getLatestRms() const
{
    // Retorna una copia thread-safe de los últimos valores RMS calculados
    // Este método puede ser llamado desde el hilo de UI de forma segura
    const juce::SpinLock::ScopedLockType sl (rmsLock);
    return lastRms; // Retorna una copia
}

juce::Array<float> MainComponent::getPeakRms() const
{
    // Retorna una copia thread-safe de los picos máximos RMS alcanzados
    // Este método puede ser llamado desde el hilo de UI de forma segura
    const juce::SpinLock::ScopedLockType sl (rmsLock);
    return peakRms; // Retorna una copia
}

void MainComponent::resetPeakRms()
{
    // Resetea los picos máximos RMS para todos los canales
    const juce::SpinLock::ScopedLockType sl (rmsLock);
    for (int ch = 0; ch < peakRms.size(); ++ch)
        peakRms.set (ch, 0.0f);
}

//==============================================================================
// MÓDULO: Callback del Timer para Actualización de UI
//==============================================================================

void MainComponent::timerCallback()
{
    // ============================================================================
    // MÓDULO: Detección de Fin de Reproducción
    // ============================================================================
    // Verifica si la reproducción se detuvo (por ejemplo, al llegar al final del archivo)
    // y rebobina automáticamente para permitir reproducir nuevamente
    
    if (! transport.isPlaying())
    {
        // Si llegamos al final del archivo, rebobinar para que Play funcione nuevamente
        const double len = transport.getLengthInSeconds();
        if (len > 0.0 && transport.getCurrentPosition() >= len - 1e-6)
            transport.setPosition (0.0);

        setButtonsEnabledState(); // Esto detendrá el timer y actualizará la UI
        return;
    }

    // Actualizar visualización de medidores
    repaint();

    // ============================================================================
    // MÓDULO: Envío de Datos OSC
    // ============================================================================
    // Si OSC está habilitado y conectado, envía los valores RMS actuales
    if (oscEnableToggle.getToggleState() && oscConnected)
    {
        auto values = getLatestRms();
        sendRmsOverOsc (values);
    }
}

//==============================================================================
// MÓDULO: Carga de Archivos de Audio
//==============================================================================

void MainComponent::chooseAndLoadFile()
{
    // ============================================================================
    // MÓDULO: Selector de Archivos Asíncrono
    // ============================================================================
    // Abre un diálogo de selección de archivos de forma asíncrona para mantener
    // la interfaz responsiva durante la operación
    
    auto chooser = std::make_shared<juce::FileChooser> ("Select an audio file to play...",
                                                         juce::File(),
                                                         "*.wav;*.aiff;*.mp3;*.flac;*.ogg;*.m4a");
    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
    {
        auto url = fc.getURLResult(); // Funciona para archivos locales y URLs sandboxed (iOS/macOS)
        if (url.isEmpty())
            return;

        loadURL (url);
    });
}

void MainComponent::loadURL (const juce::URL& url)
{
    // ============================================================================
    // MÓDULO: Carga y Configuración de Archivo de Audio
    // ============================================================================
    // Carga un archivo de audio desde una URL y lo prepara para la reproducción.
    // El proceso incluye:
    // 1. Detener la reproducción actual
    // 2. Crear un lector de formato de audio
    // 3. Crear una fuente de audio desde el lector
    // 4. Configurar el transport source con la nueva fuente
    
    // Detener reproducción actual y desvincular fuente actual
    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();

    // Crear stream de entrada desde la URL
    auto inputStream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress));
    
    if (inputStream == nullptr)
        return;
    
    // Crear AudioFormatReader: lee muestras desde un stream de archivo de audio
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (std::move (inputStream)));
    if (reader == nullptr)
        return;

    // Capturar el sample rate del archivo antes de transferir la propiedad
    const double fileSampleRate = reader->sampleRate;

    // Crear la fuente de lector (toma propiedad del reader)
    readerSource.reset (new juce::AudioFormatReaderSource (reader.release(), true));

    // Configurar la fuente; pasar el sample rate del archivo para que Transport pueda resamplear si es necesario
    transport.setSource (readerSource.get(), 0, nullptr, fileSampleRate);

    // Resetear posición al inicio
    transport.setPosition (0.0);

    // Actualizar estado de los botones
    setButtonsEnabledState();
}

//==============================================================================
// MÓDULO: Comunicación OSC (Open Sound Control)
//==============================================================================

void MainComponent::updateOscConnection()
{
    // ============================================================================
    // MÓDULO: Actualización de Conexión OSC
    // ============================================================================
    // Actualiza la conexión OSC con los parámetros actuales.
    // Si ya hay una conexión activa, la desconecta primero.
    
    // Si está conectado, desconectar primero
    if (oscConnected)
    {
        disconnectOsc();
    }

    // Validar valores de configuración necesarios
    if (oscHost.isEmpty() || oscPort <= 0)
    {
        oscConnected = false;
        return;
    }

    // Intentar conectar
    oscConnected = oscSender.connect (oscHost, oscPort);
}

void MainComponent::disconnectOsc()
{
    // Desconecta el sender OSC si está conectado
    if (oscConnected)
    {
        oscSender.disconnect();
        oscConnected = false;
    }
}

void MainComponent::sendRmsOverOsc (const juce::Array<float>& values)
{
    // ============================================================================
    // MÓDULO: Envío de Valores RMS vía OSC
    // ============================================================================
    // Envía los valores RMS actuales como un mensaje OSC.
    // Formato del mensaje: /address <float ch0> <float ch1> ...
    
    if (! oscConnected)
        return;

    // Construir mensaje: /address <float ch0> <float ch1> ...
    juce::OSCMessage msg (oscAddress.isEmpty() ? "/rms" : oscAddress);

    // Agregar cada valor RMS como un float32
    for (auto v : values)
        msg.addFloat32 (juce::jlimit (0.0f, 1.0f, v));

    // Envío de mejor esfuerzo; ignorar fallos aquí
    (void) oscSender.send (msg);
}

void MainComponent::reconnectOscIfEnabled()
{
    // ============================================================================
    // MÓDULO: Reconexión Automática OSC
    // ============================================================================
    // Si OSC está habilitado, actualiza los parámetros desde los campos de texto
    // y reconecta. Se llama cuando el usuario edita los campos mientras OSC está activo.
    
    if (oscEnableToggle.getToggleState())
    {
        oscHost = hostEdit.getText().trim();
        oscPort = portEdit.getText().getIntValue();
        oscAddress = addrEdit.getText().trim();
        if (oscAddress.isEmpty())
            oscAddress = "/rms";

        updateOscConnection();
    }
}

void MainComponent::handleOscEnableToggleClicked()
{
    // ============================================================================
    // MÓDULO: Manejo del Toggle de Habilitación OSC
    // ============================================================================
    // Maneja el clic en el botón de habilitación OSC.
    // Si se habilita, lee los parámetros desde los campos y conecta.
    // Si se deshabilita, desconecta.
    
    if (oscEnableToggle.getToggleState())
    {
        // Leer configuración más reciente desde los campos y conectar
        oscHost = hostEdit.getText().trim();
        oscPort = portEdit.getText().getIntValue();
        oscAddress = addrEdit.getText().trim();
        if (oscAddress.isEmpty()) // valor de dirección por defecto
            oscAddress = "/rms";

        updateOscConnection();

        // Si la conexión falló, desactivar el toggle
        if (! oscConnected)
            oscEnableToggle.setToggleState (false, juce::dontSendNotification);
    }
    else
    {
        disconnectOsc();
    }
}

