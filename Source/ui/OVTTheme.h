// OVTTheme.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.


// Theme state and the shared colour palette migrated to the eiffelbs-ui
// design system (ebs:: namespace). This header keeps ONLY the app-specific
// waveform overlay renderer, which depends on juce_dsp's FFT and is too
// heavy-weight for the shared library. It still includes <eiffelbs/eiffelbs.h>
// so translation units including it keep seeing the ebs:: palette after the
// cutover.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>   // juce::FFT (Spectral waveform display)
#include <eiffelbs/eiffelbs.h>

namespace ovt
{
    /** Waveform rendering display mode for the visualizer and curve editor. */
    enum class WaveformDisplayType
    {
        Line    = 0,   // Simple waveform line (outline only)
        Mirror  = 1,   // Mirrored bars (symmetric around center) - default
        Spectral = 2   // FFT magnitude spectrum rising from the bottom (EQ-like)
    };

    /** Shared waveform overlay rendering function used by both PitchVisualizer and PitchCurveEditor.
     *  Provides a consistent amplitude scale (halfH = plotArea.getHeight() * 0.35f) for all display types.
     *  @param g           Graphics context to draw into
     *  @param data        Pointer to audio sample data (normalized -1..1)
     *  @param numSamples  Number of samples in the data buffer
     *  @param plotArea    Target rectangle for rendering
     *  @param displayType  Which rendering style to use
     */
    inline void drawWaveformOverlay (juce::Graphics& g, const float* data, int numSamples,
                                     juce::Rectangle<int> plotArea, WaveformDisplayType displayType)
    {
        if (data == nullptr || numSamples <= 0) return;

        const float midY = (float) plotArea.getCentreY();
        const float halfH = (float) plotArea.getHeight() * 0.35f;
        const float plotW = (float) plotArea.getWidth();
        const float plotX = (float) plotArea.getX();

        switch (displayType)
        {
            case WaveformDisplayType::Line:
            {
                // Simple waveform line (outline only, 0.6 opacity)
                juce::Path wavePath;
                wavePath.startNewSubPath (plotX, midY);
                for (int i = 0; i < numSamples; ++i)
                {
                    const float x = plotX + plotW * (float) i / (float) numSamples;
                    const float amp = data[i] * halfH;
                    wavePath.lineTo (x, midY - amp);
                }
                g.setColour (juce::Colour (0x66ffffff)); // 0.4 opacity
                g.strokePath (wavePath, juce::PathStrokeType (1.0f));
                break;
            }
            case WaveformDisplayType::Mirror:
            default:
            {
                // Mirrored bars (symmetric around center)
                juce::Path wavePath;
                const int step = juce::jmax (1, numSamples / plotArea.getWidth());
                for (int x = plotArea.getX(); x < plotArea.getRight(); x += 2)
                {
                    const int sampleIdx = juce::jmap (x, plotArea.getX(), plotArea.getRight() - 1, 0, numSamples - 1);
                    const int endIdx = juce::jmin (sampleIdx + step, numSamples - 1);
                    float maxAbs = 0.0f;
                    for (int i = sampleIdx; i <= endIdx; ++i)
                        maxAbs = juce::jmax (maxAbs, std::abs (data[i]));
                    const float barH = maxAbs * halfH;
                    wavePath.addRoundedRectangle ((float) x, midY - barH, 2.0f, barH * 2.0f, 1.0f);
                }
                g.setColour (juce::Colour (0x18ffffff));
                g.fillPath (wavePath);
                break;
            }
            case WaveformDisplayType::Spectral:
            {
                // FFT magnitude spectrum, drawn as EQ-like bars rising from the
                // bottom. The FREQUENCY AXIS IS LOGARITHMIC (lowest ~30 Hz -> Nyquist)
                // so the voice's energy (fundamental + formants, mostly < 4 kHz)
                // spreads across the full width instead of being crammed into the
                // leftmost few percent of a linear axis. The peak magnitude over the
                // bin range mapped to each x is used so no peak is missed.
                const int fftOrder = 9;                       // 2^9 = 512 samples
                const int fftSize  = 1 << fftOrder;
                const int bufSize  = fftSize * 2;             // real->complex interleave
                if (numSamples < fftSize)
                    break;

                juce::HeapBlock<float> fbuf (static_cast<size_t> (bufSize));
                const int offset = numSamples - fftSize;
                for (int i = 0; i < fftSize; ++i)
                    fbuf[i] = data[offset + i];
                for (int i = fftSize; i < bufSize; ++i)
                    fbuf[i] = 0.0f;

                juce::dsp::FFT fft (fftOrder);
                fft.performRealOnlyForwardTransform (fbuf.getData(), true);

                const int numBins = fftSize / 2;
                const float bottom = static_cast<float> (plotArea.getBottom());
                const float maxH   = static_cast<float> (plotArea.getHeight()) * 0.95f;
                // Magnitude is shown in dB so the spectrum looks like a real EQ
                // (compressed dynamic range) instead of one giant block at the
                // fundamental. floorDb is the lowest displayed level.
                const float eps     = 1.0e-6f;
                const float floorDb = 60.0f;

                // Log frequency mapping in bin-index space (bin index is linear in
                // frequency, so a log map of bin indices gives the proper spread).
                const float rMin = 0.0015f;  // lowest displayed frequency ratio (~30 Hz @ 44.1k)
                const int numBars = juce::jmax (64, (int) (plotW / 2.0f));

                auto ratioToBin = [&] (float t)
                {
                    const float ratio = rMin * std::pow (1.0f / rMin, t);
                    return ratio * static_cast<float> (numBins);
                };

                juce::Path specPath;
                specPath.startNewSubPath (plotX, bottom);
                for (int i = 0; i <= numBars; ++i)
                {
                    const float t  = static_cast<float> (i) / static_cast<float> (numBars);
                    const float tN = static_cast<float> (i + 1) / static_cast<float> (numBars);
                    const int lo = juce::jlimit (1, numBins - 1, (int) std::floor (ratioToBin (t)));
                    const int hi = juce::jlimit (1, numBins - 1, (int) std::ceil  (ratioToBin (tN)));
                    float magLin = 0.0f;
                    for (int b = lo; b <= hi; ++b)
                    {
                        const float re = fbuf[2 * b];
                        const float im = fbuf[2 * b + 1];
                        magLin = juce::jmax (magLin, std::sqrt (re * re + im * im) / static_cast<float> (fftSize));
                    }
                    // Convert to dB and map to 0..1 over [floorDb .. 0 dB].
                    const float db   = 20.0f * std::log10 (magLin + eps);
                    const float norm = juce::jlimit (0.0f, 1.0f, (db + floorDb) / floorDb);
                    const float x = plotX + plotW * t;
                    const float y = bottom - norm * maxH;
                    specPath.lineTo (x, y);
                }
                specPath.lineTo (plotX + plotW, bottom);
                specPath.closeSubPath();

                // Gradient fill (brighter at the peaks) for a modern EQ look.
                juce::ColourGradient grad (juce::Colour (0x33ffffff), 0.0f, bottom - maxH,
                                           juce::Colour (0x10ffffff), 0.0f, bottom, false);
                g.setFillType (grad);
                g.fillPath (specPath);

                // Bright cap line on top of the spectrum.
                g.setColour (juce::Colour (0x66ffffff));
                g.strokePath (specPath, juce::PathStrokeType (1.0f));
                break;
            }
        }
    }
}
