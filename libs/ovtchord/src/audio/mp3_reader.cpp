// mp3_reader.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#include "minimp3_ex.h"

#include "audio/mp3_reader.h"
#include <vector>

namespace ovtchord
{
    bool readMp3File (const std::string& path, Mp3Data& out)
    {
        mp3dec_ex_t dec;
        if (mp3dec_ex_open (&dec, path.c_str(), MP3D_SEEK_TO_SAMPLE) != 0)
            return false;

        const int channels = dec.info.channels;
        const int hz = dec.info.hz;
        if (channels <= 0 || hz <= 0)
        {
            mp3dec_ex_close (&dec);
            return false;
        }

        out.sampleRate = hz;
        out.numChannels = channels;

        const std::size_t frameSamples = static_cast<std::size_t> (channels) * 2048;
        std::vector<mp3d_sample_t> buf (frameSamples, 0);
        std::vector<float> acc;
        const std::size_t totalFrames = (dec.samples > 0)
            ? static_cast<std::size_t> (dec.samples / static_cast<uint64_t> (channels))
            : 0;
        acc.reserve (totalFrames);

        std::size_t read;
        while ((read = mp3dec_ex_read (&dec, buf.data(), frameSamples)) > 0)
        {
            const std::size_t n = read / static_cast<std::size_t> (channels);
            for (std::size_t s = 0; s < n; ++s)
            {
                double sum = 0.0;
                for (int c = 0; c < channels; ++c)
                    sum += static_cast<double> (buf[s * static_cast<std::size_t> (channels) + static_cast<std::size_t> (c)]) / 32768.0;
                acc.push_back (static_cast<float> (sum / static_cast<double> (channels)));
            }
        }
        mp3dec_ex_close (&dec);

        out.mono = std::move (acc);
        return ! out.mono.empty();
    }
}