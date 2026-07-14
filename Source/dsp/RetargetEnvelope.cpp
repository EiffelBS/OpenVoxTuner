// RetargetEnvelope.cpp
// Implementation du filtre de retargeting IIR 1er ordre.

#include "RetargetEnvelope.h"
#include <cmath>

namespace ovtdsp
{
    RetargetEnvelope::RetargetEnvelope() = default;
    RetargetEnvelope::~RetargetEnvelope() = default;

    void RetargetEnvelope::prepare (double sr)
    {
        sampleRate = sr;
        recomputeAlpha();
    }

    void RetargetEnvelope::reset()
    {
        currentValue = 1.0f;
    }

    void RetargetEnvelope::setSpeed (float ms)
    {
        speedMs = juce::jmax (0.0f, ms);
        recomputeAlpha();
    }

    void RetargetEnvelope::recomputeAlpha()
    {
        // alpha = 1 - exp(-dt / tau) avec dt = 1 / sampleRate et tau = speedMs / 1000
        if (speedMs <= 0.0f)
        {
            alpha = 1.0f; // reponse instantanee
            return;
        }
        const double dt = 1.0 / sampleRate;
        const double tau = static_cast<double> (speedMs) / 1000.0;
        alpha = static_cast<float> (1.0 - std::exp (-dt / tau));
    }

    float RetargetEnvelope::processSample (float targetRatio)
    {
        // Filtre IIR 1er ordre (exponential smoothing).
        // y[n] = y[n-1] + alpha * (target - y[n-1])
        currentValue += alpha * (targetRatio - currentValue);
        return currentValue;
    }

    float RetargetEnvelope::processBlock (float targetRatio, int numSamples)
    {
        // Variante block-aware : on applique un seul pas de filtre par bloc,
        // mais avec un alpha equivalent a N pas per-sample successifs.
        // Formule : alpha_block = 1 - (1 - alpha_sample)^N
        // Comme alpha_sample = 1 - exp(-dt/tau) et dt = 1/sampleRate :
        //   alpha_block = 1 - exp(-numSamples / (sampleRate * tau))
        //              = 1 - exp(-blockDuration / tau)
        // Cela garantit que la constante de temps effective est tau, independamment
        // de la taille du bloc.
        if (speedMs <= 0.0f)
        {
            // Speed = 0 : reponse instantanee.
            currentValue = targetRatio;
            return currentValue;
        }
        const double blockDuration = static_cast<double> (numSamples) / sampleRate;
        const double tau = static_cast<double> (speedMs) / 1000.0;
        const float blockAlpha = static_cast<float> (1.0 - std::exp (-blockDuration / tau));
        currentValue += blockAlpha * (targetRatio - currentValue);
        return currentValue;
    }
}
