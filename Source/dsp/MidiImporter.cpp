// MidiImporter.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "MidiImporter.h"
#include "NoteUtils.h"
#include <map>
#include <vector>
#include <algorithm>

namespace ovtdsp
{
    /** Internal helper: pairs of note-on and note-off events from a MIDI file. */
    struct MidiNoteSpan
    {
        double startTime;  // seconds
        double endTime;    // seconds
        int noteNum;       // MIDI note 0-127
        int channel;       // MIDI channel 1-16
        float velocity;    // 0.0-1.0
    };

    /** Extract all complete note spans from a parsed MidiFile.
     *  A "complete note" is a note-on that has a matching note-off.
     *  Channel 10 (percussion) is always excluded. */
    static juce::Array<MidiNoteSpan> extractNoteSpans (
        const juce::MidiFile& mf,
        int filterChannel = -1) // -1 = all non-percussion channels
    {
        juce::Array<MidiNoteSpan> spans;

        // First pass: pair note-on with note-off per channel
        struct PendingNote { int channel; double time; float velocity; };
        std::map<std::pair<int, int>, PendingNote> pending; // (channel, note) -> pending

        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            const auto* track = mf.getTrack (t);
            if (track == nullptr) continue;

            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                const auto& msg = track->getEventPointer (i)->message;

                if (! msg.isNoteOn() && ! msg.isNoteOff())
                    continue;

                const int ch = msg.getChannel(); // 1-16
                if (ch == 10) continue;           // always skip percussion
                if (filterChannel >= 1 && filterChannel <= 16 && ch != filterChannel)
                    continue;

                const int note = msg.getNoteNumber();

                if (msg.isNoteOn() && msg.getVelocity() > 0)
                {
                    PendingNote pn;
                    pn.channel = ch;
                    pn.time = msg.getTimeStamp();
                    pn.velocity = msg.getVelocity() / 127.0f;
                    pending[{ ch, note }] = pn;
                }
                else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
                {
                    auto it = pending.find ({ ch, note });
                    if (it != pending.end())
                    {
                        MidiNoteSpan span;
                        span.startTime = it->second.time;
                        span.endTime = msg.getTimeStamp();
                        span.noteNum = note;
                        span.channel = ch;
                        span.velocity = it->second.velocity;

                        // Minimum duration check: skip artefact notes < 20ms
                        if (span.endTime - span.startTime >= 0.02)
                            spans.add (span);

                        pending.erase (it);
                    }
                }
            }
        }

        // Sort by start time
        std::sort (spans.begin(), spans.end(),
                   [] (const MidiNoteSpan& a, const MidiNoteSpan& b)
                   { return a.startTime < b.startTime; });

        return spans;
    }

    MidiImportInfo MidiImporter::analyzeFile (const juce::File& file)
    {
        MidiImportInfo info;

        juce::FileInputStream stream (file);
        if (stream.failedToOpen())
        {
            info.errorMessage = "Cannot open file: " + file.getFullPathName();
            return info;
        }

        juce::MidiFile mf;
        if (! mf.readFrom (stream))
        {
            info.errorMessage = "Invalid MIDI file format.";
            return info;
        }

        mf.convertTimestampTicksToSeconds();

        info.numTracks = mf.getNumTracks();
        info.totalDurationSec = mf.getLastTimestamp();

        // Count notes per channel (excluding channel 10)
        std::map<int, MidiChannelInfo> channelMap;

        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            const auto* track = mf.getTrack (t);
            if (track == nullptr) continue;

            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                const auto& msg = track->getEventPointer (i)->message;

                if (! msg.isNoteOn() || msg.getVelocity() == 0)
                    continue;

                const int ch = msg.getChannel();
                if (ch == 10) continue;

                auto& ci = channelMap[ch];
                ci.channel = ch;
                ci.numNotes++;

                const int note = msg.getNoteNumber();
                ci.minNote = std::min (ci.minNote, note);
                ci.maxNote = std::max (ci.maxNote, note);
            }
        }

        // Convert map to sorted array
        for (auto& [ch, ci] : channelMap)
            info.channels.add (ci);

        // Sort by channel number using std::sort on raw data (avoids JUCE Array::sort template issue)
        std::sort (info.channels.getRawDataPointer(),
                   info.channels.getRawDataPointer() + info.channels.size(),
                   [] (const MidiChannelInfo& a, const MidiChannelInfo& b)
                   { return a.channel < b.channel; });

        info.totalNotes = 0;
        for (auto& ci : info.channels)
            info.totalNotes += ci.numNotes;

        // Compute per-channel duration from actual note spans
        for (auto& ci : info.channels)
        {
            auto spans = extractNoteSpans (mf, ci.channel);
            if (! spans.isEmpty())
            {
                double minT = spans.getFirst().startTime;
                double maxT = spans.getLast().endTime;
                ci.durationSec = maxT - minT;
            }
        }

        info.isValid = (info.totalNotes > 0);
        if (! info.isValid)
            info.errorMessage = "MIDI file contains no notes.";

        return info;
    }

    PitchCurve MidiImporter::importFrom (
        const juce::File& file,
        MidiExtractStrategy strategy,
        int targetChannel)
    {
        PitchCurve curve;

        juce::FileInputStream stream (file);
        if (stream.failedToOpen())
            return curve;

        juce::MidiFile mf;
        if (! mf.readFrom (stream))
            return curve;

        mf.convertTimestampTicksToSeconds();

        // Determine channel filter for SpecificChannel strategy
        int filterChannel = -1;
        if (strategy == MidiExtractStrategy::SpecificChannel)
            filterChannel = targetChannel;

        auto spans = extractNoteSpans (mf, filterChannel);
        if (spans.isEmpty())
            return curve;

        // Build time-sorted event list: all note-on and note-off instants
        struct TimedEvent
        {
            double time = 0.0;
            int note = 0;
            bool isOn = true;       // true = note-on, false = note-off
            float velocity = 0.0f;
        };
        juce::Array<TimedEvent> events;

        for (auto& span : spans)
        {
            TimedEvent eOn;  eOn.time = span.startTime; eOn.note = span.noteNum;
            eOn.isOn = true;  eOn.velocity = span.velocity;
            events.add (eOn);

            TimedEvent eOff; eOff.time = span.endTime;  eOff.note = span.noteNum;
            eOff.isOn = false; eOff.velocity = 0.0f;
            events.add (eOff);
        }

        // Sort events by time using std::sort on the raw array data
        std::sort (events.getRawDataPointer(),
                   events.getRawDataPointer() + events.size(),
                   [] (const TimedEvent& a, const TimedEvent& b)
                   { return a.time < b.time; });

        // Sweep-line: at each event, maintain the set of active notes,
        // select one based on strategy, and emit a curve point.
        struct ActiveNote { int note; float velocity; };
        std::map<int, ActiveNote> active; // note -> ActiveNote

        int lastSelectedNote = -1;

        for (int i = 0; i < events.size(); ++i)
        {
            const auto& ev = events.getUnchecked (i);

            if (ev.isOn)
                active[ev.note] = { ev.note, ev.velocity };
            else
                active.erase (ev.note);

            if (active.empty())
                continue;

            // Select note based on strategy
            int selectedNote = -1;

            if (strategy == MidiExtractStrategy::HighestNote
                || strategy == MidiExtractStrategy::SpecificChannel)
            {
                // Highest note (for SpecificChannel, this is the same as "highest
                // within the channel" since spans are already filtered)
                int highest = -1;
                for (auto& [note, info] : active)
                    if (note > highest) { highest = note; selectedNote = note; }
            }
            else if (strategy == MidiExtractStrategy::LowestNote)
            {
                int lowest = 128;
                for (auto& [note, info] : active)
                    if (note < lowest) { lowest = note; selectedNote = note; }
            }
            else if (strategy == MidiExtractStrategy::LoudestNote)
            {
                float maxVel = -1.0f;
                for (auto& [note, info] : active)
                    if (info.velocity > maxVel) { maxVel = info.velocity; selectedNote = note; }
            }

            if (selectedNote < 0)
                continue;

            // Only emit a point when the selected note changes
            if (selectedNote != lastSelectedNote)
            {
                const float hz = midiToHz (static_cast<float> (selectedNote));
                curve.addOrUpdatePoint (ev.time, hz);
                lastSelectedNote = selectedNote;
            }
        }

        return curve;
    }
}
