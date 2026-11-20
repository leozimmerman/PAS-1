# Módulo 3 · Síntesis, ADSR y MIDI

Generar el sonido en vez de leerlo de un archivo: osciladores, envolventes, notas MIDI y modulación.

## Ejemplos

Todos parten del **01**, el oscilador: el 02 le agrega la envolvente, el 03 y el 04 el MIDI, y el 05 una modulación.

| Carpeta | Qué muestra |
|---|---|
| [`ejemplos/01-Oscillator`](ejemplos/01-Oscillator) | Acumulador de fase y formas de onda (seno, sierra, cuadrada, triangular, ruido); cada muestra calculada con la fórmula o leída de una tabla con interpolación; aliasing |
| [`ejemplos/02-Envelope`](ejemplos/02-Envelope) | Envolvente ADSR escrita a mano, con un botón que hace de tecla |
| [`ejemplos/03-MidiSynth`](ejemplos/03-MidiSynth) | Sinte monofónico con teclado en pantalla y teclado MIDI: nota → frecuencia, velocity → amplitud |
| [`ejemplos/04-MidiTiming`](ejemplos/04-MidiTiming) | El 03 con cada mensaje MIDI atendido en su muestra exacta, partiendo el bloque en tramos |
| [`ejemplos/05-Modulation`](ejemplos/05-Modulation) | Una onda cuadrada en una nota fija, con un LFO que modula la amplitud (tremolo) y la frecuencia (vibrato) |

Todos arrancan con el volumen bajo (0.2). Subilo de a poco.

## Ejercicio

La consigna del sábado está en [`ejercicios/`](ejercicios): **TP 3**, un sinte propio controlado por MIDI.

## Compilar

Cada ejemplo tiene un `.jucer` y un `CMakeLists.txt`, igual que HelloJuce del módulo 1:

- **Projucer:** abrir el `.jucer`, elegir el exportador (Xcode, Visual Studio 2022, Visual Studio 2026 o Linux Makefile: el que coincida con tu versión de Visual Studio) y *Save and Open in IDE*.
- **CMake:** desde la carpeta del ejemplo, `cmake -B build` y `cmake --build build`.

Projucer toma JUCE de su *Global Path*; CMake lo busca en la carpeta `JUCE` al lado de `PAS-1`
(si copiás un ejemplo a otro lugar, usá `-DJUCE_PATH`). El README de cada ejemplo lo explica.

Más detalle en el [README de HelloJuce](../modulo-1-entorno/ejemplo/HelloJuce/README.md).
