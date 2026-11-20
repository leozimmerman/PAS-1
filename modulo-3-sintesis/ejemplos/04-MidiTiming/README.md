# MidiTiming

El sinte del ejemplo 03, pero con cada mensaje MIDI atendido **en su muestra exacta**.

- En el 03, todos los mensajes del bloque se atienden al principio: cada nota arranca en la primera muestra
  del bloque aunque haya llegado a mitad. Con un buffer grande el error llega a un bloque entero
  (10,7 ms con 512 muestras a 48 kHz), distinto en cada nota, y el ritmo queda irregular.
- Acá el bloque se **parte en tramos**: se calcula el audio hasta el próximo mensaje, se atiende el mensaje
  y se sigue. Así cada nota arranca en su muestra. Es lo que hace `juce::Synthesiser` por dentro.
- El comentario "Timing de los mensajes MIDI" compara tres maneras de hacerlo.
- Lo demás es igual al 03: el código solo cambia en `getNextAudioBlock`.

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
2. Abrir `MidiTiming.jucer`, elegir el exportador (Xcode, Visual Studio 2022, Visual Studio 2026 o Linux Makefile: el que coincida con tu versión de Visual Studio) y *Save and Open in IDE*.
3. Compilar y ejecutar desde el IDE.
   - **Xcode:** esquema *MidiTiming - App* y ▶.
   - **Visual Studio:** *Depurar → Iniciar depuración* (F5).
   - **Linux:** `cd Builds/LinuxMakefile && make`, y después `./build/MidiTiming`.

### Con CMake
Desde esta carpeta:

```bash
cmake -B build
cmake --build build
```

| Sistema | Dónde queda la app |
|---|---|
| macOS | `build/MidiTiming_artefacts/MidiTiming.app` (abrir con `open`) |
| Windows | `build\MidiTiming_artefacts\Debug\MidiTiming.exe` |
| Linux | `build/MidiTiming_artefacts/MidiTiming` |

En Windows también se puede abrir esta carpeta en Visual Studio (*Abrir una carpeta local*) y ejecutar *MidiTiming.exe*.
En Linux hacen falta las [dependencias de JUCE](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md).

Más detalle en el [README de HelloJuce](../../../modulo-1-entorno/ejemplo/HelloJuce/README.md).
