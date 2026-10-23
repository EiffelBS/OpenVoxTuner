#pragma once
// MorphCoverageTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// The A/B morph drives a HAND-WRITTEN whitelist (ovtdsp::getMorphParameterIds), and until this
// test existed nothing compared it with the parameters the plugin actually declares. That is how
// "harmony_attack" ended up CAPTURED but never APPLIED: a morph carried the value in its state
// and never wrote it, so an A/B move left the per-voice fade-in where it was.
//
// This test reads the parameter declarations straight out of the plugin sources (the path
// arrives as a compile definition) and classifies EVERY one of them: either the morph drives it,
// or it is listed here with the reason it must not. A new parameter therefore has to make a
// decision, and the build fails until it does. The parameter table has its own translation unit
// (PluginParameters.cpp) since the processor was split, so both files are scanned as one text.

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <set>

#include "../../Source/dsp/PresetMorpher.h"

#ifndef OVT_SOURCE_DIR
 #error "OVT_SOURCE_DIR must point at the Source directory (see the OpenVoxTunerTests target)"
#endif

class MorphCoverageTest : public juce::UnitTest
{
public:
    MorphCoverageTest() : juce::UnitTest ("MorphCoverage") {}

    void runTest() override
    {
        // --- every parameter the plugin declares ------------------------------------------------
        // The declarations are read from every source file that can hold them: the parameter
        // table lives in PluginParameters.cpp, and PluginProcessor.cpp is scanned too so that a
        // parameter added back next to the constructor is never silently invisible.
        const juce::StringArray sourceNames { "PluginProcessor.cpp", "PluginParameters.cpp" };
        juce::String text;

        for (const auto& sourceName : sourceNames)
        {
            const juce::File source (juce::String (OVT_SOURCE_DIR) + "/" + sourceName);

            if (! source.existsAsFile())
            {
                expect (false, "Source/" + sourceName + " is readable at OVT_SOURCE_DIR");
                return;
            }

            text += source.loadFileAsString();
        }

        // Plain scanning rather than a regex: every parameter is built as
        // `make_unique<juce::AudioParameterXxx> ("id", "Name", ...)`, so the first quoted string
        // after the construction is the id. No regex engine, no pattern to get wrong.
        std::set<juce::String> declared;
        {
            const juce::String needle ("make_unique<juce::AudioParameter");
            int pos = 0;

            while (true)
            {
                const int at = text.indexOf (pos, needle);
                if (at < 0)
                    break;

                const int openQuote = text.indexOfChar (at, '"');
                const int closeQuote = openQuote >= 0 ? text.indexOfChar (openQuote + 1, '"') : -1;
                if (openQuote < 0 || closeQuote < 0)
                    break;

                declared.insert (text.substring (openQuote + 1, closeQuote));
                pos = closeQuote + 1;
            }
        }

        expectGreaterThan (static_cast<int> (declared.size()), 50,
                           "the parameter declarations were found in the source");

        // --- what the morph drives, and what it deliberately does not ---------------------------
        std::set<juce::String> morphed;
        for (const auto& id : ovtdsp::getMorphParameterIds())
            morphed.insert (id);

        // Classified by hand, each with the reason it stays out of an A/B morph. UI state and
        // debug switches are not part of a SOUND; engine and routing choices are configuration a
        // morph must not flip mid-performance. The twelve custom scale notes USED to be excluded
        // here as a known limitation (the morph carried key/scale but not the notes); they are
        // morphed now, so a morph to or from a Custom scale lands on the right set of notes.
        const std::set<juce::String> notMorphed {
            // Editor / UI state
            "ui_theme", "ui_language", "ui_show_waveform", "ui_waveform_type", "ui_auto_center",
            "auto_scroll", "editor_playhead_loop", "viz_fmin", "viz_fmax", "mode",
            "morph_amount",      // the morph's own depth: a morph that drove it would chase itself
            "dbg_test_grain",    // debug signal generator
            // Engine / routing choices (configuration, not sound)
            "pitch_detector", "formant_mode", "formant_strategy", "chord_detect_enable",
            "midi_out_enable", "midi_target_enable"
        };

        beginTest ("every declared parameter is either morphed or explicitly excluded");

        juce::StringArray unclassified;
        for (const auto& id : declared)
            if (morphed.count (id) == 0 && notMorphed.count (id) == 0)
                unclassified.add (id);

        if (! unclassified.isEmpty())
            std::cout << "       unclassified parameters: " << unclassified.joinIntoString (", ") << "\n";

        expect (unclassified.isEmpty(),
                "no declared parameter is left unclassified (add it to the morph whitelist or to "
                "the exclusion list with a reason)");

        beginTest ("the morph whitelist names parameters that exist");

        juce::StringArray ghosts;
        for (const auto& id : morphed)
            if (declared.count (id) == 0)
                ghosts.add (id);

        if (! ghosts.isEmpty())
            std::cout << "       whitelisted but not declared: " << ghosts.joinIntoString (", ") << "\n";

        expect (ghosts.isEmpty(),
                "every whitelisted id is a real parameter (a typo would silently morph nothing)");

        std::cout << "       morph coverage: " << declared.size() << " parameters declared, "
                  << morphed.size() << " morphed, " << notMorphed.size() << " excluded by policy\n";
    }
};

static MorphCoverageTest morphCoverageTest;
