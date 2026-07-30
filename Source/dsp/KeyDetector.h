// KeyDetector.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

namespace ovtdsp
{
    class KeyDetector
    {
    public:
        void reset()
        {
            profile.fill (0.0f);
            stableCount = 0;
            lastKey = -1;
            lastMinor = false;
        }

        /** Set the sliding-window length in seconds (how much history the
         *  pitch-class profile remembers). */
        void setWindowSeconds (float s) noexcept { windowSec = juce::jmax (0.5f, s); }

        /** Feed one analysis block. Call every block.
         *  @param f0Hz     detected pitch in Hz (<= 0 means silence -> just decay).
         *  @param strength contribution weight in [0,1] (use detection confidence).
         *  @param blockDurSec duration of the block in seconds.
         *
         *  The pitch-class histogram is an exponential moving average (EMA) of
         *  occurrence: each block nudges the detected class by a small coefficient
         *  `a = 1 - exp(-dt/window)`, so a single note adds only a tiny transient
         *  and the histogram reflects the LONG-TERM key, not the last note sung.
         *  This is what lets the detector stay stable while a melody moves around
         *  (including the relative-major/minor ambiguity). */
        void addDetection (float f0Hz, float strength, float blockDurSec) noexcept
        {
            const float a = 1.0f - std::exp (-blockDurSec / windowSec);
            const float keep = 1.0f - a;
            for (float& v : profile)
                v *= keep;

            if (f0Hz > 0.0f && strength > 0.0f)
            {
                const float semi = 12.0f * std::log2 (f0Hz / 440.0f); // relative to A4
                const int pc = (static_cast<int> (std::round (semi)) % 12 + 12) % 12;
                profile[pc] += a * strength;
            }
        }

        /** Compute the best estimate. Returns true only when a stable, confident
         *  estimate is available (so callers can avoid flapping the key/scale). */
        bool getEstimate (int& outKey, bool& outMinor, float& outConfidence) noexcept
        {
            float best = -1.0f, second = -1.0f;
            int  bestKey = 0;
            bool bestMinor = false;

            for (int key = 0; key < 12; ++key)
            {
                for (int m = 0; m < 2; ++m)
                {
                    const float* tpl = (m == 0) ? kMajorTemplate : kMinorTemplate;
                    const float corr = correlate (key, tpl);
                    if (corr > best)        { second = best; best = corr; bestKey = key; bestMinor = (m == 1); }
                    else if (corr > second) { second = corr; }
                }
            }

            const float total = sum();
            outConfidence = (total > 1.0e-3f) ? (best - second) / (best + 1.0e-6f) : 0.0f;

            if (outConfidence < kMinConfidence)
            {
                stableCount = 0;
                return false;
            }
            if (bestKey == lastKey && bestMinor == lastMinor)
                ++stableCount;
            else
            {
                stableCount = 0;
                lastKey = bestKey;
                lastMinor = bestMinor;
            }
            if (stableCount < kStableUpdates)
                return false;

            outKey = bestKey;
            outMinor = bestMinor;
            return true;
        }

        // The pitch-class profile is A-relative (A = pc 0, because it is derived
        // from 12*log2(hz/440)). Convert a detected key (0=A .. 11=G#) to the
        // musical key used by the plug-in's "key" parameter (0=C .. 11=B).
        static int detectorKeyToMusical (int detectorKey) noexcept
        {
            return (detectorKey + 9) % 12;
        }

    private:
        // Krumhansl-Schmuckler tonic profiles (root weight first).
        static constexpr float kMajorTemplate[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
        static constexpr float kMinorTemplate[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
        static constexpr float kMinConfidence = 0.03f;  // require a clear winner
        static constexpr int   kStableUpdates  = 8;      // ~8 blocks of stability

        std::array<float, 12> profile{};
        float windowSec = 3.0f;
        int   stableCount = 0;
        int   lastKey = -1;
        bool  lastMinor = false;

        float correlate (int key, const float* tpl) const noexcept
        {
            float s = 0.0f;
            for (int i = 0; i < 12; ++i)
            {
                const int idx = (i - key + 12) % 12; // rotate so the key is the root
                s += profile[i] * tpl[idx];
            }
            return s;
        }

        float sum() const noexcept
        {
            float s = 0.0f;
            for (float v : profile) s += v;
            return s;
        }
    };
}



