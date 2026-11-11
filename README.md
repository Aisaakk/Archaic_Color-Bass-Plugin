# Archaic  (v0.1.0)

## Compilar (VST3)
Requisitos: CMake 3.22+, un compilador C++17 y git.
- Windows: Visual Studio 2022 (carga "Desktop development with C++").
- macOS: Xcode command line tools.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

JUCE se descarga solo la primera vez. Al terminar, el .vst3 se copia a la carpeta de plugins del sistema.
Si no aparece en tu DAW, busca `Archaic.vst3` dentro de `build/Archaic_artefacts/Release/VST3/` y copialo a
`C:\Program Files\Common Files\VST3` (Windows) o `~/Library/Audio/Plug-Ins/VST3` (macOS).

## Donde tocar cada cosa
- Source/SpectralSnapper.h  -> procesamiento (tonalidad, Color, latencia)
- Source/Parameters.h       -> parametros y escalas (agrega escalas en getScales())
- Source/Look.h             -> TODO el aspecto: glass::panel(), knobs, botones, temas claro/oscuro
- Source/PluginEditor.cpp   -> posicion y tamaño de cada elemento (resized()) y fondo (paint())

## Compilar con GitHub (sin instalar nada)
Sube todo el proyecto (incluida la carpeta .github) a un repositorio, ve a la pestaña Actions,
abre "Build Archaic" y descarga los VST3 de Windows y macOS desde "Artifacts".
