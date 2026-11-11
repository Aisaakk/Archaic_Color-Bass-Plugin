cmake_minimum_required(VERSION 3.22)
project(Archaic VERSION 0.2.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# En GitHub Actions (variable CI) no se copia el plugin a carpetas del sistema
if(DEFINED ENV{CI})
  set(ARCHAIC_COPY FALSE)
else()
  set(ARCHAIC_COPY TRUE)
endif()

juce_add_plugin(Archaic
  COMPANY_NAME "Aisaak"
  PLUGIN_MANUFACTURER_CODE Aisk
  PLUGIN_CODE Arch
  FORMATS VST3
  PRODUCT_NAME "Archaic"
  VST3_CATEGORIES Fx
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD ${ARCHAIC_COPY})

target_sources(Archaic PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(Archaic PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

# Assets incrustados (fuente del logotipo). Para usar tu propia fuente: reemplaza Assets/WordmarkFont.ttf
# (GFS Didot, licencia SIL OFL). Mas assets (SVG/PNG): anadelos a SOURCES y usa BinaryData::nombre_ext
juce_add_binary_data(ArchaicAssets SOURCES Assets/WordmarkFont.ttf)
set_target_properties(ArchaicAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)


target_link_libraries(Archaic
  PRIVATE ArchaicAssets juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
