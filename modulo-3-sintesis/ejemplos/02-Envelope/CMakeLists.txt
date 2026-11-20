# ------------------------------------------------------------------------------
# Envelope: el mismo proyecto que Envelope.jucer, descrito con CMake.
#
#   cmake -B build                          (busca JUCE al lado del repo: ../JUCE)
#   cmake -B build -DJUCE_PATH=/ruta/a/JUCE (o indicá dónde está)
#   cmake --build build
#
# Si no encuentra JUCE, lo descarga de GitHub (hace falta internet la 1ra vez).
# ------------------------------------------------------------------------------
cmake_minimum_required (VERSION 3.22)
project (Envelope VERSION 1.0.0)

set (CMAKE_CXX_STANDARD 17)

set_property (GLOBAL PROPERTY USE_FOLDERS YES)
option (JUCE_ENABLE_MODULE_SOURCE_GROUPS "Mostrar los módulos de JUCE en el IDE" ON)

# --- 1. Encontrar JUCE (PAS-1/<módulo>/ejemplos/<ejemplo> → ../../../../JUCE) ---
set (JUCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../JUCE" CACHE PATH "Carpeta de JUCE")

if (EXISTS "${JUCE_PATH}/CMakeLists.txt")
    add_subdirectory ("${JUCE_PATH}" JUCE)
else()
    include (FetchContent)
    FetchContent_Declare (JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.5
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable (JUCE)
endif()

# --- 2. Declarar la aplicación ------------------------------------------------
juce_add_gui_app (Envelope
    PRODUCT_NAME "Envelope"
    COMPANY_NAME "PAS1")

juce_generate_juce_header (Envelope)

# --- 3. Archivos fuente (los mismos que usa el .jucer) ------------------------
set (SOURCES
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp)

target_sources (Envelope PRIVATE ${SOURCES})
source_group (TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})

target_compile_definitions (Envelope PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Envelope,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Envelope,JUCE_VERSION>")

# --- 4. Módulos de JUCE -------------------------------------------------------
target_link_libraries (Envelope
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
