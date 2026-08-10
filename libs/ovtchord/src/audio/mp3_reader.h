// mp3_reader.h
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Internal module: MP3 decoder (vendored minimp3). Not part of the public API.

#pragma once

#include <string>
#include <vector>

namespace ovtchord
{
    struct Mp3Data
    {
        int sampleRate = 0;
        int numChannels = 0;
        std::vector<float> mono; // mono downmix, normalized [-1, 1]
    };

    // Decodes an MP3 file to mono floats. Returns false on failure.
    bool readMp3File (const std::string& path, Mp3Data& out);
}