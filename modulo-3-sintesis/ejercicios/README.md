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

Igual que en el TP 2 (ver la tabla de [`modulo-2-buffer/ejercicios`](../../modulo-2-buffer/ejercicios/README.md#dónde-va-cada-cosa)).
En el 03 y el 04:

| Dónde | Qué va | En los ejemplos |
|---|---|---|
| `renderSamples` | El loop muestra por muestra: oscilador, envolvente, modulación. | 03: seno × envolvente × velocity. 05: el LFO |
| Una función propia para el oscilador | La forma de onda para una fase entre 0 y 1: varias formas, una tabla, formas mezcladas. | 01: `formulaSample` y `tableSample` |
| `handleMidiMessage` | Qué pasa al apretar y al soltar una tecla: calcular la frecuencia, arrancar o soltar la envolvente. | 03: nota → frecuencia, velocity → amplitud |
| `MainComponent.h`, constructor y `resized` | Los controles nuevos: igual que en el TP 2 (ver la receta para agregar un slider). | 05: los sliders del LFO |

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
