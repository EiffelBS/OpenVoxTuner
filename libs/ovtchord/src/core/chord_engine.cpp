// chord_engine.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "ovtchord/chord_engine.h"
#include "core/chord_templates.h"
#include <cmath>

namespace ovtchord
{
    namespace
    {
        bool contains (const std::vector<int>& v, int x)
        {
            for (int i : v) if (i == x) return true;
            return false;
        }
    }

    ChordEngine::ChordEngine (const Config& cfg) : cfg (cfg) {}

    ChordResult ChordEngine::recognize (const std::vector<int>& pitchClasses,
                                        int bassPitchClass, double timestamp)
    {
        ChordResult result;
        result.timestamp = timestamp;

        if (static_cast<int> (pitchClasses.size()) < cfg.minNotes)
        {
            result.valid = false;
            return result;
        }

        bool present[12] = { false };
        for (int pc : pitchClasses)
            present[((pc % 12) + 12) % 12] = true;

        float bestScore = -1.0f;
        int bestRoot = -1;
        const ChordTemplate* bestTpl = nullptr;

        for (int root = 0; root < 12; ++root)
        {
            for (const auto& tpl : chordTemplates())
            {
                int matched = 0;
                for (int iv : tpl.intervals)
                    if (present[(root + iv) % 12]) ++matched;
                const float coverage = static_cast<float> (matched) / static_cast<float> (tpl.intervals.size());

                int extra = 0;
                for (int pc : pitchClasses)
                {
                    const int iv = ((pc - root) % 12 + 12) % 12;
                    if (! contains (tpl.intervals, iv)) ++extra;
                }
                const float score = coverage - 0.5f * (static_cast<float> (extra) / static_cast<float> (pitchClasses.size()));

                const bool better = score > bestScore + 1e-6f;
                const bool tieSimpler = (std::abs (score - bestScore) <= 1e-6f)
                                        && bestTpl != nullptr
                                        && tpl.intervals.size() < bestTpl->intervals.size();
                if (better || tieSimpler)
                {
                    bestScore = score;
                    bestRoot = root;
                    bestTpl = &tpl;
                }
            }
        }

        if (bestTpl == nullptr || bestScore < cfg.minConfidence)
        {
            result.valid = false;
            return result;
        }

        result.rootPitchClass = bestRoot;
        result.pitchClasses = pitchClasses;
        result.symbol = std::string (pitchClassName (bestRoot)) + bestTpl->name;
        result.confidence = bestScore;

        if (bassPitchClass >= 0 && bassPitchClass != bestRoot)
            result.symbol += std::string ("/") + pitchClassName (bassPitchClass);

        // Stability: only valid after N consecutive frames with the same set.
        if (lastPitchClasses == pitchClasses)
            ++stableCount;
        else
        {
            stableCount = 1;
            lastPitchClasses = pitchClasses;
        }
        result.valid = (stableCount >= cfg.stabilityFrames);
        return result;
    }

    void ChordEngine::reset()
    {
        lastPitchClasses.clear();
        stableCount = 0;
    }
}
