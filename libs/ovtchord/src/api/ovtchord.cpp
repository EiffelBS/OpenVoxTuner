// ovtchord.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "ovtchord/ovtchord.h"
#include "ovtchord/midi_parser.h"
#include "ovtchord/chord_engine.h"
#include "midi/note_tracker.h"
#include "audio/audio_processor.h"
#include "audio/wav_reader.h"
#include "audio/mp3_reader.h"
#include <algorithm>
#include <string>

namespace ovtchord
{
    struct OvtChordImpl
    {
        MidiParser parser;
        NoteTracker tracker;
        ChordEngine engine;
        AudioProcessor audio;
        ChordCallback callback = nullptr;
        void* userData = nullptr;
        bool running = false;
        ChordResult lastResult;
    };

    OvtChordHandle ovtchord_init (const OvtChordConfig* config)
    {
        auto* ctx = new OvtChordImpl();
        ChordEngine::Config ecfg;
        if (config != nullptr)
        {
            ecfg.minNotes = config->minNotes;
            ecfg.minConfidence = config->minConfidence;
            ecfg.stabilityFrames = config->stabilityFrames;
        }
        ctx->engine = ChordEngine (ecfg);
        return ctx;
    }

    void ovtchord_shutdown (OvtChordHandle h) { delete h; }

    void ovtchord_start (OvtChordHandle h) { if (h != nullptr) h->running = true; }
    void ovtchord_stop  (OvtChordHandle h) { if (h != nullptr) h->running = false; }

    void ovtchord_set_callback (OvtChordHandle h, ChordCallback cb, void* userData)
    {
        if (h != nullptr) { h->callback = cb; h->userData = userData; }
    }

    namespace
    {
        void runDetection (OvtChordImpl* ctx, double timestamp)
        {
            const int lowest = ctx->tracker.lowestNote();
            const int bass = (lowest >= 0) ? lowest % 12 : -1;
            ctx->lastResult = ctx->engine.recognize (ctx->tracker.pitchClassSet(), bass, timestamp);
            if (ctx->callback != nullptr && ctx->lastResult.valid)
                ctx->callback (ctx->lastResult, ctx->userData);
        }
    }

    void ovtchord_process_midi (OvtChordHandle h, const uint8_t* data, std::size_t size, double timestampMs)
    {
        if (h == nullptr || ! h->running) return;
        const auto events = h->parser.parse (data, size, timestampMs);
        for (const auto& e : events) h->tracker.process (e);
        runDetection (h, timestampMs);
    }

    void ovtchord_process_midi_events (OvtChordHandle h, const MidiEvent* events, std::size_t count)
    {
        if (h == nullptr || ! h->running) return;
        for (std::size_t i = 0; i < count; ++i) h->tracker.process (events[i]);
        const double ts = (count > 0) ? events[count - 1].timestamp : 0.0;
        runDetection (h, ts);
    }

    ChordResult ovtchord_get_result (OvtChordHandle h)
    {
        return (h != nullptr) ? h->lastResult : ChordResult();
    }

    void ovtchord_process_audio (OvtChordHandle h, const float* samples,
                                 std::size_t numSamples, double sampleRate, double timestampMs)
    {
        if (h == nullptr || ! h->running || samples == nullptr || numSamples == 0)
            return;
        h->audio.setSampleRate (sampleRate);
        if (h->audio.process (samples, static_cast<int> (numSamples)))
        {
            h->lastResult = h->engine.recognize (h->audio.lastPitchClasses(), -1, timestampMs);
            if (h->callback != nullptr && h->lastResult.valid)
                h->callback (h->lastResult, h->userData);
        }
    }

    namespace
    {
        // Feed a mono float buffer to the audio processor in chunks and run
        // detection, storing the last result.
        void feedMono (OvtChordImpl* ctx, const float* mono, std::size_t n, int sampleRate)
        {
            ctx->audio.setSampleRate (sampleRate);
            const std::size_t chunk = 1024;
            for (std::size_t i = 0; i < n; i += chunk)
            {
                const std::size_t c = std::min (chunk, n - i);
                const double ts = static_cast<double> (i) / static_cast<double> (sampleRate) * 1000.0;
                ovtchord_process_audio (ctx, mono + i, c, sampleRate, ts);
            }
        }
    }

    void ovtchord_process_audio_file (OvtChordHandle h, const char* path)
    {
        if (h == nullptr || path == nullptr)
            return;

        const std::string p (path);
        const std::size_t dot = p.find_last_of ('.');
        const bool isMp3 = (dot != std::string::npos)
                           && (p.compare (dot + 1, 3, "mp3") == 0 || p.compare (dot + 1, 3, "MP3") == 0);

        if (isMp3)
        {
            Mp3Data mp3;
            if (readMp3File (p, mp3) && ! mp3.mono.empty())
                feedMono (h, mp3.mono.data(), mp3.mono.size(), mp3.sampleRate);
        }
        else
        {
            WavData wav;
            if (readWavFile (p, wav) && ! wav.mono.empty())
                feedMono (h, wav.mono.data(), wav.mono.size(), wav.sampleRate);
        }
    }
}
