// wav_reader.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "audio/wav_reader.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

namespace ovtchord
{
    namespace
    {
        uint32_t readU32LE (const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); }
        uint16_t readU16LE (const uint8_t* p) { return static_cast<uint16_t> (p[0] | (p[1] << 8)); }
        bool hasTag (const uint8_t* p, const char* tag) { return std::memcmp (p, tag, 4) == 0; }

        int32_t readS24LE (const uint8_t* p)
        {
            // 24-bit little-endian, sign-extend to 32-bit.
            uint32_t v = p[0] | (p[1] << 8) | (p[2] << 16);
            if (v & 0x800000) v |= 0xFF000000u;
            return static_cast<int32_t> (v);
        }
    }

    bool readWavFile (const std::string& path, WavData& out)
    {
        FILE* f = std::fopen (path.c_str(), "rb");
        if (f == nullptr) return false;

        std::fseek (f, 0, SEEK_END);
        const long size = std::ftell (f);
        std::fseek (f, 0, SEEK_SET);
        if (size < 12) { std::fclose (f); return false; }

        std::vector<uint8_t> buf (static_cast<std::size_t> (size));
        const std::size_t rd = std::fread (buf.data(), 1, static_cast<std::size_t> (size), f);
        std::fclose (f);
        if (rd != static_cast<std::size_t> (size)) return false;

        if (! hasTag (buf.data(), "RIFF") || ! hasTag (buf.data() + 8, "WAVE"))
            return false;

        int fmtChannels = 0, fmtRate = 0, fmtBits = 0, fmtFormat = 0;
        const uint8_t* dataPtr = nullptr;
        std::size_t dataSize = 0;

        for (std::size_t off = 12; off + 8 <= buf.size(); )
        {
            const char* id = reinterpret_cast<const char*> (buf.data() + off);
            const std::size_t chunkSize = readU32LE (buf.data() + off + 4);
            const std::size_t body = off + 8;

            if (std::memcmp (id, "fmt ", 4) == 0 && body + chunkSize <= buf.size())
            {
                fmtFormat = readU16LE (buf.data() + body);
                fmtChannels = readU16LE (buf.data() + body + 2);
                fmtRate = static_cast<int> (readU32LE (buf.data() + body + 4));
                fmtBits = readU16LE (buf.data() + body + 14);
            }
            else if (std::memcmp (id, "data", 4) == 0 && body + chunkSize <= buf.size())
            {
                dataPtr = buf.data() + body;
                dataSize = chunkSize;
            }
            // Advance to the next chunk (chunks are word-aligned).
            off = body + chunkSize + (chunkSize & 1);
        }

        if (fmtFormat != 1 || fmtChannels <= 0 || fmtRate <= 0 || dataPtr == nullptr)
            return false;
        if (fmtBits != 16 && fmtBits != 24)
            return false;

        const int bytesPerSample = fmtBits / 8;
        const int numFrames = static_cast<int> (dataSize / (static_cast<std::size_t> (bytesPerSample) * fmtChannels));

        out.sampleRate = fmtRate;
        out.numChannels = fmtChannels;
        out.bitsPerSample = fmtBits;
        out.mono.resize (static_cast<std::size_t> (numFrames));

        for (int fr = 0; fr < numFrames; ++fr)
        {
            double sum = 0.0;
            for (int ch = 0; ch < fmtChannels; ++ch)
            {
                const uint8_t* p = dataPtr
                    + static_cast<std::size_t> (fr * fmtChannels + ch) * bytesPerSample;
                double s;
                if (fmtBits == 16)
                    s = static_cast<double> (static_cast<int16_t> (readU16LE (p))) / 32768.0;
                else
                    s = static_cast<double> (readS24LE (p)) / 8388608.0;
                sum += s;
            }
            out.mono[static_cast<std::size_t> (fr)] = static_cast<float> (sum / fmtChannels);
        }
        return true;
    }
}