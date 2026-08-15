// wav_reader.h
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Internal module: WAV (PCM 16/24-bit) reader. MP3 is a TODO (needs minimp3).
// Not part of the public API.

#pragma once

#include <string>
#include <vector>

namespace ovtchord
{
    struct WavData
    {
        int sampleRate = 0;
        int numChannels = 0;
        int bitsPerSample = 0;
        std::vector<float> mono; // mono downmix, normalized [-1, 1]
    };

    // Reads a PCM WAV file (16/24-bit, mono/stereo). Returns false on failure.
    bool readWavFile (const std::string& path, WavData& out);
}