// test_audio.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "audio/audio_processor.h"
#include "audio/wav_reader.h"
#include "audio/mp3_reader.h"
#include "ovtchord/ovtchord.h"
#include "test_harness.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
    double twoPi() { return 2.0 * std::acos (-1.0); }

    // Sum of sines (normalized by voice count).
    std::vector<float> makeChord (const std::vector<double>& freqs, double sr, int n)
    {
        std::vector<float> sig (static_cast<std::size_t> (n), 0.0f);
        for (int i = 0; i < n; ++i)
        {
            double s = 0.0;
            for (double f : freqs) s += std::sin (twoPi() * f * i / sr);
            sig[static_cast<std::size_t> (i)] = static_cast<float> (s / static_cast<double> (freqs.size()));
        }
        return sig;
    }

    // Minimal 16-bit mono WAV writer (for reader tests).
    void writeWav16 (const std::string& path, int sampleRate, const std::vector<float>& samples)
    {
        FILE* f = std::fopen (path.c_str(), "wb");
        if (f == nullptr) return;
        const uint32_t dataSize = static_cast<uint32_t> (samples.size()) * 2;
        const uint32_t riffSize = 36 + dataSize;
        uint8_t hdr[44] = { 0 };
        std::memcpy (hdr, "RIFF", 4);
        hdr[4] = riffSize & 0xFF; hdr[5] = (riffSize >> 8) & 0xFF; hdr[6] = (riffSize >> 16) & 0xFF; hdr[7] = (riffSize >> 24) & 0xFF;
        std::memcpy (hdr + 8, "WAVE", 4);
        std::memcpy (hdr + 12, "fmt ", 4);
        hdr[16] = 16; hdr[20] = 1; hdr[21] = 0;          // PCM
        hdr[22] = 1; hdr[23] = 0;                        // mono
        hdr[24] = sampleRate & 0xFF; hdr[25] = (sampleRate >> 8) & 0xFF; hdr[26] = (sampleRate >> 16) & 0xFF; hdr[27] = (sampleRate >> 24) & 0xFF;
        const uint32_t byteRate = static_cast<uint32_t> (sampleRate) * 2;
        hdr[28] = byteRate & 0xFF; hdr[29] = (byteRate >> 8) & 0xFF; hdr[30] = (byteRate >> 16) & 0xFF; hdr[31] = (byteRate >> 24) & 0xFF;
        hdr[32] = 2; hdr[33] = 0;                        // block align
        hdr[34] = 16; hdr[35] = 0;                       // bits
        std::memcpy (hdr + 36, "data", 4);
        hdr[40] = dataSize & 0xFF; hdr[41] = (dataSize >> 8) & 0xFF; hdr[42] = (dataSize >> 16) & 0xFF; hdr[43] = (dataSize >> 24) & 0xFF;
        std::fwrite (hdr, 1, 44, f);
        for (float s : samples)
        {
            const int16_t v = static_cast<int16_t> (s * 32767.0f);
            uint8_t b[2] = { static_cast<uint8_t> (v & 0xFF), static_cast<uint8_t> ((v >> 8) & 0xFF) };
            std::fwrite (b, 1, 2, f);
        }
        std::fclose (f);
    }
}

TEST (audio_processor_c_major)
{
    using namespace ovtchord;
    const double sr = 44100.0;
    AudioProcessor::Config cfg;
    cfg.windowSize = 4096;
    cfg.hopSize = 1024;
    cfg.sampleRate = sr;
    AudioProcessor ap (cfg);

    auto sig = makeChord ({ 261.63, 329.63, 392.00 }, sr, 8192); // C E G
    bool produced = false;
    for (int i = 0; i < static_cast<int> (sig.size()); i += 1024)
        if (ap.process (sig.data() + i, 1024)) produced = true;
    CHECK (produced);

    auto pcs = ap.lastPitchClasses();
    CHECK (pcs.size() == 3);
    if (pcs.size() == 3)
    {
        CHECK (pcs[0] == 0); // C
        CHECK (pcs[1] == 4); // E
        CHECK (pcs[2] == 7); // G
    }
}

TEST (audio_processor_g7)
{
    using namespace ovtchord;
    const double sr = 44100.0;
    AudioProcessor::Config cfg;
    cfg.windowSize = 4096;
    cfg.hopSize = 1024;
    cfg.sampleRate = sr;
    AudioProcessor ap (cfg);

    auto sig = makeChord ({ 196.00, 246.94, 293.66, 349.23 }, sr, 8192); // G B D F
    bool produced = false;
    for (int i = 0; i < static_cast<int> (sig.size()); i += 1024)
        if (ap.process (sig.data() + i, 1024)) produced = true;
    CHECK (produced);

    auto pcs = ap.lastPitchClasses();
    // G(7), B(11), D(2), F(5)
    CHECK (pcs.size() == 4);
    if (pcs.size() == 4)
    {
        CHECK (pcs[0] == 2); // D
        CHECK (pcs[1] == 5); // F
        CHECK (pcs[2] == 7); // G
        CHECK (pcs[3] == 11); // B
    }
}

TEST (wav_reader_16bit)
{
    using namespace ovtchord;
    const int sr = 44100;
    const int n = 1000;
    std::vector<float> sig (static_cast<std::size_t> (n));
    for (int i = 0; i < n; ++i)
        sig[static_cast<std::size_t> (i)] = static_cast<float> (std::sin (twoPi() * 440.0 * i / sr));

    const std::string path = "test_audio_tmp.wav";
    writeWav16 (path, sr, sig);

    WavData wav;
    CHECK (readWavFile (path, wav));
    if (wav.mono.size() > 0)
    {
        CHECK (wav.sampleRate == sr);
        CHECK (wav.numChannels == 1);
        CHECK (wav.bitsPerSample == 16);
        CHECK (wav.mono.size() == static_cast<std::size_t> (n));
        // Sample 0 should be near 0, sample at quarter period near +1.
        CHECK (std::abs (wav.mono[0]) < 0.05f);
        const int quarter = sr / (4 * 440);
        CHECK (wav.mono[static_cast<std::size_t> (quarter)] > 0.9f);
    }
    std::remove (path.c_str());
}

TEST (audio_api_file_c_major)
{
    using namespace ovtchord;
    const int sr = 44100;
    auto sig = makeChord ({ 261.63, 329.63, 392.00 }, sr, 44100); // ~1 s of C major

    const std::string path = "test_api_c.wav";
    writeWav16 (path, sr, sig);

    // Run through the public API.
    OvtChordConfig cfg;
    auto* h = ovtchord_init (&cfg);
    ovtchord_start (h);
    ovtchord_process_audio_file (h, path.c_str());
    const auto r = ovtchord_get_result (h);
    CHECK (r.valid);
    CHECK (r.symbol == "C");
    ovtchord_shutdown (h);

    std::remove (path.c_str());
}

TEST (mp3_reader_rejects_non_mp3)
{
    using namespace ovtchord;
    // A WAV file is not a valid MP3 -> the MP3 reader must report failure.
    const int sr = 44100;
    std::vector<float> sig (1000, 0.0f);
    const std::string path = "test_not_mp3.wav";
    writeWav16 (path, sr, sig);

    Mp3Data mp3;
    CHECK (! readMp3File (path, mp3));
    std::remove (path.c_str());
}
