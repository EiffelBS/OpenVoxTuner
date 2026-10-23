// PluginProcessorInternal.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// What the processor translation units share: OVT_LOG (with the note that
// explains why it is gated to Debug) and the parameter-layout factory the
// constructor calls. OVT_LOG cannot stay a per-TU macro copy: the state, ARA
// and parameter units all log, and a divergent copy would silently change
// what Release logs.
#pragma once

#include "PluginProcessor.h"

// IMPORTANT: OVT_LOG is wrapped in #if JUCE_DEBUG to match the convention
// used in PitchShifter.cpp. Reason: in Release builds, juce::Logger::writeToLog
// still runs and allocates a juce::String per call (even though
// BufferedFileLogger amortises the disk I/O), and at one of the call sites
// (processBlock line 2091, "MIDI: f0_out=...") the log fires EVERY audio
// block (~100 calls/sec while singing). On real-time-constrained DAWs like
// Studio One, this string allocation + lock contention is enough to push
// the 11.6 ms block deadline (~512 samples at 44.1 kHz) and cause audible
// dropouts - the user reported this regression on 2026-07-17. Keeping the
// log gated to Debug means Release is free of any string work in the hot
// path. To re-enable logging in a Release build for diagnosis, define
// OVT_FORCE_LOG in the build flags (e.g. cmake -DCMAKE_CXX_FLAGS="-DOVT_FORCE_LOG").
#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
 #define OVT_LOG(msg) juce::Logger::writeToLog (msg)
#else
 #define OVT_LOG(msg) do { } while (false)
#endif

// Built in PluginParameters.cpp: the parameter table is 412 lines of data, and the
// constructor that consumes it is the only caller. Declared here so that one
// definition serves the constructor instead of a copy inside it.
//
// The standalone flag is the table's only input: `midi_out_enable` defaults to
// false when the plugin runs in the standalone wrapper, a fact only the instance
// knows (isStandaloneWrapper() in PluginProcessor.h). The constructor therefore
// computes it and passes it in, at the same point in construction where the table
// used to call the accessor itself.
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (bool isStandalone);
