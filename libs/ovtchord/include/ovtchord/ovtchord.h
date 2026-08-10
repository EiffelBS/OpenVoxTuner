// ovtchord.h
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Public entry point. This is a static, plugin-independent C API: no classes
// to instantiate, no plugin / JUCE / UI dependency.

#pragma once

#include "ovtchord/types.h"
#include <cstddef>

#ifdef _WIN32
  #ifdef OVTCHORD_BUILDING_DLL
    #define OVTCHORD_API __declspec (dllexport)
  #else
    #define OVTCHORD_API
  #endif
#else
  #define OVTCHORD_API
#endif

namespace ovtchord
{
    // Configuration for a detection context.
    struct OvtChordConfig
    {
        int minNotes = 3;          // minimum notes to consider a chord
        float minConfidence = 0.5f;
        int stabilityFrames = 1;   // consecutive frames for a stable result
    };

    // Opaque handle to a detection context (one per source/track).
    typedef struct OvtChordImpl* OvtChordHandle;

    // Callback invoked on each stable chord detection. `userData` is the
    // pointer passed to ovtchord_set_callback.
    typedef void (*ChordCallback) (const ChordResult& result, void* userData);

    // Create a detection context. `config` may be null to use defaults.
    OVTCHORD_API OvtChordHandle ovtchord_init (const OvtChordConfig* config);

    // Destroy a context.
    OVTCHORD_API void ovtchord_shutdown (OvtChordHandle h);

    // Start / stop detection. While stopped, process_* calls are ignored.
    OVTCHORD_API void ovtchord_start (OvtChordHandle h);
    OVTCHORD_API void ovtchord_stop (OvtChordHandle h);

    // Register a callback (may be null to disable).
    OVTCHORD_API void ovtchord_set_callback (OvtChordHandle h, ChordCallback cb, void* userData);

    // Feed a raw MIDI 1.0 byte stream. `timestampMs` is stamped onto events.
    OVTCHORD_API void ovtchord_process_midi (OvtChordHandle h, const uint8_t* data, std::size_t size, double timestampMs);

    // Feed already-parsed MIDI events.
    OVTCHORD_API void ovtchord_process_midi_events (OvtChordHandle h, const MidiEvent* events, std::size_t count);

    // Get the last detection result.
    OVTCHORD_API ChordResult ovtchord_get_result (OvtChordHandle h);

    // --- Audio ---

    // Feed an audio frame (real-time). Detection runs on the internal sliding
    // window with a "progressive" output (see plan §3.2).
    OVTCHORD_API void ovtchord_process_audio (OvtChordHandle h, const float* samples,
                                              std::size_t numSamples, double sampleRate,
                                              double timestampMs);

    // Decode a WAV file and run detection over it (last result = end of file).
    // MP3 support is a TODO (needs minimp3).
    OVTCHORD_API void ovtchord_process_audio_file (OvtChordHandle h, const char* path);
}
