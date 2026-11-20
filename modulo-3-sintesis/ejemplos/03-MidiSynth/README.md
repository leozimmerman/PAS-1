# MidiSynth

Un sinte **monofónico** controlado por MIDI: un seno (el oscilador del 01) con la envolvente del 02.

- **Teclado en pantalla** (`MidiKeyboardComponent`): se toca con el mouse o, después de hacer clic en él, con las teclas A S D F...
- **Teclado MIDI de hardware:** la app habilita todas las entradas MIDI conectadas al abrirse
  (si lo conectás con la app abierta, cerrala y volvé a abrirla).
- **Nota → frecuencia:** `f = 440 * 2^((nota - 69) / 12)`.
- **Velocity → amplitud:** de 0..127 a 0..1.
- Arriba del teclado se muestra el último mensaje: note on/off, número de nota y velocity.
- Suena una sola nota a la vez: la última que apretaste.
- **Los mensajes se atienden al principio de cada bloque:** es lo más simple, pero con un buffer grande
  las notas pierden precisión rítmica (el comentario en `getNextAudioBlock` explica por qué).
  La solución está en el ejemplo 04.

## Compilar

Antes: tener instalado lo de [REQUISITOS.md](../../../REQUISITOS.md).

### Dónde tiene que estar JUCE
- **Projucer:** no importa dónde esté el ejemplo. El `.jucer` toma JUCE del *Global Path* de Projucer
  (*Global Paths → Path to JUCE*), que se configura una sola vez.
- **CMake:** el `CMakeLists.txt` busca JUCE en `../../../../JUCE`, es decir, la carpeta `JUCE` al lado de `PAS-1`:
  ```
  (cualquier carpeta)/
  ├── JUCE/
  └── PAS-1/modulo-.../ejemplos/<este ejemplo>/
  ```
  Si copiaste el ejemplo a otro lugar (por ejemplo, a tu propio repo), ese camino ya no sirve: decile dónde está JUCE
  con `cmake -B build -DJUCE_PATH=/ruta/a/JUCE` (en Windows, con barras `/`: `-DJUCE_PATH=C:/ruta/a/JUCE`).
  Si no lo encuentra, descarga JUCE 9.0.5 de GitHub: hace falta internet y la primera vez tarda.

### Con Projucer
1. La primera vez, en Projucer: *Global Paths → Path to JUCE* con la carpeta de JUCE.
2. Abrir `MidiSynth.jucer`, elegir el exportador (Xcode, Visual Studio 2022, Visual Studio 2026 o Linux Makefile: el que coincida con tu versión de Visual Studio) y *Save and Open in IDE*.
3. Compilar y ejecutar desde el IDE.
   - **Xcode:** esquema *MidiSynth - App* y ▶.
   - **Visual Studio:** *Depurar → Iniciar depuración* (F5).
   - **Linux:** `cd Builds/LinuxMakefile && make`, y después `./build/MidiSynth`.

### Con CMake
Desde esta carpeta:

```bash
cmake -B build
cmake --build build
```

| Sistema | Dónde queda la app |
|---|---|
| macOS | `build/MidiSynth_artefacts/MidiSynth.app` (abrir con `open`) |
| Windows | `build\MidiSynth_artefacts\Debug\MidiSynth.exe` |
| Linux | `build/MidiSynth_artefacts/MidiSynth` |

En Windows también se puede abrir esta carpeta en Visual Studio (*Abrir una carpeta local*) y ejecutar *MidiSynth.exe*.
En Linux hacen falta las [dependencias de JUCE](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md).

Más detalle en el [README de HelloJuce](../../../modulo-1-entorno/ejemplo/HelloJuce/README.md).
