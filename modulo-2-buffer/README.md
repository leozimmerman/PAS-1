# Módulo 2 · El buffer de audio

Leer y modificar las muestras del buffer de audio: análisis (RMS), interfaz gráfica, suavizado y efectos simples.

## Ejemplos

Todos parten del **01**, un reproductor de archivos: la parte del reproductor es igual en todos
y cada ejemplo le agrega algo (un medidor, un efecto).

| Carpeta | Qué muestra |
|---|---|
| [`ejemplos/01-AudioFilePlayer`](ejemplos/01-AudioFilePlayer) | Cargar un archivo, play y stop: una forma simple de llenar el buffer con muestras |
| [`ejemplos/02-RmsMeter`](ejemplos/02-RmsMeter) | Calcular el RMS recorriendo el buffer, dibujar un medidor y suavizarlo |
| [`ejemplos/03-GainPan`](ejemplos/03-GainPan) | Ganancia (0 a 1) y paneo, muestra por muestra |
| [`ejemplos/04-FirstOrderFilter`](ejemplos/04-FirstOrderFilter) | Filtro pasabajos y pasaaltos de primer orden: la misma fórmula que el suavizado del 02 |
| [`ejemplos/05-DelayApp`](ejemplos/05-DelayApp) | Delay estéreo con buffer circular, feedback y dry/wet |

Para probarlos hay archivos de audio en [`audiofiles/`](../audiofiles).

## Ejercicio

La consigna del sábado está en [`ejercicios/`](ejercicios): **TP 2**, una app libre que analice o procese el buffer.

## Compilar

Cada ejemplo tiene un `.jucer` y un `CMakeLists.txt`, igual que HelloJuce del módulo 1:

- **Projucer:** abrir el `.jucer`, elegir el exportador (Xcode, Visual Studio 2022, Visual Studio 2026 o Linux Makefile: el que coincida con tu versión de Visual Studio) y *Save and Open in IDE*.
- **CMake:** desde la carpeta del ejemplo, `cmake -B build` y `cmake --build build`.

Projucer toma JUCE de su *Global Path*; CMake lo busca en la carpeta `JUCE` al lado de `PAS-1`
(si copiás un ejemplo a otro lugar, usá `-DJUCE_PATH`). El README de cada ejemplo lo explica.

Más detalle en el [README de HelloJuce](../modulo-1-entorno/ejemplo/HelloJuce/README.md).
