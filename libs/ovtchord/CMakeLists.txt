# ============================================================================
# ovtchord — standalone chord detection library (MIDI + audio)
# Autonomous CMake target. No link to the OpenVoxTuner plugin or JUCE.
#
# Usage:
#   cmake -S libs/ovtchord -B build-ovtchord -DOVTCHORD_BUILD_TESTS=ON
#   cmake --build build-ovtchord
#   ctest --test-dir build-ovtchord
# ============================================================================

cmake_minimum_required(VERSION 3.22)
project(ovtchord LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(OVTCHORD_BUILD_TESTS "Build ovtchord unit tests" OFF)
option(OVTCHORD_BUILD_BENCHMARK "Build ovtchord chord benchmark" OFF)

add_library(ovtchord STATIC
    src/midi/midi_parser.cpp
    src/midi/note_tracker.cpp
    src/core/chord_engine.cpp
    src/core/chord_templates.cpp
    src/api/ovtchord.cpp
    src/export/export.cpp
    src/audio/fft.cpp
    src/audio/chroma.cpp
    src/audio/preprocess.cpp
    src/audio/audio_processor.cpp
    src/audio/wav_reader.cpp
    src/audio/mp3_reader.cpp
)
target_include_directories(ovtchord PUBLIC include PRIVATE src external/minimp3)
target_compile_features(ovtchord PUBLIC cxx_std_17)

if(OVTCHORD_BUILD_TESTS)
    enable_testing()
    add_executable(ovtchord_tests
        tests/unit/test_main.cpp
        tests/unit/test_midi_parser.cpp
        tests/unit/test_chord_engine.cpp
        tests/unit/test_audio.cpp
    )
    target_link_libraries(ovtchord_tests PRIVATE ovtchord)
    target_include_directories(ovtchord_tests PRIVATE tests/unit src)
    add_test(NAME ovtchord_tests COMMAND ovtchord_tests)
endif()

if(OVTCHORD_BUILD_BENCHMARK)
    add_executable(ovtchord_benchmark tools/benchmark_chords.cpp)
    target_link_libraries(ovtchord_benchmark PRIVATE ovtchord)
    target_include_directories(ovtchord_benchmark PRIVATE src)
endif()
