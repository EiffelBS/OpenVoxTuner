#pragma once
// LatencyDeclaration.h
// OpenVoxTuner DSP
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// What the plugin declares to the host (PDC) is the EFFECTIVE delay of the
// shipped pitch-synchronous OLA shifter, not the internal latency mode number.
// The grain is rendered forward from the sample where it is created and its
// window peaks half a grain later, so the true delay is
//
//     effective = internalLatency + 1.25 x period (x formant ratio) - pitchMarkOffset
//
// which grows as the voice goes down. The table below is the measured effective
// delay (AM-envelope correlation, +1 semitone) at the LOWEST documented note of
// each Voice Type, rounded up by 1 ms and held monotone, from
// `bench_pitchshift_latency --latency-matrix` (see
// docs/low-latency-shifter-analysis.md, Section 1.3, for the raw matrix).
//
// Why a table and not a formula: the pitch-mark alignment is signal dependent,
// the measured data is not perfectly monotone, and the host needs one integer
// per (mode, voice type) at prepare time. Declaring the register floor means the
// plugin is never LATE; inside the register the corrected voice can land early
// by up to ~10 ms at the top of the range, which is the unavoidable price of a
// single PDC value for a pitch-dependent delay.
//
// The internal processing modes stay 10 / 12 / 20 / 30 ms (state compatibility);
// this table only changes what is reported to the host.

#include <cmath>

namespace ovtdsp
{
    static constexpr int kLatencyDeclarationModes = 4;
    static constexpr int kLatencyDeclarationVoiceTypes = 6;

    /** Declared PDC latency in milliseconds.
        @param latencyMode 0 = Direct Monitoring (internal 10 ms), 1 = Low Latency
                           (12 ms), 2 = Quality (20 ms), 3 = Safe (30 ms)
        @param voiceType   0 = Universal, 1 = Bass, 2 = Baritone, 3 = Tenor,
                           4 = Alto, 5 = Soprano.
        Universal (30-1000 Hz detector range) declares for the Bass floor (82.41 Hz):
        below E2 the reported latency is optimistic, which is the documented limit
        of an honest fixed declaration. */
    inline int declaredLatencyMs (int latencyMode, int voiceType) noexcept
    {
        // Rows: modes. Columns: Universal, Bass, Baritone, Tenor, Alto, Soprano.
        static constexpr int table[kLatencyDeclarationModes][kLatencyDeclarationVoiceTypes] =
        {
            { 31, 31, 26, 23, 20, 14 }, // internal 10 ms
            { 35, 35, 27, 24, 20, 14 }, // internal 12 ms
            { 38, 38, 28, 27, 27, 21 }, // internal 20 ms
            { 38, 38, 38, 36, 35, 30 }  // internal 30 ms
        };

        const int m = latencyMode < 0 ? 0
                    : (latencyMode >= kLatencyDeclarationModes ? kLatencyDeclarationModes - 1
                                                               : latencyMode);
        const int v = voiceType < 0 ? 0
                    : (voiceType >= kLatencyDeclarationVoiceTypes ? kLatencyDeclarationVoiceTypes - 1
                                                                  : voiceType);
        return table[(size_t) m][(size_t) v];
    }

    /** Same value in samples for the host's PDC. */
    inline int declaredLatencySamples (double sampleRate, int latencyMode, int voiceType) noexcept
    {
        const double ms = (double) declaredLatencyMs (latencyMode, voiceType);
        return (int) std::lround (ms * sampleRate / 1000.0);
    }
}
