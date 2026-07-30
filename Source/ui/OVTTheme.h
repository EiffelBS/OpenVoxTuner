// OVTTheme.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>   // juce::FFT (Spectral waveform display)

namespace ovt
{
    /** Theme mode enumeration. */
    enum class Theme { Dark, Light };

    /** Get/set the current active theme (thread-safe for UI thread). */
    inline Theme& currentTheme()
    {
        static Theme theme = Theme::Dark;
        return theme;
    }

    /** Check if the current theme is dark. */
    inline bool isDark() { return currentTheme() == Theme::Dark; }

    // === Theme colour palette ===
    // Index 0 = Dark, Index 1 = Light

    // Main background (deepest layer)
    inline juce::Colour bgDark()
    {
        return isDark() ? juce::Colour::fromString("#FF26282B")
                        : juce::Colour::fromString("#FFF0F1F5");
    }

    // Panel background (cards, blocks)
    inline juce::Colour bgPanel()
    {
        return isDark() ? juce::Colour::fromString("#FF373A3E")
                        : juce::Colour::fromString("#FFE8E9ED");
    }

    // Accent colour (primary interactive elements)
    inline juce::Colour accent()
    {
        return isDark() ? juce::Colour::fromString("#FF1A9AF0")
                        : juce::Colour::fromString("#FF1565C0");
    }

    // Soft accent (backgrounds, subtle highlights)
    inline juce::Colour accentSoft()
    {
        return isDark() ? juce::Colour::fromString("#401A9AF0")
                        : juce::Colour::fromString("#301565C0");
    }

    // Primary text colour
    inline juce::Colour text()
    {
        return isDark() ? juce::Colour::fromString("#FFE1E1E6")
                        : juce::Colour::fromString("#FF2C2C34");
    }

    // Secondary/dimmed text
    inline juce::Colour textDim()
    {
        return isDark() ? juce::Colour (0xff868686)
                        : juce::Colour (0xff666666);
    }

    // Visualizer/curve editor always use dark mode - must be opaque to cover light tab background
    inline juce::Colour vizBg()
    {
        return juce::Colour (0xff15151b);
    }

    // Grid lines (always dark: used inside visualizer/curve editor which are forced dark)
    inline juce::Colour grid()
    {
        return juce::Colour (0x20ffffff);
    }

    // Scale note lines (always dark: used inside visualizer/curve editor which are forced dark)
    inline juce::Colour scaleLine()
    {
        return juce::Colour (0x10ffffff);
    }

    // Input curve colour (pink)
    inline juce::Colour inputColour()
    {
        return isDark() ? juce::Colour (0xffe91e63).withAlpha (0.4f)
                        : juce::Colour (0xffc2185b).withAlpha (0.5f);
    }

    // Output curve colour (green)
    inline juce::Colour outputColour()
    {
        return juce::Colour (0xff00e676);
    }

    // Harmony curve colour (blue)
    inline juce::Colour harmonyColour()
    {
        return juce::Colour (0xff1A9AF0).withAlpha (0.7f);
    }

    // Piano white key
    inline juce::Colour pianoWhite()
    {
        return isDark() ? juce::Colour (0xffffffff)
                        : juce::Colour (0xffffffff);
    }

    // Piano black key
    inline juce::Colour pianoBlack()
    {
        return isDark() ? juce::Colour (0xff1a1a1a)
                        : juce::Colour (0xff2a2a2a);
    }

    // Piano key border
    inline juce::Colour pianoBorder()
    {
        return isDark() ? juce::Colour (0xff333333)
                        : juce::Colour (0xffbbbbbb);
    }

    // Piano key text
    inline juce::Colour pianoText()
    {
        return isDark() ? juce::Colour (0xff000000)
                        : juce::Colour (0xff000000);
    }

    // Header background
    inline juce::Colour headerBg()
    {
        return isDark() ? juce::Colour::fromString("#FF373A3E")
                        : juce::Colour (0xffe0e1e6);
    }

    // Header accent line
    inline juce::Colour headerAccent()
    {
        return isDark() ? juce::Colour (0x331A9AF0)
                        : juce::Colour (0x331565C0);
    }

    // Visualizer header always dark
    inline juce::Colour vizHeaderBg()
    {
        return juce::Colour (0xff191b1e);
    }

    // PitchVisualizer header accent line
    inline juce::Colour vizHeaderAccent()
    {
        return juce::Colour (0x331A9AF0);
    }

    // Visualizer legend block background
    inline juce::Colour vizLegendBg()
    {
        return juce::Colour (0xff191b1e);
    }

    // Curve editor ruler always dark
    inline juce::Colour rulerBg()
    {
        return juce::Colour (0xff1a1a1a);
    }

    // Curve editor grid always light-on-dark
    inline juce::Colour curveGrid()
    {
        return juce::Colour (0x40ffffff);
    }

    // CPU meter always uses dark mode (visible on any theme)
    inline juce::Colour cpuBg()
    {
        return juce::Colour (0xff222230);
    }

    inline juce::Colour cpuText()
    {
        return juce::Colours::white.withAlpha (0.9f);
    }

    /** Waveform rendering display mode for the visualizer and curve editor. */
    enum class WaveformDisplayType
    {
        Line    = 0,   // Simple waveform line (outline only)
        Mirror  = 1,   // Mirrored bars (symmetric around center) â€” default
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



