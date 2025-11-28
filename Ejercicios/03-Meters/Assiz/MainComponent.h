#pragma once

#include <JuceHeader.h>

// PROJUCER needs to add juce_osc

//==============================================================================
// MÓDULO: Componente Principal de la Aplicación
//==============================================================================
// MainComponent es el componente principal que gestiona:
// - Reproducción de archivos de audio
// - Cálculo y visualización de valores RMS (Root Mean Square)
// - Medidores horizontales con segmentación de colores (verde, amarillo, rojo)
// - Comunicación OSC para envío de datos RMS
// - Controles dinámicos de umbrales para la segmentación de colores
//==============================================================================

class MainComponent  : public juce::AudioAppComponent,
                       private juce::Button::Listener,
                       private juce::Timer
{
public:
    //==============================================================================
    // MÓDULO: Constructor y Destructor
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    // MÓDULO: Callbacks del Sistema de Audio
    //==============================================================================
    // Métodos requeridos por AudioAppComponent para el procesamiento de audio
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    // MÓDULO: Callbacks de Interfaz Gráfica
    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    
    //==============================================================================
    // MÓDULO: Configuración Inicial
    //==============================================================================
    void setupGuiComponents();  // Configura todos los componentes de la interfaz
    void setupAudioPlayer();    // Configura el sistema de audio

    //==============================================================================
    // MÓDULO: Acceso a Valores RMS
    //==============================================================================
    // Retorna los últimos valores RMS calculados (por canal).
    // Thread-safe: puede ser llamado desde el hilo de UI de forma segura.
    juce::Array<float> getLatestRms() const;
    
    // Retorna los picos máximos RMS alcanzados (por canal).
    // Thread-safe: puede ser llamado desde el hilo de UI de forma segura.
    juce::Array<float> getPeakRms() const;
    
    // Resetea los picos máximos RMS para todos los canales.
    void resetPeakRms();

private:
    //==============================================================================
    // MÓDULO: Miembros de Reproducción de Audio
    //==============================================================================
    juce::AudioFormatManager formatManager;              // Gestiona formatos de audio soportados
    juce::AudioTransportSource transport;                // Controla la reproducción de audio
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;  // Fuente de audio desde archivo

    //==============================================================================
    // MÓDULO: Componentes de Interfaz de Usuario - Botones
    //==============================================================================
    juce::TextButton loadButton { "Load..." };  // Botón para cargar archivo de audio
    juce::TextButton playButton { "Play" };     // Botón para iniciar reproducción
    juce::TextButton pauseButton { "Pause" };   // Botón para pausar reproducción
    juce::TextButton stopButton { "Stop" };     // Botón para detener y rebobinar al inicio

    //==============================================================================
    // MÓDULO: Componentes de Interfaz - Control de Suavizado
    //==============================================================================
    juce::Slider smoothingSlider;  // Slider para ajustar el factor de suavizado RMS
    juce::Label  smoothingLabel;  // Etiqueta del control de suavizado

    //==============================================================================
    // MÓDULO: Componentes de Interfaz - Controles de Umbrales de Color
    //==============================================================================
    // Sliders y labels para establecer dinámicamente los umbrales de segmentación
    // de colores en los medidores. Los valores se expresan en dBFS.
    juce::Slider greenThresholdSlider;   // Umbral para segmento verde (default: -12 dBFS)
    juce::Label  greenThresholdLabel;
    juce::Slider yellowThresholdSlider;   // Umbral para segmento amarillo (default: -6 dBFS)
    juce::Label  yellowThresholdLabel;

    //==============================================================================
    // MÓDULO: Componentes de Interfaz - Configuración OSC
    //==============================================================================
    juce::Label hostLabel { {}, "Host" };
    juce::TextEditor hostEdit;           // Campo para dirección IP/host OSC
    juce::Label portLabel { {}, "Port" };
    juce::TextEditor portEdit;          // Campo para puerto OSC
    juce::Label addrLabel { {}, "Address" };
    juce::TextEditor addrEdit;          // Campo para dirección OSC
    juce::ToggleButton oscEnableToggle { "Send OSC" };  // Toggle para habilitar/deshabilitar OSC

    //==============================================================================
    // MÓDULO: Componentes de Comunicación OSC
    //==============================================================================
    juce::OSCSender oscSender;          // Sender para enviar mensajes OSC
    juce::String oscHost { "127.0.0.1" };  // Host destino (default: localhost)
    int oscPort { 9000 };                // Puerto destino (default: 9000)
    juce::String oscAddress { "/rms" };  // Dirección OSC (default: /rms)
    bool oscConnected { false };         // Estado de conexión OSC

    //==============================================================================
    // MÓDULO: Callbacks de Eventos
    //==============================================================================
    void buttonClicked (juce::Button* button) override;  // Callback para clics en botones
    void timerCallback() override;                       // Callback del timer para actualizar UI

    //==============================================================================
    // MÓDULO: Funciones Helper - Carga de Archivos
    //==============================================================================
    void chooseAndLoadFile();        // Abre diálogo para seleccionar archivo
    void loadURL (const juce::URL& url);  // Carga archivo desde URL
    void setButtonsEnabledState();   // Actualiza estado de botones según contexto

    //==============================================================================
    // MÓDULO: Funciones Helper - Visualización de Medidores
    //==============================================================================
    float rmsToDbFs (float rms) const;  // Convierte valor RMS normalizado (0-1) a dBFS
    float dbToVisualPos (float db) const;  // Convierte valor dBFS a posición visual (0-1) usando curva exponencial

    //==============================================================================
    // MÓDULO: Funciones Helper - Comunicación OSC
    //==============================================================================
    void updateOscConnection();           // Actualiza conexión OSC con parámetros actuales
    void disconnectOsc();                 // Desconecta el sender OSC
    void sendRmsOverOsc (const juce::Array<float>& values);  // Envía valores RMS vía OSC
    void reconnectOscIfEnabled();        // Reconecta OSC si está habilitado
    void handleOscEnableToggleClicked();  // Maneja clic en toggle de OSC

    //==============================================================================
    // MÓDULO: Medición RMS - Almacenamiento Thread-Safe
    //==============================================================================
    // Los valores RMS se calculan en el hilo de audio y se leen desde el hilo de UI.
    // Se utiliza un SpinLock para proteger el acceso concurrente.
    mutable juce::SpinLock rmsLock;       // Lock para acceso thread-safe
    juce::Array<float> lastRms;          // Últimos valores RMS calculados (copia para UI)
    juce::Array<float> smoothedRms;      // Valores RMS suavizados (usado en cálculo)
    float rmsSmoothingAlpha = 0.2f;       // Factor de suavizado (0 = sin suavizado, 1 = máximo)
    
    // Picos máximos por canal (valores RMS máximos alcanzados)
    juce::Array<float> peakRms;          // Picos máximos RMS por canal (thread-safe con rmsLock)
    
    //==============================================================================
    // MÓDULO: Configuración de Umbrales de Segmentación de Colores
    //==============================================================================
    // Valores de umbral en dBFS para la segmentación de colores en los medidores:
    // - Verde: desde -inf hasta greenThresholdDb
    // - Amarillo: desde greenThresholdDb hasta yellowThresholdDb
    // - Rojo: desde yellowThresholdDb hasta 0 dBFS
    float greenThresholdDb = -12.0f;   // Umbral verde (default: -12 dBFS)
    float yellowThresholdDb = -6.0f;    // Umbral amarillo (default: -6 dBFS)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
