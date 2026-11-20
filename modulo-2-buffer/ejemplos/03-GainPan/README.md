# GainPan

Ganancia y paneo sobre el reproductor del ejemplo 01.

- **Ganancia:** un slider de 0 a 1. Cada muestra se multiplica por ese número (0 = silencio, 1 = volumen original).
- **Paneo de potencia constante:** izquierda `cos (p * pi/2)`, derecha `sin (p * pi/2)`, con `p` entre 0 y 1.
- Si movés rápido un slider puede escucharse un ruidito (*zipper noise*): falta suavizar el valor.

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
2. Abrir `GainPan.jucer`, elegir el exportador (Xcode, Visual Studio 2022, Visual Studio 2026 o Linux Makefile: el que coincida con tu versión de Visual Studio) y *Save and Open in IDE*.
3. Compilar y ejecutar desde el IDE.
   - **Xcode:** esquema *GainPan - App* y ▶.
   - **Visual Studio:** *Depurar → Iniciar depuración* (F5).
   - **Linux:** `cd Builds/LinuxMakefile && make`, y después `./build/GainPan`.

### Con CMake
Desde esta carpeta:

```bash
cmake -B build
cmake --build build
```

| Sistema | Dónde queda la app |
|---|---|
| macOS | `build/GainPan_artefacts/GainPan.app` (abrir con `open`) |
| Windows | `build\GainPan_artefacts\Debug\GainPan.exe` |
| Linux | `build/GainPan_artefacts/GainPan` |

En Windows también se puede abrir esta carpeta en Visual Studio (*Abrir una carpeta local*) y ejecutar *GainPan.exe*.
En Linux hacen falta las [dependencias de JUCE](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md).

Más detalle en el [README de HelloJuce](../../../modulo-1-entorno/ejemplo/HelloJuce/README.md).
