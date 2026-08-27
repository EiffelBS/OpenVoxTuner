// PitchVisualizer.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "PitchVisualizer.h"
#include "OVTFonts.h"
#include <cmath>
#include "OVTTheme.h"
#include "OVTLanguages.h"

namespace ui
{
    // === Theme colours ===
    const juce::Colour PitchVisualizer::kBg              = juce::Colour (0x4015151b);
    const juce::Colour PitchVisualizer::kGrid            = juce::Colour (0x20ffffff);
    const juce::Colour PitchVisualizer::kInputColour     = juce::Colour (0xffe91e63).withAlpha (0.4f);
    const juce::Colour PitchVisualizer::kOutputColour    = juce::Colour (0xff00e676);
    const juce::Colour PitchVisualizer::kScaleLineColour = juce::Colour (0x10ffffff);
    const juce::Colour PitchVisualizer::kHarmonyColour   = juce::Colour (0xff1A9AF0).withAlpha (0.7f);

    // === SVG icons (Lucide-style, 24x24 viewBox) ===
    // Placeholder colour #010101 is replaced at setup time for each state.
    static const char* svgZoomIn = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/><line x1="11" y1="8" x2="11" y2="14"/><line x1="8" y1="11" x2="14" y2="11"/></svg>)";

    static const char* svgZoomOut = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/><line x1="8" y1="11" x2="14" y2="11"/></svg>)";

    static const char* svgScrollUp = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="18 15 12 9 6 15"/></svg>)";

    static const char* svgScrollDown = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="6 9 12 15 18 9"/></svg>)";

    static const char* svgReset = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/></svg>)";

    // Helper: create a Drawable from SVG XML, replacing placeholder colour.
    static std::unique_ptr<juce::Drawable> createDrawableFromSVG (
        const char* svgXml, juce::Colour colour)
    {
        auto xml = juce::XmlDocument::parse (svgXml);
        if (xml == nullptr) return std::make_unique<juce::DrawablePath>();
        auto d = juce::Drawable::createFromSVG (*xml);
        if (d == nullptr) return std::make_unique<juce::DrawablePath>();
        d->replaceColour (juce::Colour (0xff010101), colour);
        return d;
    }

    PitchVisualizer::PitchVisualizer()
    {
        inputHistory.clear();
        outputHistory.clear();
        for (int i = 0; i < historySize; ++i)
        {
            inputHistory.add (0.0f);
            outputHistory.add (0.0f);
        }
        harmonyHistory.clear();
        for (int v = 0; v < maxHarmonyVoices; ++v)
        {
            juce::Array<float> h;
            for (int i = 0; i < historySize; ++i) h.add (0.0f);
            harmonyHistory.add (h);
        }

        addAndMakeVisible (pianoKeyboard);
        // The piano keyboard is display-only: do NOT let it swallow mouse
        // events (wheel / pinch) meant for the visualizer, which would block
        // scroll & zoom when the cursor is over the left piano strip.
        // Mirrors the Curve Editor's setup.
        pianoKeyboard.setInterceptsMouseClicks (false, false);
        pianoKeyboard.setRange (static_cast<int> (ovtdsp::hzToMidiFloat (fMin)),
                                static_cast<int> (ovtdsp::hzToMidiFloat (fMax)));

        // === SVG icon buttons (order: zoom, scroll, reset) ===
        setupIconBtn (zoomInButton,    svgZoomIn,    ovt::tr(ovt::Keys::kTooltipZoomIn));
        setupIconBtn (zoomOutButton,   svgZoomOut,   ovt::tr(ovt::Keys::kTooltipZoomOut));
        setupIconBtn (scrollUpButton,  svgScrollUp,  ovt::tr(ovt::Keys::kTooltipScrollUp));
        setupIconBtn (scrollDownButton,svgScrollDown,ovt::tr(ovt::Keys::kTooltipScrollDown));
        setupIconBtn (resetViewButton, svgReset,     ovt::tr(ovt::Keys::kTooltipResetView));

        zoomInButton.onClick     = [this] { zoomIn(); };
        zoomOutButton.onClick    = [this] { zoomOut(); };
        scrollUpButton.onClick   = [this] { scrollUp(); };
        scrollDownButton.onClick = [this] { scrollDown(); };
        resetViewButton.onClick  = [this] { resetView(); };

        targetFMin = fMin;
        targetFMax = fMax;

        // Allocate the spectral ring buffer (recent audio samples for the FFT view).
        waveformRing.setSize (1, kWaveRingCapacity);

        startTimerHz (30);
    }

    PitchVisualizer::~PitchVisualizer()
    {
        // Theme broadcast teardown must precede member destruction.
        ebs::unsubscribeTheme (this);
        stopTimer();
    }

    void PitchVisualizer::setupIconBtn (juce::DrawableButton& btn, const char* svgXml,
                                         const juce::String& tooltip, bool /*isToggle*/)
    {
        // Strokes follow the active palette instead of fixed grey/white so
        // the toolbar stays readable on Light backgrounds too.
        auto normal = createDrawableFromSVG (svgXml, ebs::text().withAlpha (0.75f));
        auto over   = createDrawableFromSVG (svgXml, ebs::text());
        auto down   = createDrawableFromSVG (svgXml, ebs::accent());
        btn.setImages (normal.get(), over.get(), down.get());
        btn.setTooltip (tooltip);
        btn.setColour (juce::DrawableButton::backgroundColourId,     juce::Colour (0x331A9AF0));
        btn.setColour (juce::DrawableButton::backgroundOnColourId,   juce::Colour (0x661A9AF0));
        btn.setColour (juce::DrawableButton::textColourId,           juce::Colours::white);
        addAndMakeVisible (btn);
        ebs::subscribeTheme (this);   // idempotent; one subscription suffices
    }

    void PitchVisualizer::themeChanged() { restyleToolbarIcons(); }

    void PitchVisualizer::restyleToolbarIcons()
    {
        const std::pair<juce::DrawableButton*, const char*> skins[] =
        {
            { &zoomInButton,     svgZoomIn },
            { &zoomOutButton,    svgZoomOut },
            { &scrollUpButton,   svgScrollUp },
            { &scrollDownButton, svgScrollDown },
            { &resetViewButton,  svgReset }
        };
        for (auto& s : skins)
        {
            auto normal = createDrawableFromSVG (s.second, ebs::text().withAlpha (0.75f));
            auto over   = createDrawableFromSVG (s.second, ebs::text());
            auto down   = createDrawableFromSVG (s.second, ebs::accent());
            s.first->setImages (normal.get(), over.get(), down.get());
        }
    }

    void PitchVisualizer::pushInputPitch (float hz)
    {
        if (inputHistory.size() >= historySize) inputHistory.remove (0);
        inputHistory.add (hz);
        latestInputHz = hz;
    }

    void PitchVisualizer::pushOutputPitch (float hz)
    {
        if (outputHistory.size() >= historySize) outputHistory.remove (0);
        outputHistory.add (hz);
        latestOutputHz = hz;
    }

    void PitchVisualizer::setNoteInfo (const ovtdsp::NoteInfo& info)
    {
        noteInfo = info;
    }

    void PitchVisualizer::setScaleIntervals (const juce::Array<int>& intervals)
    {
        scaleIntervals = intervals;
        pianoKeyboard.setScaleIntervals (intervals);
    }

    void PitchVisualizer::setHarmonyFrequencies (const juce::Array<float>& freqs)
    {
        for (int v = 0; v < maxHarmonyVoices; ++v)
        {
            float value = 0.0f;
            if (v < freqs.size()) value = freqs[v];
            auto& h = harmonyHistory.getReference (v);
            if (h.size() >= historySize) h.remove (0);
            h.add (value);
        }
    }

    void PitchVisualizer::setWaveformOverlay (const float* samples, int numSamples, double sampleRate)
    {
        if (samples == nullptr || numSamples <= 0)
        {
            hasWaveform = false;
            return;
        }
        // Keep the most recent audio block for the Line / Mirror display types
        // (unchanged behaviour).
        waveformBuffer.setSize (1, numSamples, false, false, true);
        waveformBuffer.copyFrom (0, 0, samples, numSamples);
        waveformSampleRate = sampleRate;

        // Append into the ring buffer so the Spectral (FFT) view can always build
        // a >= 512-sample window from recent audio, regardless of the host block size.
        const int cap = kWaveRingCapacity;
        const int n = juce::jmin (numSamples, cap);
        for (int i = 0; i < n; ++i)
        {
            waveformRing.setSample (0, waveformRingWritePos, samples[i]);
            waveformRingWritePos = (waveformRingWritePos + 1) % cap;
        }
        waveformTotalWritten += numSamples;
        hasWaveform = true;
    }

    void PitchVisualizer::paintWaveformOverlay (juce::Graphics& g, juce::Rectangle<int> plotArea)
    {
        if (! hasWaveform) return;

        // The Spectral (FFT) view needs a contiguous >= 512-sample window, which we
        // rebuild from the ring buffer of recent samples.
        if (currentDisplayType == static_cast<int> (ovt::WaveformDisplayType::Spectral))
        {
            const int cap = kWaveRingCapacity;
            const int available = juce::jmin (cap, waveformTotalWritten);
            if (available < (1 << 9)) return;  // need at least fftSize (512) samples

            // Flatten the most recent `available` samples (oldest -> newest) into a
            // contiguous tail buffer ending just before the ring write position.
            const int tailStart = (waveformRingWritePos - available + cap) % cap;
            waveformTailBuffer.setSize (1, available, false, false, true);
            if (tailStart + available <= cap)
                waveformTailBuffer.copyFrom (0, 0, waveformRing, 0, tailStart, available);
            else
            {
                const int first = cap - tailStart;
                waveformTailBuffer.copyFrom (0, 0, waveformRing, 0, tailStart, first);
                waveformTailBuffer.copyFrom (0, first, waveformRing, 0, 0, available - first);
            }

            ovt::drawWaveformOverlay (g, waveformTailBuffer.getReadPointer (0),
                                      available, plotArea,
                                      ovt::WaveformDisplayType::Spectral);
            return;
        }

        if (waveformBuffer.getNumSamples() == 0) return;
        ovt::drawWaveformOverlay (g, waveformBuffer.getReadPointer (0),
                                  waveformBuffer.getNumSamples(), plotArea,
                                  static_cast<ovt::WaveformDisplayType> (currentDisplayType));
    }

    float PitchVisualizer::hzToY (float hz, int height) const
    {
        if (hz <= 0.0f) return static_cast<float> (height);
        const float midiF = ovtdsp::hzToMidiFloat (hz);
        const int lowestMidi = pianoKeyboard.getLowestMidi();
        const int highestMidi = pianoKeyboard.getHighestMidi();
        const int range = juce::jmax (1, highestMidi - lowestMidi);
        const float t = (midiF - static_cast<float> (lowestMidi)) / static_cast<float> (range);
        return height * (1.0f - juce::jlimit (0.0f, 1.0f, t));
    }

    float PitchVisualizer::yToHz (float y, int height) const
    {
        if (height <= 0) return 0.0f;
        const float t = 1.0f - juce::jlimit (0.0f, 1.0f, y / static_cast<float> (height));
        const int lowestMidi = pianoKeyboard.getLowestMidi();
        const int highestMidi = pianoKeyboard.getHighestMidi();
        const int range = juce::jmax (1, highestMidi - lowestMidi);
        const float midiF = static_cast<float> (lowestMidi) + t * static_cast<float> (range);
        return ovtdsp::midiToHz (midiF);
    }

    void PitchVisualizer::mouseMove (const juce::MouseEvent& e)
    {
        const int headerH = juce::jmin (50, getHeight() / 4);
        const int pianoW = pianoKeyboard.getWidth() > 0 ? pianoKeyboard.getWidth() : 60;
        const auto plotArea = juce::Rectangle<int> (pianoW, headerH, getWidth() - pianoW, getHeight() - headerH);

        auto pos = e.getPosition();
        if (plotArea.contains (pos))
        {
            isMouseOverPlot = true;
            hoverHz = yToHz (static_cast<float> (pos.y - plotArea.getY()), plotArea.getHeight());
        }
        else
        {
            isMouseOverPlot = false;
            hoverHz = 0.0f;
        }
        repaint();
    }

    void PitchVisualizer::mouseExit (const juce::MouseEvent&)
    {
        isMouseOverPlot = false;
        hoverHz = 0.0f;
        repaint();
    }

    void PitchVisualizer::paint (juce::Graphics& g)
    {
        const auto b = getLocalBounds();
        const int W = b.getWidth();
        const int H = b.getHeight();

        const int headerH = juce::jmin (50, H / 4);
        const int pianoW = pianoKeyboard.getWidth() > 0 ? pianoKeyboard.getWidth() : 60;
        const auto plotArea = juce::Rectangle<int> (pianoW, headerH, W - pianoW, H - headerH);

        // === Background ===
        g.fillAll (ebs::vizBg());

        // === Modern header strip ===
        {
            g.setColour (ebs::vizHeaderBg());
            g.fillRect (0, 0, W, headerH);
            g.setColour (ebs::vizHeaderAccent());
            g.fillRect (0, headerH - 2, W, 2);

            const int badgeH = headerH - 12;
            const int badgeY = (headerH - badgeH) / 2;
            const int noteAreaX = 14;
            const int curW = static_cast<int> (std::round (badgeAnimW));

            // ---- Unified animated note badge ----
            {
                const bool splitMode = noteInfo.valid
                                       && noteInfo.targetName != noteInfo.name;

                // Glow behind the badge
                if (noteInfo.valid)
                {
                    const juce::Colour glowCol = splitMode
                        ? juce::Colour (0x15e53935) : juce::Colour (0x151A9AF0);
                    g.setColour (glowCol);
                    g.fillRoundedRectangle ((float) (noteAreaX - 3), (float) (badgeY - 3),
                                            (float) (curW + 6), (float) (badgeH + 6), 8.0f);
                }

                // Badge background
                if (! noteInfo.valid)
                {
                    g.setColour (juce::Colour (0x11ffffff));
                    g.fillRoundedRectangle ((float) noteAreaX, (float) badgeY,
                                            (float) curW, (float) badgeH, 6.0f);
                    g.setColour (juce::Colour (0x22ffffff));
                    g.drawRoundedRectangle ((float) noteAreaX, (float) badgeY,
                                            (float) curW, (float) badgeH, 6.0f, 1.0f);
                }
                else if (splitMode)
                {
                    // Split: left half = detected (red), right half = target (green)
                    const int splitPx = static_cast<int> (std::round (badgeSplitX));

                    // 1) Full badge in detected color (red) - covers the entire
                    //    badge area including the rounded corners.
                    g.setColour (juce::Colour (0xffe53935));
                    g.fillRoundedRectangle ((float) noteAreaX, (float) badgeY,
                                            (float) curW, (float) badgeH, 6.0f);

                    // 2) Green right half - use a pixel-perfect Rectangle clip
                    //    (no anti-aliasing) to prevent any red bleed at the edges.
                    //    The rectangle extends to the badge right edge; the
                    //    rounded corners are masked by the border drawn in step 3.
                    g.saveState();
                    g.reduceClipRegion (noteAreaX + splitPx, badgeY,
                                        curW - splitPx, badgeH);
                    g.setColour (juce::Colour (0xff66bb6a));
                    g.fillRect (noteAreaX + splitPx, badgeY, curW - splitPx, badgeH);
                    g.restoreState();

                    // 3) Border - covers the sharp green corners at the seam and
                    //    provides a consistent outline around the full badge.
                    g.setColour (juce::Colour (0x55ffffff));
                    g.drawRoundedRectangle ((float) noteAreaX, (float) badgeY,
                                            (float) curW, (float) badgeH, 6.0f, 1.0f);
                }
                else
                {
                    // Single note (green - in tune)
                    g.setColour (juce::Colour (0xff66bb6a));
                    g.fillRoundedRectangle ((float) noteAreaX, (float) badgeY,
                                            (float) curW, (float) badgeH, 6.0f);
                    g.setColour (juce::Colour (0x554caf50));
                    g.drawRoundedRectangle ((float) noteAreaX, (float) badgeY,
                                            (float) curW, (float) badgeH, 6.0f, 1.0f);
                }

                // Text
                g.setFont (ovt::fontNoteLarge());
                if (! noteInfo.valid)
                {
                    g.setColour (juce::Colour (0x66ffffff));
                    g.drawText ("--", noteAreaX, badgeY, curW, badgeH,
                                juce::Justification::centred);
                }
                else if (splitMode)
                {
                    const int splitPx = static_cast<int> (std::round (badgeSplitX));
                    // Detected note (white on red)
                    g.setColour (juce::Colours::white);
                    g.drawText (noteInfo.name, noteAreaX, badgeY, splitPx, badgeH,
                                juce::Justification::centred);
                    // Target note (black on green)
                    g.setColour (juce::Colour (0xff1a1a1a));
                    g.drawText (noteInfo.targetName,
                                noteAreaX + splitPx, badgeY, curW - splitPx, badgeH,
                                juce::Justification::centred);
                }
                else
                {
                    g.setColour (juce::Colour (0xff1a1a1a));
                    g.drawText (noteInfo.name, noteAreaX, badgeY, curW, badgeH,
                                juce::Justification::centred);
                }
            }

            // ---- LED-grid VU meter ----
            {
                const float cents = noteInfo.valid ? noteInfo.cents : 0.0f;
                constexpr int segmentsPerSide = 8;
                constexpr int totalSegments = 2 * segmentsPerSide + 1;
                constexpr float centsPerSegment = 50.0f / (float) segmentsPerSide;
                constexpr int meterFixedW = 300;
                // Center the meter in the full plugin width; clamp so it
                // never overlaps the badge (left) or the toolbar buttons (right).
                const int meterW = juce::jmin (meterFixedW, juce::jmax (160, W / 2));
                const int btnZoneW = 5 * 20 + 4 * 3 + 16;
                const int badgeRight = noteAreaX + curW + 8;
                const int minLeft = badgeRight;
                const int maxLeft = W - btnZoneW - meterW;
                const int idealLeft = (W - meterW) / 2;
                const int meterLeft = juce::jlimit (minLeft, juce::jmax (minLeft, maxLeft), idealLeft);
                const int meterY = badgeY + 3;
                const int meterH = badgeH - 6;

                if (meterW > 80 && noteInfo.valid)
                {
                    constexpr int segGap = 2;
                    constexpr int segCount = totalSegments;
                    const int totalGaps = (segCount - 1) * segGap;
                    const int segW = (meterW - totalGaps) / segCount;
                    const int segWClamped = juce::jmax (6, segW);
                    g.setColour (juce::Colour (0x2215151b));
                    g.fillRoundedRectangle ((float) meterLeft - 4, (float) meterY - 2,
                                            (float) (meterW + 8), (float) (meterH + 4), 4.0f);

                    static const juce::Colour segmentColors[segmentsPerSide + 1] = {
                        juce::Colour (0xff4caf50),
                        juce::Colour (0xff66bb6a), juce::Colour (0xff8bc34a),
                        juce::Colour (0xffcddc39), juce::Colour (0xffffeb3b),
                        juce::Colour (0xffff9800), juce::Colour (0xffff5722),
                        juce::Colour (0xfff44336), juce::Colour (0xffd32f2f)
                    };

                    for (int i = 0; i < segCount; ++i)
                    {
                        int offset = i - segmentsPerSide;
                        int absOffset = std::abs (offset);
                        float threshold = (float) absOffset * centsPerSegment;
                        bool isActive = false;
                        if (offset == 0)
                            isActive = (std::abs (cents) < centsPerSegment * 0.5f && std::abs (cents) >= 0.001f);
                        else if (offset > 0)
                            isActive = (cents >= threshold);
                        else
                            isActive = (cents <= -threshold);

                        int segX = meterLeft + i * (segWClamped + segGap);
                        int segY = meterY;
                        int sH = meterH;
                        if (absOffset == 0) { segY -= 1; sH += 2; }

                        juce::Colour baseCol = (absOffset == 0)
                            ? juce::Colour (0xff00bcd4)
                            : segmentColors[juce::jmin (absOffset, segmentsPerSide)];

                        if (isActive)
                        {
                            g.setColour (baseCol);
                            g.fillRoundedRectangle ((float) segX, (float) segY,
                                                    (float) segWClamped, (float) sH, 2.5f);
                            g.setColour (baseCol.brighter (0.3f).withAlpha (0.5f));
                            g.fillRoundedRectangle ((float) (segX + 1), (float) (segY + 1),
                                                    (float) (segWClamped - 2), (float) (sH - 3), 1.5f);
                            if (absOffset == 0)
                            {
                                g.setColour (juce::Colour (0xffffffff).withAlpha (0.3f));
                                g.fillRoundedRectangle ((float) (segX + 1), (float) (segY + 1),
                                                        (float) (segWClamped - 2), (float) (sH - 2), 2.0f);
                            }
                        }
                        else
                        {
                            g.setColour (baseCol.withAlpha (0.12f));
                            g.fillRoundedRectangle ((float) segX, (float) segY,
                                                    (float) segWClamped, (float) sH, 2.5f);
                        }

                        g.setColour (juce::Colour (0x22ffffff));
                        g.drawRoundedRectangle ((float) segX, (float) segY,
                                                (float) segWClamped, (float) sH, 2.5f, 0.5f);
                        if (absOffset == 0)
                        {
                            g.setColour (juce::Colour (0x8800bcd4));
                            g.drawRoundedRectangle ((float) (segX - 1), (float) (segY - 1),
                                                    (float) (segWClamped + 2), (float) (sH + 2), 3.0f, 1.5f);
                        }
                    }

                    const int centerSegX = meterLeft + segmentsPerSide * (segWClamped + segGap);
                    g.setFont (ovt::fontMeter0());
                    g.setColour (juce::Colour (0xaa00bcd4));
                    g.drawText ("0", centerSegX - 12, meterY + meterH + 2, 24, 10,
                                juce::Justification::centred);
                }
            }
        }

        // === Plot area: pitch curves ===
        g.saveState();
        g.reduceClipRegion (plotArea);

        // Waveform overlay (Line / Mirror / Spectral) drawn first so the octave,
        // scale and pitch curves paint on top of it.
        paintWaveformOverlay (g, plotArea);

        // --- Y-axis frequency labels (Hz) ---
        {
            const int lowestMidi  = static_cast<int> (std::ceil (ovtdsp::hzToMidiFloat (fMin)));
            const int highestMidi = static_cast<int> (std::floor (ovtdsp::hzToMidiFloat (fMax)));
            g.setFont (ovt::fontYAxis());
            g.setColour (juce::Colour (0x44ffffff));
            // Draw labels for C notes on the right edge of the plot
            const int firstC = (lowestMidi % 12 == 0)
                                 ? lowestMidi
                                 : lowestMidi + (12 - lowestMidi % 12);
            for (int midi = firstC; midi <= highestMidi; midi += 12)
            {
                const float hz = ovtdsp::midiToHz (static_cast<float> (midi));
                const float y = plotArea.getY() + hzToY (hz, plotArea.getHeight());
                const int yi = static_cast<int> (y);
                const juce::String hzLabel = juce::String (static_cast<int> (std::round (hz)));
                g.drawText (hzLabel + " Hz", plotArea.getRight() - 50, yi - 10, 48, 12,
                            juce::Justification::centredRight);
            }
        }

        // --- Dynamic octave reference lines (C notes) ---
        {
            const int lowestMidi  = static_cast<int> (std::ceil (ovtdsp::hzToMidiFloat (fMin)));
            const int highestMidi = static_cast<int> (std::floor (ovtdsp::hzToMidiFloat (fMax)));
            const int firstC = (lowestMidi % 12 == 0)
                                 ? lowestMidi
                                 : lowestMidi + (12 - lowestMidi % 12);

            g.setColour (kGrid.brighter (0.15f));
            for (int midi = firstC; midi <= highestMidi; midi += 12)
            {
                const float hz = ovtdsp::midiToHz (static_cast<float> (midi));
                const float y = plotArea.getY() + hzToY (hz, plotArea.getHeight());
                const int yi = static_cast<int> (y);
                g.drawHorizontalLine (yi,
                                      static_cast<float> (plotArea.getX()),
                                      static_cast<float> (plotArea.getRight()));
                const int oct = ovtdsp::midiToOctave (midi);
                const juce::String label = "C" + juce::String (oct);
                g.setFont (ovt::fontOctaveLabel());
                g.setColour (ebs::isDark() ? juce::Colour (0x55ffffff) : juce::Colour (0x55000000));
                g.drawText (label, plotArea.getX() + 2, yi - 10, 28, 12,
                            juce::Justification::centredLeft);
                g.setColour (kGrid.brighter (0.15f));
            }

            // Scale note lines
            g.setColour (ebs::scaleLine());
            for (int midi = lowestMidi; midi <= highestMidi; ++midi)
            {
                const int noteInOct = ovtdsp::midiToNoteInOctave (midi);
                if (noteInOct == 0) continue;
                if (! scaleIntervals.contains (noteInOct)) continue;
                const float hz = ovtdsp::midiToHz (static_cast<float> (midi));
                const float y = plotArea.getY() + hzToY (hz, plotArea.getHeight());
                g.drawHorizontalLine (static_cast<int> (y),
                                      static_cast<float> (plotArea.getX()),
                                      static_cast<float> (plotArea.getRight()));
            }
        }

        // Input pitch curve (red)
        if (inputHistory.size() > 1)
        {
            juce::Path p;
            const float dx = static_cast<float> (plotArea.getWidth()) / static_cast<float> (historySize - 1);
            for (int i = 0; i < inputHistory.size(); ++i)
            {
                const float hz = inputHistory[i];
                if (hz <= 0.0f) continue;
                const float y = plotArea.getY() + hzToY (hz, plotArea.getHeight());
                const float x = plotArea.getX() + dx * (historySize - inputHistory.size() + i);
                if (i == 0 || inputHistory[i - 1] <= 0.0f) p.startNewSubPath (x, y);
                else p.lineTo (x, y);
            }
            g.setColour (kInputColour);
            g.strokePath (p, juce::PathStrokeType (1.5f));
        }

        // Output pitch curve (green)
        if (outputHistory.size() > 1)
        {
            juce::Path p;
            const float dx = static_cast<float> (plotArea.getWidth()) / static_cast<float> (historySize - 1);
            for (int i = 0; i < outputHistory.size(); ++i)
            {
                const float hz = outputHistory[i];
                if (hz <= 0.0f) continue;
                const float y = plotArea.getY() + hzToY (hz, plotArea.getHeight());
                const float x = plotArea.getX() + dx * (historySize - outputHistory.size() + i);
                if (i == 0 || outputHistory[i - 1] <= 0.0f) p.startNewSubPath (x, y);
                else p.lineTo (x, y);
            }
            g.setColour (kOutputColour);
            g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Harmony voices (blue) - only when enabled via the Curve Editor menu
        if (showHarmonies)
        {
            const float dx = static_cast<float> (plotArea.getWidth()) / static_cast<float> (historySize - 1);
            for (int v = 0; v < maxHarmonyVoices; ++v)
            {
                const auto& h = harmonyHistory[v];
                if (h.size() <= 1) continue;
                juce::Path p;
                bool hasAny = false;
                bool segmentOpen = false;
                for (int i = 0; i < h.size(); ++i)
                {
                    const float hz = h[i];
                    const float x = plotArea.getX() + dx * (historySize - h.size() + i);
                    if (hz <= 0.0f) { segmentOpen = false; continue; }
                    const float y = plotArea.getY() + hzToY (hz, plotArea.getHeight());
                    if (! segmentOpen) { p.startNewSubPath (x, y); segmentOpen = true; hasAny = true; }
                    else p.lineTo (x, y);
                }
                if (hasAny)
                {
                    g.setColour (kHarmonyColour);
                    g.strokePath (p, juce::PathStrokeType (0.8f));
                }
            }
        }

        g.restoreState();

        // === "FOLLOWS MIDI IN" badge (pulsing glow, top-right of plot) ===
        if (midiFollowActive)
        {
            const float pulse = 0.5f + 0.5f * std::sin (juce::Time::getMillisecondCounter() * 0.006f);
            const juce::String midiTxt = ovt::tr (ovt::Keys::kLabelMidiFollowBadge);
            g.setFont (ovt::fontMeter0());
            const int textW = g.getCurrentFont().getStringWidth (midiTxt);
            const int padX = 10;
            const int w = textW + padX * 2;
            const int h = 20;
            const int x = plotArea.getRight() - w - 12;
            const int y = plotArea.getY() + 10;
            const juce::Colour base = juce::Colour (0xffff9800); // amber
            // Outer glow (pulsing)
            g.setColour (base.withAlpha (0.22f + 0.20f * pulse));
            g.fillRoundedRectangle ((float) (x - 4), (float) (y - 4),
                                    (float) (w + 8), (float) (h + 8), 10.0f);
            // Body
            g.setColour (base.withAlpha (0.9f));
            g.fillRoundedRectangle ((float) x, (float) y, (float) w, (float) h, 6.0f);
            // Pulsing border
            g.setColour (juce::Colours::white.withAlpha (0.5f + 0.45f * pulse));
            g.drawRoundedRectangle ((float) x, (float) y, (float) w, (float) h, 6.0f, 1.0f);
            // Text (dark on amber for contrast)
            g.setColour (juce::Colours::black);
            g.drawText (midiTxt, x, y, w, h, juce::Justification::centred);
        }

        // --- Hover cursor (crosshair + Hz/note readout) ---
        if (isMouseOverPlot && hoverHz > 0.0f)
        {
            const int hoverY = plotArea.getY() + static_cast<int> (hzToY (hoverHz, plotArea.getHeight()));
            // Horizontal crosshair line
            g.setColour (juce::Colour (0x44ffffff));
            g.drawHorizontalLine (hoverY,
                                  static_cast<float> (plotArea.getX()),
                                  static_cast<float> (plotArea.getRight()));
            // Readout box
            const juce::String noteName = ovtdsp::hzToNoteName (hoverHz);
            const juce::String hzText = juce::String (static_cast<int> (std::round (hoverHz))) + " Hz";
            const juce::String readout = noteName + "  " + hzText;
            g.setFont (ovt::fontReadout());
            // Round UP + 2px safety margin: getStringWidth truncating cast
            // used to leave the "Hz" suffix clipped with "..." ellipsis.
            const int textW = static_cast<int> (std::ceil (
                juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), readout))) + 2;
            const int boxW = textW + 14;
            const int boxH = 16;
            int boxX = plotArea.getX() + 4;
            int boxY = hoverY - boxH - 4;
            if (boxY < plotArea.getY()) boxY = hoverY + 4;
            // Keep the whole readout inside the plot so the "Hz" suffix is
            // never clipped by the component's right edge.
            boxX = juce::jmin (boxX, plotArea.getRight() - boxW - 2);
            boxX = juce::jmax (boxX, plotArea.getX() + 2);
            g.setColour (juce::Colour (0xcc15151e));
            g.fillRoundedRectangle ((float) boxX, (float) boxY, (float) boxW, (float) boxH, 3.0f);
            g.setColour (juce::Colour (0x661A9AF0));
            g.drawRoundedRectangle ((float) boxX, (float) boxY, (float) boxW, (float) boxH, 3.0f, 0.5f);
            g.setColour (juce::Colours::white);
            g.drawText (readout, boxX + 7, boxY, boxW - 14, boxH,
                        juce::Justification::centredLeft);
        }

        // === Legend + Stats panel (bottom-right of plot area) ===
        {
            const int panelW = 160;
            const int panelH = 68;
            const int panelX = plotArea.getRight() - panelW - 4;
            const int panelY = plotArea.getBottom() - panelH - 4;

            // Background (semi-translucent)
            g.setColour (ebs::vizLegendBg().withAlpha (0.85f));
            g.fillRoundedRectangle ((float) panelX, (float) panelY,
                                    (float) panelW, (float) panelH, 4.0f);

            // Row 1: Curve legend
            g.setFont (ovt::fontLegend());
            int ly = panelY + 4;
            g.setColour (kInputColour);
            g.drawText (ovt::tr(ovt::Keys::kLegendInput), panelX + 4, ly, 38, 10, juce::Justification::centredLeft);
            g.setColour (kOutputColour);
            g.drawText (ovt::tr(ovt::Keys::kLegendOutput), panelX + 44, ly, 40, 10, juce::Justification::centredLeft);
            if (showHarmonies)
            {
                g.setColour (kHarmonyColour);
                g.drawText (ovt::tr(ovt::Keys::kLegendHarmony), panelX + 88, ly, 26, 10, juce::Justification::centredLeft);
            }
            ly += 12;

            // Row 2: Shortcut hints
            g.setColour (juce::Colours::grey.withAlpha (0.7f));
            g.setFont (ovt::fontLegendHint());
            g.drawText (ovt::tr(ovt::Keys::kLegendScrollHint), panelX + 4, ly, 72, 10, juce::Justification::centredLeft);
            g.drawText (ovt::tr(ovt::Keys::kLegendZoomHint), panelX + 80, ly, 76, 10, juce::Justification::centredLeft);
            ly += 12;

            // Row 3: Tuning statistics
            g.setFont (ovt::fontLegendHint());
            const int inTunePct = static_cast<int> (percentInTune);
            const juce::String statsText = juce::String (inTunePct) + "% " + ovt::tr (ovt::Keys::kLegendInTune);
            juce::Colour statsColour;
            if (inTunePct >= 80)      statsColour = juce::Colour (0xff4caf50);
            else if (inTunePct >= 50) statsColour = juce::Colour (0xffffc107);
            else                      statsColour = juce::Colour (0xffff9800);
            g.setColour (statsColour);
            g.drawText (statsText, panelX + 4, ly, 72, 10, juce::Justification::centredLeft);

            if (centsHistory.size() > 0)
            {
                g.setColour (juce::Colour (0x88ffffff));
                const juce::String avgText = "avg:" + juce::String (static_cast<int> (avgCents)) + "c";
                g.drawText (avgText, panelX + 80, ly, 76, 10, juce::Justification::centredLeft);
            }
        }
    }

    void PitchVisualizer::resized()
    {
        const int headerH = juce::jmin (50, getHeight() / 4);
        pianoKeyboard.setBounds (0, headerH, 60, getHeight() - headerH);

        // SVG icon buttons: top-right corner of the header.
        // Order: zoom in, zoom out, scroll up, scroll down, reset
        const int btnSize = 20;
        const int btnGap = 3;
        const int totalW = 5 * btnSize + 4 * btnGap;
        int bx = getWidth() - totalW - 8;
        const int by = (headerH - btnSize) / 2;

        zoomInButton.setBounds    (bx, by, btnSize, btnSize); bx += btnSize + btnGap;
        zoomOutButton.setBounds   (bx, by, btnSize, btnSize); bx += btnSize + btnGap;
        scrollUpButton.setBounds  (bx, by, btnSize, btnSize); bx += btnSize + btnGap;
        scrollDownButton.setBounds(bx, by, btnSize, btnSize); bx += btnSize + btnGap;
        resetViewButton.setBounds (bx, by, btnSize, btnSize);
    }

    void PitchVisualizer::updateStatistics()
    {
        if (centsHistory.size() < 2) return;

        // Calculate average
        float sum = 0.0f;
        for (int i = 0; i < centsHistory.size(); ++i)
            sum += centsHistory[i];
        avgCents = sum / static_cast<float> (centsHistory.size());

        // Calculate standard deviation
        float sumSq = 0.0f;
        for (int i = 0; i < centsHistory.size(); ++i)
        {
            float diff = centsHistory[i] - avgCents;
            sumSq += diff * diff;
        }
        stdDevCents = std::sqrt (sumSq / static_cast<float> (centsHistory.size()));

        // Calculate in-tune percentage (within +/- 15 cents)
        int inTune = 0;
        for (int i = 0; i < centsHistory.size(); ++i)
            if (std::abs (centsHistory[i]) <= 15.0f) ++inTune;
        percentInTune = (centsHistory.size() > 0)
            ? (100.0f * static_cast<float> (inTune) / static_cast<float> (centsHistory.size()))
            : 0.0f;
    }

    void PitchVisualizer::timerCallback()
    {
        pianoKeyboard.setCurrentPitches (latestInputHz, latestOutputHz);

        // Track cents for statistics
        if (noteInfo.valid)
        {
            if (centsHistory.size() >= statsWindowSize) centsHistory.remove (0);
            centsHistory.add (noteInfo.cents);
            updateStatistics();
        }

        // Smooth animated zoom/scroll transitions.
        if (animating)
        {
            const float lerp = 0.25f;
            fMin += (targetFMin - fMin) * lerp;
            fMax += (targetFMax - fMax) * lerp;
            if (std::abs (fMin - targetFMin) < 0.1f && std::abs (fMax - targetFMax) < 0.1f)
            {
                fMin = targetFMin;
                fMax = targetFMax;
                animating = false;
                if (onZoomChanged) onZoomChanged (fMin, fMax);
            }
            pianoKeyboard.setRange (static_cast<int> (ovtdsp::hzToMidiFloat (fMin)),
                                    static_cast<int> (ovtdsp::hzToMidiFloat (fMax)));
        }

        // Animate unified note badge width (single vs split mode).
        {
            const bool showTarget = noteInfo.valid
                                    && noteInfo.targetName != noteInfo.name;
            badgeTargetW = showTarget ? 180.0f : 90.0f;
            badgeTargetSplitX = showTarget ? 90.0f : 90.0f;
            const float bwLerp = 0.20f;
            badgeAnimW += (badgeTargetW - badgeAnimW) * bwLerp;
            badgeSplitX += (badgeTargetSplitX - badgeSplitX) * bwLerp;
        }

        // Auto-center: keep the output pitch vertically centered.
        // Uses log-space smoothing (matches the semi-log Y-axis) and
        // direct view lerp for fluid, jank-free scrolling.
        if (autoCenter && latestOutputHz > 0.0f)
        {
            // IIR smoothing in log space to absorb vibrato and fast pitch jitter.
            const float targetLogHz = std::log (latestOutputHz);
            const float smoothCoeff = 0.12f;
            smoothedOutputHz += (targetLogHz - smoothedOutputHz) * smoothCoeff;

            const float halfRangeLog = (std::log (fMax) - std::log (fMin)) / 2.0f;
            const float newMin = std::exp (smoothedOutputHz - halfRangeLog);
            const float newMax = std::exp (smoothedOutputHz + halfRangeLog);

            // Lerp the actual view for fluid movement (matches the zoom animation).
            const float viewLerp = 0.18f;
            fMin += (newMin - fMin) * viewLerp;
            fMax += (newMax - fMax) * viewLerp;

            // Clamp to safe frequency range.
            if (fMin < 16.35f) { float r = fMax / fMin; fMin = 16.35f; fMax = fMin * r; }
            if (fMax > 8372.0f) { float r = fMax / fMin; fMax = 8372.0f; fMin = fMax / r; }

            // Keep targets in sync so a manual interaction starts from the correct position.
            targetFMin = fMin;
            targetFMax = fMax;
            pianoKeyboard.setRange (static_cast<int> (ovtdsp::hzToMidiFloat (fMin)),
                                    static_cast<int> (ovtdsp::hzToMidiFloat (fMax)));
        }
        else if (autoCenter && latestOutputHz <= 0.0f)
        {
            // Signal lost - keep the last known center position.
            // Do NOT reset smoothedOutputHz; the view stays put until a new
            // note is detected, preventing the scroll-to-bottom on silence.
        }

        repaint();
    }

    void PitchVisualizer::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        // If a pinch (mouseMagnify) is in progress, the trackpad may also emit a
        // smooth scroll from finger translation. Let the pinch own the gesture and
        // ignore the concurrent scroll so zoom and scroll don't fight.
        if (juce::Time::getMillisecondCounter() - lastMagnifyMs < 120)
            return;

        autoCenter = false;
        float scrollAmount = wheel.deltaY;

        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            float zoomFactor = 1.0f - scrollAmount * 2.0f;
            if (zoomFactor < 0.1f) zoomFactor = 0.1f;
            if (zoomFactor > 10.0f) zoomFactor = 10.0f;

            float centerPitch = std::exp (std::log (fMin) + (std::log (fMax) - std::log (fMin)) * 0.5f);
            float currentRangeCents = 1200.0f * std::log2 (fMax / fMin);
            float newRangeCents = currentRangeCents * zoomFactor;

            if (newRangeCents < 1200.0f) newRangeCents = 1200.0f;
            if (newRangeCents > 1200.0f * 8.0f) newRangeCents = 1200.0f * 8.0f;

            float halfRangeLog = (newRangeCents / 1200.0f) * std::log (2.0f) / 2.0f;
            float centerLog = std::log (centerPitch);

            fMin = targetFMin = std::exp (centerLog - halfRangeLog);
            fMax = targetFMax = std::exp (centerLog + halfRangeLog);
        }
        else
        {
            float currentRangeLog = std::log (fMax / fMin);
            float shiftLog = scrollAmount * currentRangeLog * 0.5f;
            fMin = targetFMin = std::exp (std::log (fMin) + shiftLog);
            fMax = targetFMax = std::exp (std::log (fMax) + shiftLog);
        }

        if (targetFMin < 16.35f) { float r = targetFMax / targetFMin; targetFMin = fMin = 16.35f; targetFMax = fMax = targetFMin * r; }
        if (targetFMax > 8372.0f) { float r = targetFMax / targetFMin; targetFMax = fMax = 8372.0f; targetFMin = fMin = targetFMax / r; }

        // Apply immediately (no animation) so trackpad gestures feel as responsive
        // as the Curve Editor. The animated transition is kept for the toolbar
        // buttons only.
        pianoKeyboard.setRange (static_cast<int> (ovtdsp::hzToMidiFloat (fMin)),
                                static_cast<int> (ovtdsp::hzToMidiFloat (fMax)));
        animating = false;
        if (onZoomChanged) onZoomChanged (fMin, fMax);
        repaint();
    }

    void PitchVisualizer::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
    {
        lastMagnifyMs = juce::Time::getMillisecondCounter();

        // macOS trackpad pinch: scaleFactor > 1 => pinch out => zoom in (narrower
        // range). Mirrors the Curve Editor's mouseMagnify/applyZoom. Zoom is
        // centred on the pitch under the cursor (or the view centre).
        const int headerH = juce::jmin (50, getHeight() / 4);
        const int pianoW = pianoKeyboard.getWidth() > 0 ? pianoKeyboard.getWidth() : 60;
        const auto plotArea = juce::Rectangle<int> (pianoW, headerH, getWidth() - pianoW, getHeight() - headerH);

        const float anchor = (e.position.y >= plotArea.getY() && e.position.y <= plotArea.getBottom())
                                    ? yToHz (static_cast<float> (e.position.y - plotArea.getY()), plotArea.getHeight())
                                    : std::sqrt (fMin * fMax);

        // Multiplicative zoom: range /= scaleFactor (same convention as the
        // Curve Editor, so pinch-out zooms in).
        float currentRangeCents = 1200.0f * std::log2 (fMax / fMin);
        float newRangeCents = currentRangeCents / juce::jlimit (0.1f, 10.0f, scaleFactor);
        if (newRangeCents < 1200.0f)         newRangeCents = 1200.0f;
        if (newRangeCents > 1200.0f * 8.0f) newRangeCents = 1200.0f * 8.0f;

        const float halfRangeLog = (newRangeCents / 1200.0f) * std::log (2.0f) / 2.0f;
        const float centerLog = std::log (anchor);
        fMin = targetFMin = std::exp (centerLog - halfRangeLog);
        fMax = targetFMax = std::exp (centerLog + halfRangeLog);

        if (targetFMin < 16.35f) { float r = targetFMax / targetFMin; targetFMin = fMin = 16.35f; targetFMax = fMax = targetFMin * r; }
        if (targetFMax > 8372.0f) { float r = targetFMax / targetFMin; targetFMax = fMax = 8372.0f; targetFMin = fMin = targetFMax / r; }

        // Apply immediately (no animation) so the pinch tracks the fingers 1:1,
        // matching the Curve Editor.
        pianoKeyboard.setRange (static_cast<int> (ovtdsp::hzToMidiFloat (fMin)),
                                static_cast<int> (ovtdsp::hzToMidiFloat (fMax)));
        animating = false;
        repaint();
    }

    void PitchVisualizer::scrollUp()
    {
        autoCenter = false;
        float currentRangeLog = std::log (fMax / fMin);
        float shiftLog = currentRangeLog * 0.15f;
        targetFMin = std::exp (std::log (fMin) + shiftLog);
        targetFMax = std::exp (std::log (fMax) + shiftLog);
        if (targetFMax > 8372.0f) { float r = targetFMax / targetFMin; targetFMax = 8372.0f; targetFMin = targetFMax / r; }
        animating = true;
    }

    void PitchVisualizer::scrollDown()
    {
        autoCenter = false;
        float currentRangeLog = std::log (fMax / fMin);
        float shiftLog = currentRangeLog * 0.15f;
        targetFMin = std::exp (std::log (fMin) - shiftLog);
        targetFMax = std::exp (std::log (fMax) - shiftLog);
        if (targetFMin < 16.35f) { float r = targetFMax / targetFMin; targetFMin = 16.35f; targetFMax = targetFMin * r; }
        animating = true;
    }

    void PitchVisualizer::zoomIn()
    {
        autoCenter = false;
        float centerPitch = std::exp (std::log (fMin) + (std::log (fMax) - std::log (fMin)) * 0.5f);
        float currentRangeCents = 1200.0f * std::log2 (fMax / fMin);
        float newRangeCents = currentRangeCents * 0.7f;
        if (newRangeCents < 1200.0f) newRangeCents = 1200.0f;
        float halfRangeLog = (newRangeCents / 1200.0f) * std::log (2.0f) / 2.0f;
        float centerLog = std::log (centerPitch);
        targetFMin = std::exp (centerLog - halfRangeLog);
        targetFMax = std::exp (centerLog + halfRangeLog);
        animating = true;
    }

    void PitchVisualizer::zoomOut()
    {
        autoCenter = false;
        float centerPitch = std::exp (std::log (fMin) + (std::log (fMax) - std::log (fMin)) * 0.5f);
        float currentRangeCents = 1200.0f * std::log2 (fMax / fMin);
        float newRangeCents = currentRangeCents * 1.4f;
        if (newRangeCents > 1200.0f * 8.0f) newRangeCents = 1200.0f * 8.0f;
        float halfRangeLog = (newRangeCents / 1200.0f) * std::log (2.0f) / 2.0f;
        float centerLog = std::log (centerPitch);
        targetFMin = std::exp (centerLog - halfRangeLog);
        targetFMax = std::exp (centerLog + halfRangeLog);
        if (targetFMin < 16.35f) { float r = targetFMax / targetFMin; targetFMin = 16.35f; targetFMax = targetFMin * r; }
        if (targetFMax > 8372.0f) { float r = targetFMax / targetFMin; targetFMax = 8372.0f; targetFMin = targetFMax / r; }
        animating = true;
    }

    void PitchVisualizer::resetView()
    {
        autoCenter = false;
        targetFMin = kDefaultFMin;
        targetFMax = kDefaultFMax;
        animating = true;
    }

    void PitchVisualizer::setAutoCenter (bool enabled)
    {
        autoCenter = enabled;
        if (enabled && latestOutputHz > 0.0f)
        {
            // Seed the smoothed value with the current pitch (log space).
            smoothedOutputHz = std::log (latestOutputHz);
            const float halfRangeLog = (std::log (fMax) - std::log (fMin)) / 2.0f;
            targetFMin = std::exp (smoothedOutputHz - halfRangeLog);
            targetFMax = std::exp (smoothedOutputHz + halfRangeLog);
            animating = true;
        }
    }

    void PitchVisualizer::setZoomRange (float newFMin, float newFMax)
    {
        fMin = targetFMin = juce::jmax (16.0f, newFMin);
        fMax = targetFMax = juce::jmin (8372.0f, newFMax);
        if (fMin >= fMax) { fMin = targetFMin = 50.0f; fMax = targetFMax = 1500.0f; }
        pianoKeyboard.setRange (static_cast<int> (ovtdsp::hzToMidiFloat (fMin)),
                                static_cast<int> (ovtdsp::hzToMidiFloat (fMax)));
    }

    void PitchVisualizer::mouseDown (const juce::MouseEvent& event)
    {
        if (event.mods.isRightButtonDown())
        {
            if (onRightClick)
                onRightClick();
            return;
        }
        Component::mouseDown (event);
    }

    bool PitchVisualizer::exportAsImage (const juce::File& filePath)
    {
        // Render the component to a high-quality image at 2x resolution.
        const int scale = 2;
        const int w = getWidth() * scale;
        const int h = getHeight() * scale;
        if (w <= 0 || h <= 0) return false;

        // Create an opaque image (no alpha) to avoid transparency issues
        juce::Image image (juce::Image::RGB, w, h, true);
        {
            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::scale ((float) scale));
            paint (g);
        }

        juce::PNGImageFormat png;
        juce::FileOutputStream stream (filePath);
        if (stream.failedToOpen()) return false;
        return png.writeImageToStream (image, stream);
    }
}



