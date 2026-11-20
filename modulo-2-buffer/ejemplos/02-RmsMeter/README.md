# RmsMeter

Un medidor de nivel RMS sobre el reproductor del ejemplo 01.

- **RMS a mano:** en `getNextAudioBlock`, un loop recorre las muestras del buffer, las eleva al cuadrado, las promedia
  y saca la raíz cuadrada.
- **Pasar el valor a la interfaz:** el hilo de audio lo guarda en un `std::atomic` y un `Timer` lo lee 30 veces por segundo.
- **Medidor:** una barra dibujada en `paint()`, y un `Label` con el valor del RMS.
- **Suavizado:** el slider elige `n` en `nuevoValor = ultimoValor * n + nuevoValor * (1 - n)`.
  Esta fórmula es un filtro pasabajos de un polo: la volvemos a usar en el ejemplo 04.

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
2. Abrir `RmsMeter.jucer`, elegir el exportador (Xcode, Visual Studio 2022, Visual Studio 2026 o Linux Makefile: el que coincida con tu versión de Visual Studio) y *Save and Open in IDE*.
3. Compilar y ejecutar desde el IDE.
   - **Xcode:** esquema *RmsMeter - App* y ▶.
   - **Visual Studio:** *Depurar → Iniciar depuración* (F5).
   - **Linux:** `cd Builds/LinuxMakefile && make`, y después `./build/RmsMeter`.

### Con CMake
Desde esta carpeta:

```bash
cmake -B build
cmake --build build
```

| Sistema | Dónde queda la app |
|---|---|
| macOS | `build/RmsMeter_artefacts/RmsMeter.app` (abrir con `open`) |
| Windows | `build\RmsMeter_artefacts\Debug\RmsMeter.exe` |
| Linux | `build/RmsMeter_artefacts/RmsMeter` |

En Windows también se puede abrir esta carpeta en Visual Studio (*Abrir una carpeta local*) y ejecutar *RmsMeter.exe*.
En Linux hacen falta las [dependencias de JUCE](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md).

Más detalle en el [README de HelloJuce](../../../modulo-1-entorno/ejemplo/HelloJuce/README.md).
