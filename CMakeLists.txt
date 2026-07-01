# ============================================================================
# CMakeLists.txt - Autotune Clone
# Utilise le support CMake natif de JUCE 8 (ne necessite pas Projucer compile).
# Prerequis : CMake >= 3.22 et Visual Studio 2022.
# ============================================================================

cmake_minimum_required(VERSION 3.22)
project(OpenVoxTuner VERSION 0.1.1 LANGUAGES CXX)

# Standard C++17 (Projucer utilise ce standard pour les plugins).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Position Independent Code (necessaire pour certains formats de plugin).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------
# Compatibilite macOS : forcer un deployment target compatible
# avec JUCE 8 pour les builds sur Xcode 17+ / SDK 26.x "insider".
# ---------------------------------------------------------------
if(APPLE)
    # Force le SDK macOS a une version compatible avec JUCE 8.
    # JuCE 8 supporte officiellement macOS 10.15+ (Catalina) / 11.0+ (Big Sur).
    # Sur un SDK "insider" 26.x, certaines API sont depreciees et le flag
    # -Wunguarded-availability peut generer des erreurs. On fixe le
    # deployment target pour utiliser uniquement les API disponibles
    # depuis cette version.
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS deployment target (force 11.0 pour compat JUCE8 avec SDK 26.x)")
    endif()
    # Desactiver les avertissements de disponibilite non guardee
    # (causes par des API nouvelles dans le SDK 26.x que JUCE 8 ne connait pas)
    add_compile_options(-Wno-unguarded-availability)
    
    message(STATUS "macOS build: deployment target = ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

# Emplacement de JUCE (relatif ou absolu).
if(NOT DEFINED JUCE_PATH)
    set(JUCE_PATH "C:/JUCE" CACHE PATH "Chemin vers le depot JUCE")
endif()

# Active les exemples / tests si demandes.
option(AUTOTUNE_BUILD_TESTS "Compile les tests unitaires" OFF)

# Charge le support JUCE pour CMake.
add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/_juce_build)

# Chemin vers l'ARA SDK (necessaire pour le support ARA)
juce_set_ara_sdk_path(${CMAKE_CURRENT_SOURCE_DIR}/external/ARA_SDK)

# Formats plugin : VST3 + Standalone partout, AU en option sur macOS.
set(OVT_PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    option(OVT_ENABLE_AU "Build AU format on macOS" ON)
    if(OVT_ENABLE_AU)
        list(APPEND OVT_PLUGIN_FORMATS AU)
    endif()
endif()

# Cree la cible plugin.
juce_add_plugin(OpenVoxTuner
    COMPANY_NAME "EiffelBS"
    PLUGIN_MANUFACTURER_CODE Eiff
    PLUGIN_CODE OvtP
    FORMATS ${OVT_PLUGIN_FORMATS}
    NEEDS_MIDI_OUTPUT TRUE
    # Studio One / Fender Studio Pro requires the MIDI input bus to be present
    # during initial plugin scan so that it can register and persist MIDI output
    # routing to other tracks. Without this, MIDI routes show "Missing Device"
    # on project reload. The MIDI input is not processed by the plugin.
    NEEDS_MIDI_INPUT TRUE
    IS_ARA_EFFECT TRUE             # Active le support ARA2 (Audio Random Access)
    PRODUCT_NAME "OpenVoxTuner"
    VERSION 0.1.1
    ICON_BIG "assets/icon.png"
    ICON_SMALL "assets/icon.png")

# --- Build info generation ---
# Create a small header with a build id and timestamp so the UI/log can show which build is loaded.
string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M:%S")
execute_process(COMMAND git rev-parse --short HEAD
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_HASH
                RESULT_VARIABLE GIT_RESULT
                OUTPUT_STRIP_TRAILING_WHITESPACE)
if(GIT_RESULT EQUAL 0)
    set(BUILD_ID "${GIT_HASH}")
else()
    set(BUILD_ID "${BUILD_TIME}")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Source/BuildInfo.h.in ${CMAKE_BINARY_DIR}/BuildInfo.h @ONLY)

# Ensure built headers are available to our target
target_include_directories(OpenVoxTuner PRIVATE ${CMAKE_BINARY_DIR})

# Ensure the generated plugin metadata declares MIDI I/O for VST3 event buses.
# The plugin PRODUCES MIDI output (harmony note output on separate channels).
# MIDI_INPUT is declared to ensure Studio One/Fender Studio Pro correctly
# registers and persists MIDI output routing between sessions — the MIDI input
# bus is present but not processed by the plugin.
target_compile_definitions(OpenVoxTuner PRIVATE
    JUCE_PLUGIN_PRODUCES_MIDI_OUTPUT=1
    JUCE_PLUGIN_WANTS_MIDI_INPUT=1
    OVT_UPDATE_INFO_URL="${OVT_UPDATE_INFO_URL}"
)

# Workaround for VST3 JUCE 8 Parameter ID Warning.
# Le format VST3 attend des ID haches, mais JUCE genere les siens (compat VST2).
# Comme on ne distribue PAS de version VST2, on peut ignorer ce warning
# sans risque (les sessions ne referenceront jamais de VST2 de toute facon).
target_compile_definitions(OpenVoxTuner PRIVATE
    JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1)

# Chemins des sources.
target_sources(OpenVoxTuner PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/dsp/IPitchDetector.h
    Source/dsp/YinPitchDetector.cpp
    Source/dsp/YinPitchDetector.h
    #Source/dsp/SwipePitchDetector.cpp
    #Source/dsp/SwipePitchDetector.h
    #Source/dsp/PyinPitchDetector.cpp
    #Source/dsp/PyinPitchDetector.h
    Source/dsp/ScaleQuantizer.cpp
    Source/dsp/ScaleQuantizer.h
    Source/dsp/PitchShifter.cpp
    Source/dsp/PitchShifter.h
    Source/dsp/HarmonyEngine.cpp
    Source/dsp/HarmonyEngine.h
    Source/dsp/RetargetEnvelope.cpp
    Source/dsp/RetargetEnvelope.h
    Source/dsp/FormantPreserver.cpp
    Source/dsp/FormantPreserver.h
    Source/dsp/PitchCurve.cpp
    Source/dsp/PitchCurve.h
    Source/dsp/NoteUtils.h
    Source/ui/PitchVisualizer.cpp
    Source/ui/PitchVisualizer.h
    Source/ui/PitchCurveEditor.cpp
    Source/ui/PitchCurveEditor.h
    Source/ui/ScaleKeyboardComponent.cpp
    Source/ui/ScaleKeyboardComponent.h
    Source/ui/PianoKeyboard.cpp
    Source/ui/PianoKeyboard.h
    Source/ui/LookAndFeel.cpp
    Source/ui/LookAndFeel.h
    Source/external/presonus/ipsleditcontroller.h
)

# Modules JUCE requis.
target_link_libraries(OpenVoxTuner PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce_ara_sdk
)

# URL de mise a jour (JSON public sur GitHub raw).
if(NOT DEFINED OVT_UPDATE_INFO_URL)
    set(OVT_UPDATE_INFO_URL "https://raw.githubusercontent.com/EiffelBS/OpenVoxTuner/main/docs/releases/latest.json" CACHE STRING "Update info JSON URL")
endif()

# === Cible pour les tests unitaires (GTest-like console app) ===
juce_add_console_app(OpenVoxTunerTests
    PRODUCT_NAME "OpenVoxTunerTests"
    VERSION 0.1.0)

target_sources(OpenVoxTunerTests PRIVATE
    test/Main.cpp
    test/dsp/PitchDetectorTest.cpp
    test/dsp/ScaleQuantizerTest.cpp
    test/dsp/RetargetEnvelopeTest.cpp
    test/dsp/FormantPreserverTest.cpp
    test/dsp/PitchCurveTest.cpp
    
    # On lie les sources DSP a tester
    Source/dsp/PitchDetector.cpp
    Source/dsp/ScaleQuantizer.cpp
    Source/dsp/RetargetEnvelope.cpp
    Source/dsp/FormantPreserver.cpp
    Source/dsp/PitchCurve.cpp
    Source/dsp/PitchShifter.cpp
    Source/dsp/HarmonyEngine.cpp
)

target_link_libraries(OpenVoxTunerTests PRIVATE
    juce::juce_audio_basics
    juce::juce_events
    juce::juce_graphics
)
