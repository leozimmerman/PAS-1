# Modulation

El oscilador del ejemplo 01 (solo la onda cuadrada, en una nota fija de 220 Hz) con un **LFO**
(*Low Frequency Oscillator*): un oscilador lento que mueve un parámetro.

- **LFO a mano:** un seno igual al del 01, entre 0.1 y 20 Hz.
- **Modulación de amplitud (tremolo):** el volumen sube y baja. Profundidad de 0 a 1.
- **Modulación de frecuencia (vibrato):** la altura sube y baja alrededor de 220 Hz. Profundidad en Hz.
- Las dos profundidades arrancan en 0: subí una por vez para escuchar cada modulación.

El volumen arranca bajo (0.2). La nota suena cuando está tildado *Sonar*.

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
2. Abrir `Modulation.jucer`, elegir el exportador (Xcode, Visual Studio 2022, Visual Studio 2026 o Linux Makefile: el que coincida con tu versión de Visual Studio) y *Save and Open in IDE*.
3. Compilar y ejecutar desde el IDE.
   - **Xcode:** esquema *Modulation - App* y ▶.
   - **Visual Studio:** *Depurar → Iniciar depuración* (F5).
   - **Linux:** `cd Builds/LinuxMakefile && make`, y después `./build/Modulation`.

### Con CMake
Desde esta carpeta:

```bash
cmake -B build
cmake --build build
```

| Sistema | Dónde queda la app |
|---|---|
| macOS | `build/Modulation_artefacts/Modulation.app` (abrir con `open`) |
| Windows | `build\Modulation_artefacts\Debug\Modulation.exe` |
| Linux | `build/Modulation_artefacts/Modulation` |

En Windows también se puede abrir esta carpeta en Visual Studio (*Abrir una carpeta local*) y ejecutar *Modulation.exe*.
En Linux hacen falta las [dependencias de JUCE](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md).

Más detalle en el [README de HelloJuce](../../../modulo-1-entorno/ejemplo/HelloJuce/README.md).
