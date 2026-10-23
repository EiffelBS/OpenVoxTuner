// PluginProcessorAra.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// updateAraMetadata(): reads key signatures, bar signatures, chords and tempo
// from the ARA document controller. Moved out of PluginProcessor.cpp (step 4 of
// the source split) verbatim, both the ARA branch and the empty stub the
// OVT_ARA_ENABLED=0 build compiles.
#include "PluginProcessor.h"

#if OVT_ARA_ENABLED
#include <ARA_Library/Utilities/ARAPitchInterpretation.h>
#endif

// --- ARA metadata reading (UI thread only) ---
// Reads key signatures and bar signatures from the ARA document controller.
// MUST be called from the UI thread - HostContentReader acquires a lock that
// can deadlock the audio thread in some hosts (Cubase LE 15, Live VST3).
#if OVT_ARA_ENABLED
void OpenVoxTunerAudioProcessor::updateAraMetadata()
{
    if (! isBoundToARA())
        return;

    auto* dc = getDocumentController();
    if (dc == nullptr)
        return;

    auto* doc = dc->getDocument();
    if (doc == nullptr)
        return;

    auto contexts = doc->getMusicalContexts();
    if (contexts.empty() || contexts[0] == nullptr)
        return;

    // --- Key signatures ---
    {
        ARA::PlugIn::HostContentReader<ARA::kARAContentTypeKeySignatures> reader (contexts[0]);
        if (reader.getEventCount() > 0)
        {
            auto* keySig = reader.getDataPtrForEvent (0);
            if (keySig != nullptr)
            {
                int chromatic = ((keySig->root * 7) % 12 + 12) % 12;

                int scaleIndex = 0; // Chromatic by default
                int activeNotes = 0;
                for (int i = 0; i < 12; ++i)
                    if (keySig->intervals[i] == 0xFF) activeNotes++;

                if (activeNotes == 12)
                    scaleIndex = 0; // Chromatic
                else if (keySig->intervals[4] == 0xFF)
                    scaleIndex = 1; // Major
                else if (keySig->intervals[3] == 0xFF)
                    scaleIndex = 4; // Natural Minor

                if (keyParam && (keyIntParam != nullptr ? keyIntParam->get() : static_cast<int> (std::round (keyParam->load() * 11.0f))) != chromatic)
                {
                    if (auto* param = parameters.getParameter ("key"))
                        param->setValueNotifyingHost (param->convertTo0to1 (chromatic));
                }

                if (scaleParam && (scaleChoiceParam != nullptr ? scaleChoiceParam->getIndex() : static_cast<int> (scaleParam->load())) != scaleIndex)
                {
                    if (auto* param = parameters.getParameter ("scale"))
                        param->setValueNotifyingHost (param->convertTo0to1 (scaleIndex));
                }
            }
        }
    }

    // --- Bar signatures ---
    {
        ARA::PlugIn::HostContentReader<ARA::kARAContentTypeBarSignatures> barReader (contexts[0]);
        juce::ScopedLock lock (araBarSigLock);
        araBarSignatures.clear();
        for (int i = 0; i < barReader.getEventCount(); ++i)
        {
            auto* barSig = barReader.getDataPtrForEvent (i);
            if (barSig != nullptr)
            {
                araBarSignatures.push_back ({
                    static_cast<double> (barSig->position),
                    static_cast<int> (barSig->numerator),
                    static_cast<int> (barSig->denominator)
                });
            }
        }
        if (! araBarSignatures.empty())
        {
            currentTimeSigNumerator.store (araBarSignatures[0].numerator);
            currentTimeSigDenominator.store (araBarSignatures[0].denominator);
        }
    }

    // --- Chords (kARAContentTypeSheetChords) ---
    // Extract the lead-sheet chord (root + bass) so the Harmony engine can
    // adapt its strategy dynamically (e.g. force a diatonic harmony type
    // when the host reports a dominant-7th). ARA provides multiple events
    // sorted by position; each event is valid until the next one, and the
    // first event is assumed valid for all times before it is actually defined.
    {
        ARA::PlugIn::HostContentReader<ARA::kARAContentTypeSheetChords> chordReader (contexts[0]);
        const int chordCount = chordReader.getEventCount();

        // Rebuild the time-indexed chord-override windows from the ScaleQuantizer.
        // The last chord root/bass are still cached for copyAraChord() / GUI.
        if (scaleQuantizer != nullptr)
            scaleQuantizer->clearChordOverrides();

        if (chordCount > 0)
        {
            // Use the last chord event for the cached root/bass atomics (ARA
            // semantics: each event valid until the next).
            const int bestIdx = juce::jmax (0, chordCount - 1);
            {
                juce::ScopedLock lock (araChordLock);
                araChordEvents.clear();
                // Generate chord symbols from the interval data (the host's
                // `name` field is often empty). Unicode symbols (ø, ♭, ♯, Δ, °)
                // are clearer than ASCII ("halfdim", "b", "#", "maj", "dim").
                ARA::ChordInterpreter chordInterpreter (false);
                for (int i = 0; i < chordCount; ++i)
                {
                    auto* chord = chordReader.getDataPtrForEvent (i);
                    if (chord == nullptr)
                        continue;

                    const int rootCOF = static_cast<int> (chord->root);
                    const int bassCOF = static_cast<int> (chord->bass);

                    // ARACircleOfFifthsIndex -> chromatic pitch class 0..11.
                    // ARA convention: C=0, G=1, D=2 (circle of fifths), F=-1.
                    // Conversion: (root * 7) mod 12, wrapped into [0,11].
                    const int rootPC = ((rootCOF * 7) % 12 + 12) % 12;
                    const int bassPC = ((bassCOF * 7) % 12 + 12) % 12;

                    // Decode the chord's full pitch-class set: intervals
                    // kARAChordIntervalUsed (0xFF) or diatonic degrees (0x01..0x0D)
                    // mark the active intervals relative to the root.
                    juce::Array<int> chordNotes;
                    chordNotes.add (rootPC);
                    for (int k = 0; k < 12; ++k)
                        if (chord->intervals[k] != 0) // kARAChordIntervalUnused == 0x00
                            chordNotes.addIfNotAlreadyThere ((rootPC + k) % 12);
                    chordNotes.addIfNotAlreadyThere (bassPC); // the bass can fall outside the triad
                    chordNotes.sort();

                    // Validity window: [position, next position).
                    double start = static_cast<double> (chord->position);
                    double end   = (i + 1 < chordCount)
                                       ? static_cast<double> (chordReader.getDataPtrForEvent (i + 1)->position)
                                       : (start + 1.0e6); // last chord -> open-ended
                    const double duration = juce::jmax (0.0, end - start);

                    if (scaleQuantizer != nullptr)
                        scaleQuantizer->setChordOverride (chordNotes, start, duration);

                    AraChordEvent ev;
                    ev.root = rootCOF;
                    ev.bass = bassCOF;
                    ev.startPPQ = start;
                    ev.duration = duration;
                    ev.name = juce::String::fromUTF8 (chordInterpreter.getNameForChord (*chord).c_str());
                    // "N.C." = undefined chord (no intervals) -> fall back to
                    // the host-provided name, else empty (badge shows the root).
                    if (ev.name == "N.C.")
                        ev.name = (chord->name != nullptr) ? juce::String::fromUTF8 (chord->name) : juce::String();
                    araChordEvents.push_back (ev);

                    if (i == bestIdx)
                    {
                        araChordRoot.store (rootCOF, std::memory_order_release);
                        araChordBass.store (bassCOF, std::memory_order_release);
                    }
                }
            }
        }
        else
        {
            juce::ScopedLock lock (araChordLock);
            araChordEvents.clear();
            araChordRoot.store (-999, std::memory_order_release);
            araChordBass.store (-999, std::memory_order_release);
        }
    }

    // --- Tempo (kARAContentTypeTempoEntries) ---
    // Extract the host tempo so the plugin can align time-based DSP
    // (e.g. humanize, vibrato rate, harmony attack timing) to the actual
    // project BPM instead of guessing from the sample rate.
    // ARA tempo entries: { timePosition (sec), quarterPosition (beats) }.
    // BPM = (quarterDelta / timeDelta) * 60.
    {
        ARA::PlugIn::HostContentReader<ARA::kARAContentTypeTempoEntries> tempoReader (contexts[0]);
        const int tempoCount = tempoReader.getEventCount();
        if (tempoCount >= 2)
        {
            // Compute BPM from the first and last tempo sync points.
            auto* first = tempoReader.getDataPtrForEvent (0);
            auto* last  = tempoReader.getDataPtrForEvent (tempoCount - 1);
            if (first != nullptr && last != nullptr)
            {
                const double dt = static_cast<double> (last->timePosition) - static_cast<double> (first->timePosition);
                const double dq = static_cast<double> (last->quarterPosition) - static_cast<double> (first->quarterPosition);
                if (dt > 0.0 && dq > 0.0)
                {
                    const double bpm = (dq / dt) * 60.0;
                    araTempoBpm.store (bpm, std::memory_order_release);
                    hasAraTempoFlag.store (true, std::memory_order_release);
                }
            }
        }
        else
        {
            araTempoBpm.store (0.0, std::memory_order_release);
            hasAraTempoFlag.store (false, std::memory_order_release);
        }
    }
}
#else
void OpenVoxTunerAudioProcessor::updateAraMetadata() {}
#endif // OVT_ARA_ENABLED
