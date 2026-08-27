# ============================================================================
# CMakeLists.txt - OpenVoxTuner
# Uses JUCE 8 native CMake support (does not require a compiled Projucer).
# Prerequisites: CMake >= 3.22 and Visual Studio 2022.
# ============================================================================

cmake_minimum_required(VERSION 3.22)

# Single source of truth for the plugin version.
# Override at build time with -DOVT_VERSION=1.2.3 (done by the release pipeline
# so the shipped binaries and the update checker agree with the git tag).
set(OVT_VERSION "0.1.0" CACHE STRING "OpenVoxTuner version (e.g. 0.1.61). Override with -DOVT_VERSION.")

project(OpenVoxTuner VERSION ${OVT_VERSION} LANGUAGES C CXX)

# C++17 standard (the standard Projucer uses for plugins).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Position Independent Code (needed by some plugin formats).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------
# Source encoding: force UTF-8 for MSVC so accented characters and
# non-Latin translations (Japanese, etc.) stored as literals in
# OVTLanguages.h are preserved. Without this flag, MSVC decodes the
# source as CP1252 and emits mojibake at compile time.
# ---------------------------------------------------------------
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ---------------------------------------------------------------
# macOS compatibility: force a deployment target compatible
# with JUCE 8 for builds on Xcode 17+ / SDK 26.x "insider".
# ---------------------------------------------------------------
if(APPLE)
    # Force the macOS SDK to a version compatible with JUCE 8.
    # JUCE 8 officially supports macOS 10.15+ (Catalina) / 11.0+ (Big Sur).
    # On an "insider" SDK 26.x, some APIs are deprecated and the flag
    # -Wunguarded-availability may raise errors. We pin the deployment
    # target so only APIs available since that version are used.
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS deployment target (forced to 11.0 for JUCE8 compatibility with SDK 26.x)")
    endif()
    # Disable unguarded-availability warnings
    # (caused by new APIs in SDK 26.x unknown to JUCE 8)
    add_compile_options(-Wno-unguarded-availability)
    
    message(STATUS "macOS build: deployment target = ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

# JUCE location (relative or absolute path).
if(NOT DEFINED JUCE_PATH)
    set(JUCE_PATH "C:/JUCE" CACHE PATH "Path to the JUCE repository")
endif()

# Load the JUCE CMake support.
add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/_juce_build)

# ---------------------------------------------------------------
# eiffelbs-ui: shared EiffelBS design system (header-only INTERFACE,
# pinned tag). Linking it carries onto every consumer:
#   - the ABI-critical defines JUCE_STRING_UTF_TYPE=8 /
#     JUCE_WEB_BROWSER=0 / JUCE_USE_CURL=0 (shared by BOTH plugin
#     binaries inside one DAW process),
#   - /std:c++20 (cxx_std_20) and MSVC /utf-8.
# Offline builds can point at a local checkout instead of GitHub:
#   cmake -B build -DFETCHCONTENT_SOURCE_DIR_EIFFELBS-UI=<checkout>
# (DASH spelled exactly like this - this name keeps its hyphen.)
# ---------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(eiffelbs-ui
    GIT_REPOSITORY https://github.com/EiffelBS/eiffelbs-ui.git
    GIT_TAG        v0.4.2
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(eiffelbs-ui)

# Standalone chord detection library (MIDI + audio). No JUCE / plugin dependency.
# Used by the plugin for real-time chord-aware tuning (MIDI + sidechain sources).
add_subdirectory(libs/ovtchord)

# ARA2 support (optional -- disable for hosts that do not support it,
# e.g. Ableton Live, FL Studio).
option(OVT_ENABLE_ARA "Build with ARA2 support" ON)
if(OVT_ENABLE_ARA)
    juce_set_ara_sdk_path(${CMAKE_CURRENT_SOURCE_DIR}/external/ARA_SDK)
endif()

# Plugin formats: VST3 + Standalone everywhere, AU/CLAP optional on macOS.
set(OVT_PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    option(OVT_ENABLE_AU "Build AU format on macOS" ON)
    if(OVT_ENABLE_AU)
        list(APPEND OVT_PLUGIN_FORMATS AU)
    endif()
    # CLAP is NOT available in JUCE 8.x: verified up to 8.0.14 (the latest
    # 8.x tag on juce-framework/JUCE) -- there is no CLAP module and no
    # plugin_client support. CLAP was added in a later JUCE release (post-8.x).
    # The getStringWidth -> GlyphArrangement port below is still required for any
    # JUCE >= 8.0.10 upgrade, so keep this wired but disabled until JUCE
    # is upgraded past 8.x.
    option(OVT_ENABLE_CLAP "Build CLAP format on macOS" OFF)
    if(OVT_ENABLE_CLAP)
        list(APPEND OVT_PLUGIN_FORMATS CLAP)
    endif()
endif()

# Create the plugin target.
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
    IS_ARA_EFFECT ${OVT_ENABLE_ARA}  # ARA2 support (optional, see OVT_ENABLE_ARA)
    # VST3 category: effect ("Fx") with the "Pitch" sub-category so DAWs (Cubase,
    # Nuendo, Studio One, ...) file the plugin under "Pitch & Time" alongside other
    # pitch-correction plug-ins (Auto-Tune, Melodyne, Graillon, ...). Without this,
    # JUCE defaults to a generic effect category and the plug-in lands in the
    # generic Effects folder instead.
    VST3_CATEGORIES "Fx" "Pitch"
    PRODUCT_NAME "OpenVoxTuner"
    VERSION ${OVT_VERSION}
    ICON_BIG "assets/icon.png"
    ICON_SMALL "assets/icon.png")

# NOTE on per-format bundle identifiers:
# JUCE assigns every plugin format (Standalone / VST3 / AU) the SAME
# CFBundleIdentifier, derived from the base target as "com.EiffelBS.OpenVoxTuner".
# The macOS Installer builds its <relocate> rules from that id, so three identical
# ids make it collapse the bundles and abort the install
# ("Unable to move 'OpenVoxTuner.component' to 'Applications'"). A clean fix needs
# a UNIQUE id per format. JUCE only exposes the base-target id (the per-format
# targets are not used for plist generation), so the unique ids are patched into
# each bundle's Info.plist at packaging time in scripts/build_macos_pkg.sh.
# (Host-facing plugin IDs come from PLUGIN_CODE/MANUFACTURER_CODE, not the bundle
# id, so this is safe.)

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
# registers and persists MIDI output routing between sessions -- the MIDI input
# bus is present but not processed by the plugin.
target_compile_definitions(OpenVoxTuner PRIVATE
    JUCE_PLUGIN_PRODUCES_MIDI_OUTPUT=1
    JUCE_PLUGIN_WANTS_MIDI_INPUT=1
    OVT_UPDATE_INFO_URL="${OVT_UPDATE_INFO_URL}"
    OVT_ARA_ENABLED=$<BOOL:${OVT_ENABLE_ARA}>
)

# Workaround for VST3 JUCE 8 Parameter ID Warning.
# The VST3 format expects hashed IDs, but JUCE generates its own (VST2 compat).
# Since we do NOT ship a VST2 build, this warning can safely be ignored
# (sessions will never reference a VST2 anyway).
target_compile_definitions(OpenVoxTuner PRIVATE
    JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1)

# Source paths.
target_sources(OpenVoxTuner PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/dsp/IPitchDetector.h
    Source/dsp/YinPitchDetector.cpp
    Source/dsp/YinPitchDetector.h
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
    Source/dsp/LpcFormantPreserver.cpp
    Source/dsp/LpcFormantPreserver.h
    Source/dsp/PitchCurve.cpp
    Source/dsp/PitchCurve.h
    Source/dsp/MidiImporter.cpp
    Source/dsp/MidiImporter.h
    Source/dsp/IEffect.h
    Source/dsp/ReverbEffect.cpp
    Source/dsp/ReverbEffect.h
    Source/dsp/FactoryPresets.cpp
    Source/dsp/FactoryPresets.h
    Source/ui/PresetGallery.cpp
    Source/ui/PresetGallery.h
    Source/ui/PitchVisualizer.cpp
    Source/ui/PitchVisualizer.h
    Source/ui/PitchCurveEditor.cpp
    Source/ui/PitchCurveEditor.h
    Source/ui/ScaleKeyboardComponent.cpp
    Source/ui/ScaleKeyboardComponent.h
    Source/ui/PianoKeyboard.cpp
    Source/ui/PianoKeyboard.h
    Source/external/presonus/ipsleditcontroller.h
)

# Embed the app logo (assets/icon.svg) so the UI can draw it as a vector
# Drawable at runtime (used by the OpenVoxTuner header and the OpenVoxKey editor).
juce_add_binary_data(OVTAssets
    SOURCES assets/icon.svg
    NAMESPACE ovtAssets
    HEADER_NAME ovt_assets_binary_data.h)

# Required JUCE modules.
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
    $<$<BOOL:${OVT_ENABLE_ARA}>:juce_ara_sdk>
    OVTAssets
    ovtchord
    eiffelbs-ui
)

# Update info URL (public JSON on GitHub raw).
if(NOT DEFINED OVT_UPDATE_INFO_URL)
    set(OVT_UPDATE_INFO_URL "https://raw.githubusercontent.com/EiffelBS/OpenVoxTuner/main/docs/releases/latest.json" CACHE STRING "Update info JSON URL")
endif()

# --- Install targets (system-wide plugin deployment) ---
# Allows `cmake --install build-mac` to deploy all plugin formats to
# /Library/Audio/Plug-Ins/.  Use `--component <name>` to install just
# one format.  Adjust the DESTINATION paths if you prefer the per-user
# ~/Library/Audio/Plug-Ins/ tree (drop the leading `/Library/`).
#
# Typical workflow after a successful build:
#   sudo cmake --install build-mac
#   sudo codesign --force --sign - \
#       /Library/Audio/Plug-Ins/Components/OpenVoxTuner.component \
#       /Library/Audio/Plug-Ins/VST3/OpenVoxTuner.vst3
#
# Available components: AU, VST3, Standalone, CompanionAU, CompanionVST3

# OpenVoxTuner
if(TARGET OpenVoxTuner_AU)
    install(TARGETS OpenVoxTuner_AU
        COMPONENT AU
        DESTINATION "/Library/Audio/Plug-Ins/Components")
endif()

if(TARGET OpenVoxTuner_VST3)
    install(TARGETS OpenVoxTuner_VST3
        COMPONENT VST3
        DESTINATION "/Library/Audio/Plug-Ins/VST3")
endif()

if(TARGET OpenVoxTuner_Standalone)
    install(TARGETS OpenVoxTuner_Standalone
        COMPONENT Standalone
        DESTINATION "/Applications")
endif()

# OpenVoxKey (companion plugin)
if(TARGET OpenVoxKey_AU)
    install(TARGETS OpenVoxKey_AU
        COMPONENT CompanionAU
        DESTINATION "/Library/Audio/Plug-Ins/Components")
endif()

if(TARGET OpenVoxKey_VST3)
    install(TARGETS OpenVoxKey_VST3
        COMPONENT CompanionVST3
        DESTINATION "/Library/Audio/Plug-Ins/VST3")
endif()

# === Companion key-detection plug-in (OpenVoxKey) ===========================
# A tiny, transparent "effect" placed on an accompaniment track. It detects the
# musical key/scale and publishes it to the shared ovtdsp::KeyBridge so the main
# OpenVoxTuner instance (Key Source = "Companion") can pick it up. It passes the
# audio through unchanged so the accompaniment keeps playing.
# Companion formats: only DAW plug-in formats (VST3/AU). A Standalone makes no
# sense here -- the companion needs a host track to analyse and an OpenVoxTuner
# instance to consume the detected key, neither of which exists autonomously.
set(OVT_COMPANION_FORMATS VST3)
if(APPLE)
    if(OVT_ENABLE_AU)
        list(APPEND OVT_COMPANION_FORMATS AU)
    endif()
    if(OVT_ENABLE_CLAP)
        list(APPEND OVT_COMPANION_FORMATS CLAP)
    endif()
endif()
juce_add_plugin(OpenVoxKey
    COMPANY_NAME "EiffelBS"
    PLUGIN_MANUFACTURER_CODE Eiff
    PLUGIN_CODE OvtK
    FORMATS ${OVT_COMPANION_FORMATS}
    VST3_CATEGORIES "Fx" "Pitch"
    PRODUCT_NAME "OpenVoxKey"
    VERSION ${OVT_VERSION})

target_compile_definitions(OpenVoxKey PRIVATE
    JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1)

# Allow "#include "dsp/..."" from the companion sources (they live in Source/companion/).
target_include_directories(OpenVoxKey PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_sources(OpenVoxKey PRIVATE
    Source/companion/OpenVoxKeyProcessor.cpp
    Source/companion/OpenVoxKeyProcessor.h
    Source/companion/OpenVoxKeyEditor.h
    Source/dsp/IPitchDetector.h
    Source/dsp/YinPitchDetector.cpp
    Source/dsp/YinPitchDetector.h
    Source/dsp/KeyDetector.h
    Source/dsp/KeyBridge.h
    Source/dsp/ScaleQuantizer.cpp
    Source/dsp/ScaleQuantizer.h
)

target_link_libraries(OpenVoxKey PRIVATE
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
    OVTAssets
    eiffelbs-ui
)

# === Unit test target (GTest-like console app) ===
# Main.cpp includes all test files via #include, so we only compile Main.cpp
juce_add_console_app(OpenVoxTunerTests
    PRODUCT_NAME "OpenVoxTunerTests"
    VERSION 0.1.0)

target_sources(OpenVoxTunerTests PRIVATE
    test/Main.cpp

    # DSP sources to test (linked so tests can access internals)
    Source/dsp/ScaleQuantizer.cpp
    Source/dsp/RetargetEnvelope.cpp
    Source/dsp/FormantPreserver.cpp
    Source/dsp/LpcFormantPreserver.cpp
    Source/dsp/PitchCurve.cpp
    Source/dsp/MidiImporter.cpp
    Source/dsp/PitchShifter.cpp
    Source/dsp/HarmonyEngine.cpp
    Source/ui/ScaleKeyboardComponent.cpp
)
target_link_libraries(OpenVoxTunerTests PRIVATE
    eiffelbs-ui                       # PitchCurveEditor.h consumes the theme API
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
)
