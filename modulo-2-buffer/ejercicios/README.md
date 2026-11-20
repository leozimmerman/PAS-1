# TP 2 · Una app que analiza o procesa el buffer

## Objetivo

Diseñar y programar **tu propia app** que trabaje con las muestras del buffer de audio: que las **analice**
(mida algo de la señal y lo muestre), que las **procese** (las modifique para cambiar el sonido), o las dos cosas.

La app es libre. Lo único que pedimos es que quede dentro de lo que vimos en el módulo.

## Punto de partida

Copiá la carpeta [`../ejemplos/01-AudioFilePlayer`](../ejemplos/01-AudioFilePlayer) a una carpeta **fuera de este repo**,
en tu propio repositorio de Git, y cambiale el nombre al proyecto:

- **Projucer:** abrí el `.jucer` y cambiá el *Project Name*.
- **CMake:** en el `CMakeLists.txt`, reemplazá `AudioFilePlayer` por el nombre de tu app.

El ejemplo ya trae un **`.gitignore`**, para que Git no guarde lo que se genera al compilar (`Builds/`, `build/`...).
Es un archivo oculto: copiá la carpeta entera, no solo los archivos que ves (en Mac, `Cmd + Shift + .` muestra los ocultos en Finder).
Fuera de `PAS-1`, CMake ya no encuentra JUCE solo: compilá con `cmake -B build -DJUCE_PATH=/ruta/a/JUCE`
(ver "Dónde tiene que estar JUCE" en el README del ejemplo). Con Projucer no cambia nada.

El reproductor ya te llena el buffer con muestras. A partir de ahí, lo que hagas en `getNextAudioBlock` es tuyo.
Los ejemplos 02 a 05 te sirven de referencia. Para probar, hay archivos de audio en [`audiofiles/`](../../audiofiles).

## Dónde va cada cosa

Casi todo tu trabajo va en `MainComponent.h` y `MainComponent.cpp`. `Main.cpp` solo crea la ventana y no hace falta tocarlo.
El bloque marcado `---- Reproductor ----` es el reproductor: dejalo como está y agregá lo tuyo después.

| Dónde | Qué va | Hilo | En los ejemplos |
|---|---|---|---|
| `MainComponent.h` | Las variables de la clase: controles (`Slider`, `Label`, `ComboBox`), parámetros (`std::atomic<float>`), el estado del proceso y los rectángulos donde vas a dibujar. | | 03: `gain`, `pan` y sus sliders. 04: `lastOutput[2]`. 05: `delayBuffer` |
| `MainComponent()` (constructor) | Configurar cada control (`setRange`, `setValue`, `onValueChange`) y mostrarlo con `addAndMakeVisible`. Arrancar el `Timer` si lo usás. Al final, `setSize` y `setAudioChannels`. | interfaz | 03 y 05: sliders con `onValueChange`. 02: `startTimerHz (30)` |
| `prepareToPlay` | Se llama antes de que empiece el audio y te dice el **sample rate** y el **tamaño de bloque**. Guardá el sample rate, reservá memoria y poné el estado en cero. | audio | 04: guarda `currentSampleRate`. 05: reserva y limpia `delayBuffer` |
| `getNextAudioBlock` | **Tu procesamiento.** Primero el reproductor llena el buffer y después recorrés las muestras (canal por canal, muestra por muestra) para leerlas o modificarlas. | audio | 02: calcula el RMS. 03: multiplica por la ganancia. 04: filtra. 05: delay |
| `releaseResources` | Se llama cuando el audio se detiene. Casi nunca hace falta agregar nada. | audio | |
| `paint` | Dibujar: fondo, medidores, formas de onda, texto. Solo dibuja con los valores que ya tenés y no calcula audio. | interfaz | 02: la barra del medidor |
| `resized` | Ubicar cada control en la ventana con `setBounds`, recortando el área con `removeFromTop` y `removeFromLeft`. Se llama al abrir la ventana y cada vez que cambia de tamaño. | interfaz | 02: explicación con dibujo de `removeFromTop` |
| `timerCallback` | Opcional. Se llama varias veces por segundo en el hilo de la interfaz: lee lo que midió el audio, actualiza los labels y llama a `repaint()`. Necesita heredar de `juce::Timer`. | interfaz | 02: suaviza el RMS y redibuja |
| `~MainComponent()` (destructor) | `shutdownAudio()` primero. Ya está hecho. | interfaz | |

**Los dos hilos.** `getNextAudioBlock` corre en el hilo de audio y tiene que terminar rápido: sin pedir memoria,
sin abrir archivos, sin esperar a nada. Lo que comparten el audio y la interfaz (un parámetro de un slider o un
valor medido) va en un `std::atomic` (ver el comentario largo en el `.h` del ejemplo 02).

### Receta: agregar un slider que controle el audio

1. **`.h`**: el control y el parámetro.
   ```cpp
   juce::Slider driveSlider;
   juce::Label  driveLabel { {}, "Drive" };
   std::atomic<float> drive { 1.0f };
   ```
2. **Constructor**: configurarlo y mostrarlo.
   ```cpp
   driveSlider.setRange (1.0, 10.0, 0.1);
   driveSlider.setValue (drive);
   driveSlider.onValueChange = [this] { drive = (float) driveSlider.getValue(); };
   addAndMakeVisible (driveSlider);
   addAndMakeVisible (driveLabel);
   ```
3. **`resized`**: darle un lugar.
   ```cpp
   auto sliderRow = area.removeFromTop (30);
   driveLabel.setBounds (sliderRow.removeFromLeft (100));
   driveSlider.setBounds (sliderRow);
   ```
4. **`getNextAudioBlock`**: usarlo con cada muestra.
   ```cpp
   float d = drive; // leerlo una vez por bloque
   for (int channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
       for (int i = 0; i < bufferToFill.numSamples; ++i)
       {
           int index = bufferToFill.startSample + i;
           float sample = bufferToFill.buffer->getSample (channel, index);
           bufferToFill.buffer->setSample (channel, index, std::tanh (sample * d));
       }
   ```

Para **mostrar** algo que midió el audio el camino es el inverso: `getNextAudioBlock` escribe un `std::atomic`,
`timerCallback` lo lee y llama a `repaint()`, y `paint` lo dibuja (ejemplo 02).

## Reglas

- Trabajá **muestra por muestra** sobre el buffer, escribiendo vos los cálculos.
- **Sin FFT** ni análisis de frecuencia.
- **Sin síntesis:** el sonido sale del archivo, no de osciladores (eso viene en el módulo 3).
- Una interfaz con al menos un control (slider, botón, combo) que cambie algo de lo que hace la app,
  o una visualización de lo que mide.

## Algunas ideas

Son solo sugerencias: podés tomar una, combinar varias o hacer otra cosa.

**Análisis**
- Medidor de **peak** con indicador de *clipping* (se prende cuando una muestra llega a 1).
- **Osciloscopio:** dibujar la forma de onda del último bloque.
- Medidor **por canal** (izquierdo y derecho) o de correlación entre canales.
- Contar los **cruces por cero** del bloque: da una idea de qué tan aguda o ruidosa es la señal.

**Procesamiento**
- **Distorsión:** recortar las muestras (*clipping*) o pasarlas por una curva como `std::tanh`.
- **Bitcrusher:** reducir la resolución de las muestras (redondear) o repetir muestras para bajar el sample rate.
- **Ancho estéreo:** pasar a mono, invertir canales o ensanchar con mid/side.
- **Delay multi-tap** o **ping-pong** (el eco salta de un canal al otro), a partir del ejemplo 05.

**Análisis + procesamiento**
- **Noise gate:** silenciar la señal cuando el RMS baja de un umbral.
- **Compresor simple:** bajar la ganancia cuando el nivel supera un umbral.
- **Filtro controlado por el nivel:** que la frecuencia de corte del filtro del ejemplo 04 siga al RMS.

