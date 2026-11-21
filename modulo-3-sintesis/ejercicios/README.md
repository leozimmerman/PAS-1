# TP 3 · Un sinte propio

## Objetivo

Diseñar y programar **tu propio sintetizador**, controlado por MIDI: que genere el sonido muestra por muestra,
con una envolvente, y que lo puedas tocar con el teclado en pantalla o con un teclado MIDI.

El sinte es libre. Lo único que pedimos es que quede dentro de lo que vimos en el módulo.

## Punto de partida

Copiá el ejemplo [`../ejemplos/03-MidiSynth`](../ejemplos/03-MidiSynth) o el [`../ejemplos/04-MidiTiming`](../ejemplos/04-MidiTiming)
(el mismo, con el timing MIDI exacto) a una carpeta **fuera de este repo**, en tu propio repositorio de Git.

Y cambiale el nombre al proyecto:

- **Projucer:** abrí el `.jucer` y cambiá el *Project Name*.
- **CMake:** en el `CMakeLists.txt`, reemplazá `MidiSynth` (o `MidiTiming`) por el nombre de tu app.

El ejemplo ya trae un **`.gitignore`**, para que Git no guarde lo que se genera al compilar (`Builds/`, `build/`...).
Es un archivo oculto: copiá la carpeta entera, no solo los archivos que ves (en Mac, `Cmd + Shift + .` muestra los ocultos en Finder).
Fuera de `PAS-1`, CMake ya no encuentra JUCE solo: compilá con `cmake -B build -DJUCE_PATH=/ruta/a/JUCE`
(ver "Dónde tiene que estar JUCE" en el README del ejemplo). Con Projucer no cambia nada.

Los ejemplos 01, 02 y 05 te sirven de referencia (formas de onda y tablas, la envolvente, el LFO).

## Dónde va cada cosa

Igual que en el TP 2, casi todo tu trabajo va en `MainComponent.h` y `MainComponent.cpp`, y `Main.cpp` no hace falta tocarlo.
Los bloques marcados `==== MIDI ====` reciben las notas: dejalos como están y agregá lo tuyo en el oscilador,
la envolvente o en funciones nuevas. Lo que no cambia respecto del TP 2 (constructor, `resized`, `paint`, destructor)
está explicado en la tabla de [`modulo-2-buffer/ejercicios`](../../modulo-2-buffer/ejercicios/README.md#dónde-va-cada-cosa).

| Dónde | Qué va | Hilo | En los ejemplos |
|---|---|---|---|
| `MainComponent.h` | Las variables de la clase: los parámetros que mueven los sliders (`std::atomic<float>`), el estado de la voz (fases, etapa y valor de la envolvente, nota actual) y los controles. | | 01: `waveform`, `mode` y las tablas. 02: `attack`...`release` y `Stage`. 05: `lfoRate`, `lfoPhase` |
| `MainComponent()` (constructor) | Configurar cada control y mostrarlo (con `setupSlider` te ahorrás repetir código). Si calculás tablas de onda, llenalas acá, antes de `setAudioChannels`. | interfaz | 01: `fillTable`. 02 y 03: `setupSlider` |
| `prepareToPlay` | Guardar el sample rate y poner en cero el estado: las fases de los osciladores y del LFO, la envolvente en `Idle`. | audio | 03: `phase`, `stage` y `midiCollector.reset` |
| `getNextAudioBlock` | Junta los mensajes MIDI del bloque, los atiende y llama a `renderSamples`. En el 03 y el 04 no hace falta tocarlo: es lo que cambia entre los dos. | audio | 03: atiende todo al principio. 04: corta el bloque en tramos, uno por mensaje |
| `renderSamples` | **Tu síntesis.** El loop muestra por muestra: oscilador, envolvente, modulación, mezcla, y escribir el resultado en todos los canales. | audio | 03: seno × envolvente × velocity. 05: el LFO (en `getNextAudioBlock`) |
| Una función propia para el oscilador | La forma de onda para una fase entre 0 y 1: varias formas, una tabla, formas mezcladas. Que solo calcule: la fase la avanza `renderSamples`. | audio | 01: `formulaSample` y `tableSample` |
| `nextEnvelopeValue` | La envolvente: avanza un paso por muestra y devuelve su valor. Acá cambiás las etapas o las curvas. | audio | 02: el ADSR lineal |
| `handleMidiMessage` | Qué pasa al apretar y al soltar una tecla: calcular la frecuencia, arrancar o soltar la envolvente. Corre en el hilo de audio, llamada desde `getNextAudioBlock`. | audio | 03: nota → frecuencia, velocity → amplitud |
| `timerCallback` | Muestra el último mensaje MIDI. Si querés mostrar algo más (la etapa de la envolvente, el LFO), pasalo con un `std::atomic`. | interfaz | 02: la etapa y el valor de la envolvente. 03: el último mensaje |
| `~MainComponent()` (destructor) | Desconectar el MIDI (`removeMidiInputDeviceCallback`) y `shutdownAudio()`. Ya está hecho. | interfaz | |

**Los dos hilos.** Todo lo que suena (`renderSamples`, `handleMidiMessage`, la envolvente y los osciladores) corre
en el hilo de audio: sin pedir memoria, sin abrir archivos, sin esperar a nada. Por eso las tablas se llenan
en el constructor y no al tocar una nota. Los parámetros que vienen de los sliders van en `std::atomic`.
El estado de la voz (fases, envolvente, nota actual) lo usa solo el audio y puede ser una variable común.

### Receta: agregar un segundo oscilador desafinado

1. **`.h`**: el parámetro, la fase del segundo oscilador y el control.
   ```cpp
   std::atomic<float> detune { 10.0f }; // en cents (100 cents = un semitono)
   float phase2 = 0.0f;                 // solo la usa el hilo de audio

   juce::Slider detuneSlider;
   juce::Label  detuneLabel { {}, "Detune" };
   ```
2. **Constructor**: configurarlo y mostrarlo (con el `setupSlider` del ejemplo).
   ```cpp
   setupSlider (detuneSlider, detuneLabel, 0.0, 50.0, detune, " cents");
   ```
3. **`prepareToPlay`**: arrancar la fase en cero.
   ```cpp
   phase2 = 0.0f;
   ```
4. **`resized`**: darle un lugar (si no entra, agrandá la ventana en el `setSize` del constructor).
   ```cpp
   auto detuneRow = area.removeFromTop (30);
   detuneLabel.setBounds (detuneRow.removeFromLeft (100));
   detuneSlider.setBounds (detuneRow);
   area.removeFromTop (5);
   ```
5. **`renderSamples`**: calcular los dos osciladores y mezclarlos.
   ```cpp
   float ratio = std::pow (2.0f, detune / 1200.0f); // cents -> factor de frecuencia (leerlo una vez por tramo)

   for (int i = 0; i < numSamples; ++i)
   {
       float osc1 = std::sin (juce::MathConstants<float>::twoPi * phase);
       float osc2 = std::sin (juce::MathConstants<float>::twoPi * phase2);
       float sample = 0.5f * (osc1 + osc2);  // la mitad de cada uno, para no pasarse de 1

       sample = sample * nextEnvelopeValue() * velocityGain;
       // ... escribir en los canales, igual que antes ...

       phase += f / (float) sr;
       if (phase >= 1.0f)
           phase -= 1.0f;

       phase2 += f * ratio / (float) sr;
       if (phase2 >= 1.0f)
           phase2 -= 1.0f;
   }
   ```

Para cualquier otro oscilador o LFO es lo mismo: **una fase propia** en el `.h`, que se pone en cero en `prepareToPlay`
y se avanza en `renderSamples`, sumando su frecuencia dividida por el sample rate en cada muestra (ejemplos 01 y 05).

## Reglas

- **App standalone** (`AudioAppComponent`), como los ejemplos: sin plugins (vienen en el módulo 4).
- **Sin sampling:** el sonido sale de tus osciladores, no de archivos de audio (también viene en el módulo 4).
- El DSP lo escribís vos, muestra por muestra. De JUCE podés usar lo que usan los ejemplos (`ADSR`,
  `MidiKeyboardComponent`, `MidiMessageCollector`, los controles de la interfaz) y `Synthesiser`, pero **no** los
  osciladores, filtros ni efectos ya hechos de `juce::dsp`.

## Algunas ideas

Son solo sugerencias: podés tomar una, combinar varias o hacer otra cosa.

**Osciladores**
- **Tablas propias:** llenar una tabla con una forma inventada o dibujada, o con una **suma de armónicos** (síntesis aditiva).
- **Dos osciladores por nota** mezclados, con uno desafinado (*detune*) o una octava abajo (sub-oscilador).
- **Unísono:** varias copias de la misma nota levemente desafinadas, para un sonido más ancho.
- **Ancho de pulso:** una cuadrada que no cambia a la mitad del ciclo, sino donde diga un slider.

**Modulación**
- **FM entre osciladores:** un oscilador que modula la frecuencia de otro, a frecuencias audibles.
- **AM o ring modulation:** multiplicar dos osciladores.
- **LFO con forma seleccionable** o con varios destinos a la vez.
- **Chorus:** el delay del módulo 2 con el tiempo modulado por un LFO.

**Sobre la salida**
- **Filtro** de primer orden del módulo 2 sobre la mezcla, con la frecuencia de corte controlada por una envolvente o un LFO.
- **Delay** del módulo 2 sobre la salida del sinte.

**Interpretación**
- **Polifonía** (más difícil): tocar acordes con `juce::Synthesiser`, una voz por nota.
- **Arpegiador:** al mantener un acorde, tocar sus notas una por una.
- **Portamento:** en el sinte monofónico, que la frecuencia se deslice de una nota a la siguiente.
- **Pitch bend** o **rueda de modulación** de un teclado MIDI (en `handleMidiMessage`: `isPitchWheel()`, `isController()`).
