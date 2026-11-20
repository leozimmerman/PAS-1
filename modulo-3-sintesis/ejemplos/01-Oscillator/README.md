# Oscillator

Un oscilador escrito a mano: cada muestra la calcula la app, no sale de un archivo.

- **Acumulador de fase:** la fase va de 0 a 1 y en cada muestra avanza `frecuencia / sr`.
- **Formas de onda:** seno, sierra, cuadrada, triangular y ruido.
- **Fórmula o tabla:** el selector *Muestras* elige cómo se obtiene cada muestra.
  - *Formula*: se calcula en cada muestra (`std::sin`, etc.). Exacta, sin memoria, pero se hace 44100 veces por segundo.
  - *Tabla*: un ciclo calculado una vez y guardado en memoria; después solo se lee, con **interpolación lineal**.
    Es barato y la tabla puede tener cualquier forma. Destildá *Interpolar* para escuchar los escalones
    (con `tableSize` en 16 o 64 se nota mucho más).
- **Aliasing:** con la sierra o la cuadrada en frecuencias altas aparecen tonos que bajan mientras la frecuencia sube.

El volumen arranca bajo (0.2). El oscilador suena cuando está tildado *Sonar*.

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
2. Abrir `Oscillator.jucer`, elegir el exportador (Xcode, Visual Studio 2022, Visual Studio 2026 o Linux Makefile: el que coincida con tu versión de Visual Studio) y *Save and Open in IDE*.
3. Compilar y ejecutar desde el IDE.
   - **Xcode:** esquema *Oscillator - App* y ▶.
   - **Visual Studio:** *Depurar → Iniciar depuración* (F5).
   - **Linux:** `cd Builds/LinuxMakefile && make`, y después `./build/Oscillator`.

### Con CMake
Desde esta carpeta:

```bash
cmake -B build
cmake --build build
```

| Sistema | Dónde queda la app |
|---|---|
| macOS | `build/Oscillator_artefacts/Oscillator.app` (abrir con `open`) |
| Windows | `build\Oscillator_artefacts\Debug\Oscillator.exe` |
| Linux | `build/Oscillator_artefacts/Oscillator` |

En Windows también se puede abrir esta carpeta en Visual Studio (*Abrir una carpeta local*) y ejecutar *Oscillator.exe*.
En Linux hacen falta las [dependencias de JUCE](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md).

Más detalle en el [README de HelloJuce](../../../modulo-1-entorno/ejemplo/HelloJuce/README.md).
