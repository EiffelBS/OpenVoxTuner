// preprocess.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "audio/preprocess.h"
#include <cmath>

namespace ovtchord
{
    AudioPreprocessor::AudioPreprocessor() : AudioPreprocessor (Config()) {}

    AudioPreprocessor::AudioPreprocessor (const Config& cfg) : cfg (cfg) {}

    void AudioPreprocessor::setSampleRate (double sampleRate)
    {
        if (sampleRate > 0.0 && sr != sampleRate)
        {
            sr = sampleRate;
            coeffsValid = false;
        }
    }

    void AudioPreprocessor::updateCoeffs()
    {
        const double pi = std::acos (-1.0);
        const double q = 0.7071; // Butterworth Q

        // High-pass at lowFreqHz.
        {
            const double w0 = 2.0 * pi * cfg.lowFreqHz / sr;
            const double alpha = std::sin (w0) / (2.0 * q);
            const double a0 = 1.0 + alpha;
            const double c = std::cos (w0);
            hp0 = (1.0 + c) / 2.0 / a0;
            hp1 = -(1.0 + c) / a0;
            hp2 = (1.0 + c) / 2.0 / a0;
            hpa1 = -2.0 * c / a0;
            hpa2 = (1.0 - alpha) / a0;
        }
        // Low-pass at highFreqHz.
        {
            const double w0 = 2.0 * pi * cfg.highFreqHz / sr;
            const double alpha = std::sin (w0) / (2.0 * q);
            const double a0 = 1.0 + alpha;
            const double c = std::cos (w0);
            lp0 = (1.0 - c) / 2.0 / a0;
            lp1 = (1.0 - c) / a0;
            lp2 = (1.0 - c) / 2.0 / a0;
            lpa1 = -2.0 * c / a0;
            lpa2 = (1.0 - alpha) / a0;
        }
        coeffsValid = true;
        hx1 = hx2 = hy1 = hy2 = 0.0;
        lx1 = lx2 = ly1 = ly2 = 0.0;
    }

    void AudioPreprocessor::reset()
    {
        hx1 = hx2 = hy1 = hy2 = 0.0;
        lx1 = lx2 = ly1 = ly2 = 0.0;
        smoothedPeak = 0.0;
        coeffsValid = false;
    }

    bool AudioPreprocessor::process (std::vector<float>& samples)
    {
        if (! coeffsValid)
            updateCoeffs();

        // Noise gate: compute peak; if below threshold, keep frame as-is.
        float peak = 0.0f;
        for (float s : samples)
            if (std::abs (s) > peak) peak = std::abs (s);
        if (peak < cfg.gateThreshold)
            return false;

        // Bandpass: high-pass then low-pass, continuous state across frames.
        for (float& v : samples)
        {
            const float in = v;
            // HP stage.
            const double hy = hp0 * in + hp1 * hx1 + hp2 * hx2 - hpa1 * hy1 - hpa2 * hy2;
            hx2 = hx1; hx1 = in;
            hy2 = hy1; hy1 = hy;
            // LP stage (input = HP output).
            const double ly = lp0 * hy + lp1 * lx1 + lp2 * lx2 - lpa1 * ly1 - lpa2 * ly2;
            lx2 = lx1; lx1 = hy;
            ly2 = ly1; ly1 = ly;
            v = static_cast<float> (ly);
        }

        // Gentle AGC: normalize by a smoothed peak toward agcTarget.
        smoothedPeak = 0.98 * smoothedPeak + 0.02 * peak;
        if (smoothedPeak > 1.0e-6f)
        {
            const float gain = cfg.agcTarget / smoothedPeak;
            for (float& s : samples) s *= gain;
        }
        return true;
    }
}