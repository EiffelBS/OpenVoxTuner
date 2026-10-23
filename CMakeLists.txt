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
#   - /std:c++20 (cxx_std_20) and MSVC /utf-8,
#   - since v0.12.7, juce::juce_gui_extra (the library's LogWindow
#     lives there). Every target linking eiffelbs-ui must therefore
#     list juce_gui_extra itself - do NOT drop it from the module
#     lists below, the failure is "target juce::juce_gui_extra not
#     found" at configure time.
# Offline builds can point at a local checkout instead of GitHub:
#   cmake -B build -DFETCHCONTENT_SOURCE_DIR_EIFFELBS-UI=<checkout>
# (DASH spelled exactly like this - this name keeps its hyphen.)
# The override lives in the local CMake cache, so it OUTRANKS the pinned tag:
# a build configured against a stale copy compiles that copy and the GIT_TAG
# line is decorative. Clear it with an empty value to make the pin authoritative.
# ---------------------------------------------------------------
include(FetchContent)
# The bottom-row frames use ebs::FrameTitleBand (a left vertical title spine naming a frame), introduced in v0.15.11;
# v0.15.14 draws its captions as accent-free tracked capitals and v0.15.15 measures every shipped room name against
# the fixed cell floor. v0.17.0 gave a fold an axis (ExpansibleFrame::setFoldVertical, default horizontal, so every
# frame here is unchanged) and v0.17.1 adds ebs::ActivityDot (one dot per signal path, sized by the level that path
# carried by ebs::ActivityKnob inside the knob of the component it reports). v0.17.4 to v0.17.7 added ebs::TokenField, a DataList text scale
# and two button glyph tweaks, none of which this plugin uses yet. The pin must stay at or above
# the newest feature used, and adding components is a patch bump.
FetchContent_Declare(eiffelbs-ui
    GIT_REPOSITORY https://github.com/EiffelBS/eiffelbs-ui.git
    GIT_TAG        v0.17.7
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
    # Standalone build must embed NSMicrophoneUsageDescription in its Info.plist.
    # Without this key macOS denies microphone access with no permission prompt
    # (observed on a fresh macOS rebuild: input device opens, capture stays silent).
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "OpenVoxTuner needs microphone access to analyse and correct your pitch."
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
# NOMINMAX: kept as protection, not as a library requirement. This TU includes
# <windows.h> BARE through the ARA SDK and the VST3 plugin-client chain
# (Source/PluginProcessor.cpp L63), so the Windows min/max function-like macros are
# live, and juce_UMPEndpoint.h calls std::min WITHOUT parentheses: MSVC fails
# (C2589 + C2059). It was REQUIRED from eiffelbs-ui v0.12.26, whose umbrella
# included Waveform.h / ThumbnailPool.h and made every consumer TU compile
# juce_audio_devices; since v0.12.27 those two widgets are opt-in includes, so the
# library no longer forces the define (measured: the tests target builds and its
# 136 tests pass with NOMINMAX removed). Killing the macros is also what
# Source/dsp/BlockAwareOnePole.h has been working around by hand.
target_compile_definitions(OpenVoxTuner PRIVATE
    JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
    NOMINMAX)

# Source paths.
target_sources(OpenVoxTuner PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    # The editor is split by domain. This TU holds only the HelpOverlayComponent
    # definitions; the class stays declared in PluginEditor.h.
    Source/PluginEditorHelp.cpp
    Source/dsp/IPitchDetector.h
    Source/dsp/YinPitchDetector.cpp
    Source/dsp/YinPitchDetector.h
    Source/dsp/ScaleQuantizer.cpp
    Source/dsp/ScaleQuantizer.h
    Source/dsp/PitchShifter.cpp
    Source/dsp/PitchShifter.h
    Source/dsp/HarmonyEngine.cpp
    Source/dsp/HarmonyEngine.h
    Source/dsp/Doubler.cpp
    Source/dsp/Doubler.h
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
    # Audio file -> pitch curve import (waveform decoding via juce_audio_formats,
    # then the same YIN detector in offline mode).
    Source/dsp/AudioCurveImporter.cpp
    Source/dsp/AudioCurveImporter.h
    Source/dsp/IEffect.h
    Source/dsp/ReverbEffect.cpp
    Source/dsp/ReverbEffect.h
    # Tempo-synced ping-pong delay on the lead bus. It is an in-place stage: it
    # declares no latency, and the shared room is fed by the existing lead send tap.
    Source/dsp/DelayEffect.cpp
    Source/dsp/DelayEffect.h
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
    # Self-contained editor components that used to live inside PluginEditor.cpp.
    # Each one defines every member inline, so there is no .cpp to list.
    Source/ui/CorrectionModeSwitch.h
    Source/ui/AudioImportDialogComponent.h
    Source/ui/MidiChannelSelectorComponent.h
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
    juce::juce_gui_extra              # required by eiffelbs-ui >= v0.12.7 (its LogWindow); keep
    $<$<BOOL:${OVT_ENABLE_ARA}>:juce_ara_sdk>
    OVTAssets
    ovtchord
    eiffelbs-ui
)

# Update info URL (public JSON on GitHub raw).
if(NOT DEFINED OVT_UPDATE_INFO_URL)
    set(OVT_UPDATE_INFO_URL "https://raw.githubusercontent.com/EiffelBS/OpenVoxTuner/main/docs/releases/latest.json" CACHE STRING "Update info JSON URL")
endif()

# === Standalone re-signing (macOS microphone permission / TCC) ===============
# macOS keys the microphone permission to the app's CODE IDENTITY, not to its
# path. JUCE signs the Standalone ad-hoc, whose identity is the binary's own
# cdhash: it changes on EVERY relink, so macOS treats each rebuild as a new app
# and silently re-denies mic capture - the device opens and the samples stay
# zero, with no permission prompt (observed on macOS 26/27 betas, see README).
# Re-signing with a stable identity (Developer ID Application) keys TCC to
# bundle-id + team instead: the prompt appears once and survives future
# rebuilds. scripts/build_macos.sh does the same in its step 2b - this
# POST_BUILD step exists so a plain `ninja` / `cmake --build` cycle does not
# drop the grant again between scripted builds.
#
# Only the Standalone needs it: VST3/AU are loaded inside a DAW, and the host's
# own TCC permission covers the audio input.
#
# OVT_CODESIGN_IDENTITY:
#   "auto" (default) - the first "Developer ID Application" identity in the
#                      keychain; nothing happens if the machine has none, so a
#                      contributor without a certificate keeps JUCE's ad-hoc
#                      signature (and simply re-accepts the mic prompt).
#   <identity>       - used verbatim, e.g.
#                      -DOVT_CODESIGN_IDENTITY="Developer ID Application: NAME (TEAM)"
#   "-"              - explicit fresh ad-hoc signature, i.e. the old behaviour.
# Set the same name in the environment to override it (as build_macos.sh does).
# ===
set(OVT_CODESIGN_IDENTITY "auto" CACHE STRING
    "macOS: identity the Standalone is re-signed with after each link (\"auto\", \"-\", or an identity)")

if(APPLE AND TARGET OpenVoxTuner_Standalone)
    set(_ovt_sign_identity "${OVT_CODESIGN_IDENTITY}")

    if(NOT _ovt_sign_identity AND DEFINED ENV{OVT_CODESIGN_IDENTITY})
        set(_ovt_sign_identity "$ENV{OVT_CODESIGN_IDENTITY}")
    endif()

    if(_ovt_sign_identity STREQUAL "auto")
        # `security` prints each identity as "<hash> \"Developer ID Application: ...\"".
        execute_process(
            COMMAND security find-identity -p codesigning -v
            OUTPUT_VARIABLE _ovt_identity_list
            ERROR_QUIET)
        string(REGEX MATCH "\"Developer ID Application:[^\"]*\"" _ovt_identity_match "${_ovt_identity_list}")
        string(REPLACE "\"" "" _ovt_sign_identity "${_ovt_identity_match}")
    endif()

    if(_ovt_sign_identity)
        # TARGET_BUNDLE_DIR is the assembled .app, exists by the time POST_BUILD runs.
        # --force is required: JUCE's linker signature is already in place.
        add_custom_command(TARGET OpenVoxTuner_Standalone POST_BUILD
            COMMAND codesign --force --sign "${_ovt_sign_identity}"
                    "$<TARGET_BUNDLE_DIR:OpenVoxTuner_Standalone>"
            COMMENT "Re-signing Standalone with '${_ovt_sign_identity}' (stable TCC mic permission)"
            VERBATIM)
    else()
        message(STATUS "OpenVoxTuner: no Developer ID identity found - the Standalone "
                       "keeps JUCE's ad-hoc signature and macOS re-asks for the mic "
                       "after every rebuild (set OVT_CODESIGN_IDENTITY to fix).")
    endif()
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
    juce::juce_gui_extra              # required by eiffelbs-ui >= v0.12.7 (its LogWindow); keep
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
    # Audio import (the tests generate WAVs, decode them and check the detected
    # notes). The module reuses YinPitchDetector in offline mode.
    Source/dsp/AudioCurveImporter.cpp
    Source/dsp/YinPitchDetector.cpp
    Source/dsp/PitchShifter.cpp
    Source/dsp/HarmonyEngine.cpp
    Source/dsp/Doubler.cpp
    # The shared send room: the test drives ReverbEffect directly, so its TU is compiled
    # here. juce::Reverb lives in juce_dsp, which this target did not link before.
    Source/dsp/ReverbEffect.cpp
    # The lead delay, driven directly by DelayEffectTest: it needs the same TU
    # reachable from the console app, plus juce_dsp for LinearSmoothedValue.
    Source/dsp/DelayEffect.cpp
    Source/ui/ScaleKeyboardComponent.cpp
)
target_link_libraries(OpenVoxTunerTests PRIVATE
    eiffelbs-ui                       # PitchCurveEditor.h consumes the theme API
    juce::juce_audio_basics
    juce::juce_audio_formats          # AudioCurveImporter decodes WAV/AIFF/FLAC/OGG
    juce::juce_audio_processors
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_extra              # eiffelbs-ui >= v0.12.7 links it INTERFACE
    juce::juce_dsp                    # ReverbEffect.cpp uses juce::Reverb
)

# NOMINMAX is kept as protection for this project's own TUs, which include
# <windows.h> bare (Source/PluginProcessor.cpp L63, Source/dsp/KeyBridge.h L16,
# via the ARA SDK and the VST3 plugin-client chain). It was REQUIRED from
# eiffelbs-ui v0.12.26, whose umbrella included Waveform.h + ThumbnailPool.h and
# so compiled juce_audio_devices in every consumer TU: juce_UMPEndpoint.h calls
# std::min WITHOUT parentheses, which the Windows min/max macros turn into
# `std::(...)` (MSVC C2589 + C2059). Measured on v0.12.27, where the two audio
# widgets are opt-in includes: this target builds and its 136 tests pass with the
# define REMOVED, so eiffelbs-ui no longer forces it. Keeping it costs nothing and
# covers any future path that reaches juce_audio_devices.
target_compile_definitions(OpenVoxTunerTests PRIVATE NOMINMAX
    # MorphCoverageTest reads the parameter declarations out of the processor source, so the
    # tests target has to know where that source lives.
    OVT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Source")

# === LPC formant framing benchmark (console app) ===
# tools/bench_formant_lsd.cpp — log-spectral distortion (LSD) of the LPC
# cross-synthesis on synthetic vowels at r = 0.5/1.5/2.0, across DAW block
# sizes 64..1024, plus the CPU cost per audio second. Runs in CI next to the
# unit tests (see .github/workflows/ci.yml, test job).
juce_add_console_app(bench_formant_lsd
    PRODUCT_NAME "bench_formant_lsd"
    VERSION 0.1.0)

target_sources(bench_formant_lsd PRIVATE
    tools/bench_formant_lsd.cpp
    Source/dsp/LpcFormantPreserver.cpp
)
target_link_libraries(bench_formant_lsd PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_data_structures
    juce::juce_events
)

# === De-Esser placement benchmark (console app) ===
# tools/bench_deesser_placement.cpp — compares the shipped order (de-esser before
# ReverbEffect, so the corrected lead voice is de-essed) against de-essing after
# the reverb (which also de-esses the wet tail). Reports sibilance cut, total
# level, tail brightness and tail pumping, all measured on the output.
juce_add_console_app(bench_deesser_placement
    PRODUCT_NAME "bench_deesser_placement"
    VERSION 0.1.0)

target_sources(bench_deesser_placement PRIVATE
    tools/bench_deesser_placement.cpp
    Source/dsp/ReverbEffect.cpp
)
target_link_libraries(bench_deesser_placement PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_data_structures
    juce::juce_events
    juce::juce_dsp
)
# === Low-latency pitch-shifter analysis benchmark (console app) ===
# tools/bench_pitchshift_latency.cpp - Phase 3 of the Hard Tune plan: measures
# the shipped pitch-synchronous OLA shifter at declared latencies 8/10/12/20/30
# ms and at fundamentals 82..400 Hz (declared vs EFFECTIVE latency, CPU, level,
# clicks, pitch accuracy, spectral distortion), and prices the same metrics on a
# two-tap crossfade delay-line prototype that reaches ~2.5-4 ms. Informational
# tool: it prints the tables and fails only its own sanity gates.
juce_add_console_app(bench_pitchshift_latency
    PRODUCT_NAME "bench_pitchshift_latency"
    VERSION 0.1.0)

target_sources(bench_pitchshift_latency PRIVATE
    tools/bench_pitchshift_latency.cpp
    Source/dsp/PitchShifter.cpp
    Source/dsp/YinPitchDetector.cpp
)
target_link_libraries(bench_pitchshift_latency PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_data_structures
    juce::juce_events
    juce::juce_dsp
)

# === Hard Tune shifter A/B renderer (console app) ===
# tools/render_hard_tune_ab.cpp - renders ONE Hard Tune correction trajectory
# (short YIN window, chromatic snap, no smoothing) through the shipped OLA
# shifter and through the two-tap low-latency probe, then writes anonymised WAVs
# plus a key so the shifter difference can be judged by ear on a real take.
# Analysis tool: not part of the plugin, and no audio file is committed.
juce_add_console_app(render_hard_tune_ab
    PRODUCT_NAME "render_hard_tune_ab"
    VERSION 0.1.0)

target_sources(render_hard_tune_ab PRIVATE
    tools/render_hard_tune_ab.cpp
    Source/dsp/PitchShifter.cpp
    Source/dsp/YinPitchDetector.cpp
)
target_link_libraries(render_hard_tune_ab PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_data_structures
    juce::juce_events
)

# === Hard Tune vibrato-residual benchmark (console app) ===
# tools/bench_vibrato_residual.cpp - how much of a 3-7 Hz vibrato survives the
# Hard Tune chain (short YIN window, chromatic snap, instant ratio), and which
# part of the residual comes from the detector vs the shifter's own f0
# smoothing. Informational: prints tables and fails only its own sanity gates.
juce_add_console_app(bench_vibrato_residual
    PRODUCT_NAME "bench_vibrato_residual"
    VERSION 0.1.0)

target_sources(bench_vibrato_residual PRIVATE
    tools/bench_vibrato_residual.cpp
    Source/dsp/PitchShifter.cpp
    Source/dsp/YinPitchDetector.cpp
)
target_link_libraries(bench_vibrato_residual PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_data_structures
    juce::juce_events
)
