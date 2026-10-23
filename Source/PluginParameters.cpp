// PluginParameters.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// The plugin's parameter table: every parameter the host can automate, in the
// order the APVTS declares them. Moved out of the constructor (step 4 of the
// source split); the entries below are the original initializer list verbatim,
// returned as a ParameterLayout the constructor hands to the APVTS.
//
// `isStandalone` is the one value read from the processor instance by the old
// in-constructor table (isStandaloneWrapper()); the constructor passes it in, so
// the table itself needs no instance.
#include "PluginProcessorInternal.h"

#include "dsp/ReverbTuning.h"   // the two room tunings and the "ready to sing" send defaults

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (bool isStandalone)
{
    return {
                      // Speed: retargeting time in milliseconds (0-200 ms)
                      std::make_unique<juce::AudioParameterFloat> (
                          "speed", "Speed",
                          juce::NormalisableRange<float> (0.0f, 200.0f, 1.0f),
                          20.0f),
                      std::make_unique<juce::AudioParameterChoice> (
                          "latency_mode", "Latency Mode",
                          juce::StringArray { "Direct Monitoring", "Low Latency", "Quality", "Safe" }, 0),

                      // Amount: correction intensity (0-100%)
                      std::make_unique<juce::AudioParameterFloat> (
                          "amount", "Amount",
                          juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
                          1.0f),

                      // Formant Shift: formant shift in semitones (-12 to +12)
                      std::make_unique<juce::AudioParameterFloat> (
                            "formant", "Formant Shift",
                            juce::NormalisableRange<float> (-5.0f, 5.0f, 0.1f), 0.0f),

                        // Doubler amount (0..100 %). Stereo doubling of the CORRECTED
                        // lead: detune 0 -> 10 cents, Haas time offset 0 -> 25 ms,
                        // L/R spread opposite to the lead. Independent of
                        // harmony_enable — the doubler works on the dry voice alone.
                        // At 0 % the copy is skipped entirely (no pitch-shifter call)
                        // and the output is bit-identical, so old presets are unchanged.
                        std::make_unique<juce::AudioParameterFloat> (
                           "double_amount", "Double Amount",
                           juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f),

                       // Doubler power switch, same shape as the sibling effects'
                       // enable flags (lead_reverb_enable, noise_gate_enable, ...): the
                       // amount keeps its value when switched off, the knob is
                       // greyed and the copy is simply not applied.
                       std::make_unique<juce::AudioParameterBool> (
                           "double_enable", "Double Enable", false),

                      // Formant Shift Enable
                      std::make_unique<juce::AudioParameterBool> (
                          "formant_enable", "Formant Enable", false),

                      // Formant Preservation Mode: Legacy (single peaking EQ) vs MultiFormant (F1-F4)
                      // Default: MultiFormant (index 1) for best quality; Legacy available for CPU savings
                      std::make_unique<juce::AudioParameterChoice> (
                          "formant_mode", "Formant Mode",
                          juce::StringArray { "Legacy", "MultiFormant" }, 1),

                      // Formant Preservation Strategy: selects the formant-preservation
                      // method for the lead and harmony voices.
                      //   0 = Current : partial 1/sqrt(r) pre-warp, fixed male centers.
                      //   1 = P0      : full 1/r pre-warp + voice-type-aware centers.
                      //   2 = P1      : LPC cross-synthesis (post-PSOLA), creative re-applied.
                      //   3 = P2      : peaking-EQ MultiFormant + fast smoothing (reactive).
                      //   4 = P3      : allpass-cascade formant shifting (transparent, phase-only).
                      std::make_unique<juce::AudioParameterChoice> (
                          "formant_strategy", "Formant Strategy",
                          juce::StringArray { "Subtle", "Balanced", "Marked", "Reactive", "Precise" }, 4),

                      // Key: tonic index (0=C, 1=C#, ..., 11=B)
                      std::make_unique<juce::AudioParameterInt> (
                          "key", "Key", 0, 11, 0),

                      // Scale: mode index
                      std::make_unique<juce::AudioParameterChoice> (
                          "scale", "Scale", juce::StringArray {
                              "Chromatic", "Major", "Melodic Minor", "Harmonic Minor", "Natural Minor", 
                              "Major Pentatonic", "Minor Pentatonic", "Blues", "Dorian", "Phrygian", 
                              "Lydian", "Mixolydian", "Locrian", "Custom"
                          }, 0),

                      // Key/Scale detection master switch. When off, the user drives
                      // key/scale manually; when on, the source below detects it. This
                      // replaces the old "Manual" choice of the key_source parameter.
                      std::make_unique<juce::AudioParameterBool> (
                          "key_detect", "Key/Scale Detection", false),

                      // Key detection source (used only when key_detect is on):
                      // 0=Auto (in-plugin audio analysis),
                      // 1=OpenVoxKey (shared bridge from the OpenVoxKey companion detector),
                      // 2=Sidechain (analysis of the sidechain input).
                      std::make_unique<juce::AudioParameterChoice> (
                          "key_source", "Key Source",
                          juce::StringArray { "Auto", "OpenVoxKey", "Sidechain" }, 0),

                      // Chord detection master switch (ovtchord). When on, the
                      // plugin detects the current chord in real time from the
                      // MIDI input ("Tuning follows MIDI IN") and/or the sidechain
                      // bus, and feeds it to the ScaleQuantizer chord override
                      // (priority: MIDI > ARA > Sidechain > scale).
                      std::make_unique<juce::AudioParameterBool> (
                          "chord_detect_enable", "Chord Detection", false),

                      // Companion group letter (must match the OpenVoxKey instance).
                      std::make_unique<juce::AudioParameterChoice> (
                          "companion_group", "Companion Group",
                          juce::StringArray { "A", "B", "C", "D" }, 0),

                      // 12 booleans for the custom scale.
                      // Only active when Scale = Custom.
                      // Default: C major -> {C, D, E, F, G, A, B}.
                      std::make_unique<juce::AudioParameterBool> ("custom0",  "Custom C",  true),
                      std::make_unique<juce::AudioParameterBool> ("custom1",  "Custom C#", false),
                      std::make_unique<juce::AudioParameterBool> ("custom2",  "Custom D",  true),
                      std::make_unique<juce::AudioParameterBool> ("custom3",  "Custom D#", false),
                      std::make_unique<juce::AudioParameterBool> ("custom4",  "Custom E",  true),
                      std::make_unique<juce::AudioParameterBool> ("custom5",  "Custom F",  true),
                      std::make_unique<juce::AudioParameterBool> ("custom6",  "Custom F#", false),
                      std::make_unique<juce::AudioParameterBool> ("custom7",  "Custom G",  true),
                      std::make_unique<juce::AudioParameterBool> ("custom8",  "Custom G#", false),
                      std::make_unique<juce::AudioParameterBool> ("custom9",  "Custom A",  true),
                      std::make_unique<juce::AudioParameterBool> ("custom10", "Custom A#", false),
                      std::make_unique<juce::AudioParameterBool> ("custom11", "Custom B",  true),

                      // Bypass: skip all processing
                      std::make_unique<juce::AudioParameterBool> (
                          "bypass", "Bypass", false),

                      // Mode: Live (0) or Curve Editor (1)
                      std::make_unique<juce::AudioParameterChoice> (
                          "mode", "Mode", juce::StringArray { "Live", "Curve Editor" }, 0),

                      // Harmony Type (vocale) : None, presets...  default = ThirdBelowAbove (index 3)
                      std::make_unique<juce::AudioParameterChoice> (
                          "harmony_type", "Harmony Type",
                          juce::StringArray {
                              "None",
                              "3rd Below", "3rd Above", "3rd Below + Above",
                              "4th Below", "4th Above", "4th Below + Above",
                              "5th Below", "5th Above", "5th Below + Above",
                              "3rd Below + 5th Above", "5th Below + 3rd Above",
                              "Octave Below", "Octave Above", "Octave Below + Above",
                              "Vocal Stack (3 voices)", "Vocal Stack (4 voices)",
                              "Power Chord", "Parallel 3rd", "Drone",
                              "Unison (2 voices)", "Unison + Octaves (4 voices)"
                          }, 3),

                      // Harmony Enable : master on/off - disabled by default
                      std::make_unique<juce::AudioParameterBool> (
                          "harmony_enable", "Harmony Enable", false),

                      // Harmony Gain: harmony volume level - 1.0 by default
                      std::make_unique<juce::AudioParameterFloat> (
                          "harmony_gain", "Harmony Volume",
                          juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.75f),

                      // Harmony Blend: mix of lead voice / harmonies - 0.5 by default
                      std::make_unique<juce::AudioParameterFloat> (
                          "harmony_blend", "Harmony Blend",
                          juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f)
                      ,
                      // Harmony Attack : per-voice fade-in duration (ms) applied to each
                      // harmony voice's attack. Smoother / more progressive than a hard
                      // onset. When the Noise Gate is enabled the effective attack is
                      // clamped to a short gate-follow time so the harmony swells with the
                      // gated dry signal. 35 ms by default (matches the previous internal
                      // attack). Range 1..300 ms.
                      std::make_unique<juce::AudioParameterFloat> (
                          "harmony_attack", "Harmony Attack (ms)",
                          juce::NormalisableRange<float> (1.0f, 300.0f, 1.0f), 35.0f)
                      ,
                      // Use Voice : enabled by default
                      std::make_unique<juce::AudioParameterBool> (
                          "harmony_use_voice", "Use Voice for Harmony", true),
                      // Shifted voices : 4 by default
                      std::make_unique<juce::AudioParameterInt> (
                          "harmony_shifted_voices", "Shifted Voices", 1, 4, 4),
                      std::make_unique<juce::AudioParameterChoice> (
                          "harmony_tone", "Harmony Tone",
                          juce::StringArray { "Choir", "Organ" }, 0),
                      std::make_unique<juce::AudioParameterFloat> (
                          "harmony_tone_color", "Harmony Tone Color",
                          juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f),
                       // When on (default), the harmony voices follow the lead voice's correction
                       // character (vibrato preservation, humanize, flex, attack-aware), so they move
                       // with the lead instead of staying locked to the scale grid.
                       std::make_unique<juce::AudioParameterBool> (
                          "harmony_follow_lead", "Harmony Follow Lead", true),
                       // Gain Match: when on (default), the harmony mix is scaled by
                       // 1/sqrt(1+N) where N = number of active harmony voices, so the
                       // total output RMS is roughly equal to the dry input RMS (this
                       // matches the perceptual balance for uncorrelated harmony voices
                       // and is a good compromise for correlated Unison/Unison+Octaves
                       // which the user reports as "much louder"). The dry signal is
                       // untouched. The harmony_volume knob still controls the overall
                       // harmony level (post-compensation). User can turn this off to
                       // restore the natural "additive" boost.
                       std::make_unique<juce::AudioParameterBool> (
                          "harmony_gain_match", "Harmony Gain Match", true),

                       // Harmony Formant Shift : formant shift for harmony voices only (-5 to +5 st)
                       std::make_unique<juce::AudioParameterFloat> (
                          "harmony_formant", "Harmony Formant Shift",
                          juce::NormalisableRange<float> (-5.0f, 5.0f, 0.1f), 0.0f),

                       std::make_unique<juce::AudioParameterBool> (
                          "midi_out_enable", "MIDI Out Enable",
                          // In standalone mode, disable MIDI out by default.
                          ! isStandalone)
                      , std::make_unique<juce::AudioParameterBool> (
                          "midi_target_enable", "MIDI Target Enable", false)
                      , std::make_unique<juce::AudioParameterBool> (
                          "dbg_test_grain", "Debug Test Grain", false)
                      , std::make_unique<juce::AudioParameterInt> (
                            "editor_measures", "Editor Measures", 1, 32, 4)
                      , std::make_unique<juce::AudioParameterBool> (
                            "editor_playhead_loop", "Editor Playhead Loop", false)
                      , std::make_unique<juce::AudioParameterBool> (
                            "auto_scroll", "Auto Scroll", true)
                      , std::make_unique<juce::AudioParameterChoice> (
                            "pitch_detector", "Pitch Detector",
                            juce::StringArray { "YIN", "Reserved" }, 0)
                      // Voice Type: constrains pitch detector search range to reduce
                      // octave errors and improve detection speed. Default = Universal
                      // (full 30-1000 Hz range). Other options narrow the range to
                      // typical vocal registers.
                      , std::make_unique<juce::AudioParameterChoice> (
                            "voice_type", "Voice Type",
                            juce::StringArray { "Universal", "Bass", "Baritone", "Tenor", "Alto", "Soprano" }, 0)
                      , std::make_unique<juce::AudioParameterBool> (
                            juce::ParameterID { "noise_gate_enable", 1 }, "Noise Gate", false)
                      , std::make_unique<juce::AudioParameterFloat> (
                            juce::ParameterID { "noise_gate_threshold", 1 }, "Gate Threshold",
                            juce::NormalisableRange<float> (-80.0f, 0.0f, 1.0f), -50.0f)
                      // Upward compression: lifts quiet passages before tuning.
                      // Single knob = amount (0..1); pivot follows the signal RMS.
                      , std::make_unique<juce::AudioParameterBool> (
                            "upward_comp_enable", "Upward Comp Enable", false)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "upward_comp_amount", "Upward Comp Amount",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f)
                      // === Shared reverb buses: ROOM + HALL ===
                      // Two rooms shared by both voices, the way a vocal is sent to two
                      // reverb buses in a DAW. Each voice has its own on/off switch and its
                      // own amount to each room; there is no separate "send" control, so the
                      // knob IS the send and the dry voice always stays at unity.
                      //
                      // The switches start OFF, so a fresh session is dry. The knobs start at
                      // the "ready to sing" values from ReverbTuning.h: switching the Lead
                      // switch ON applies a balanced short-room + hall pair immediately,
                      // which is the single-knob experience the legacy Mix knob gave.
                      , std::make_unique<juce::AudioParameterBool> (
                            "lead_reverb_enable", "Lead Reverb", false)
                      , std::make_unique<juce::AudioParameterBool> (
                            "harmony_reverb_enable", "Harmony Reverb", false)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "lead_room_send", "Lead Room",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), ovtdsp::kDefaultLeadRoomSend)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "lead_hall_send", "Lead Hall",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), ovtdsp::kDefaultLeadHallSend)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "harmony_room_send", "Harmony Room",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), ovtdsp::kDefaultHarmonyRoomSend)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "harmony_hall_send", "Harmony Hall",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), ovtdsp::kDefaultHarmonyHallSend)
                      // Sidechain ducker on the rooms' returns. The key is the CORRECTED LEAD,
                      // never the noise gate: the rooms dip while the lead sings so a long hall
                      // cannot mask the consonants, and bloom back between phrases. Depth 0 = no
                      // ducking, 1 = up to -12 dB while the lead sings at its reference level.
                      , std::make_unique<juce::AudioParameterFloat> (
                            "reverb_duck_depth", "Reverb Ducking",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), ovtdsp::kDefaultReverbDuckDepth)
                        // Master output stage. The trim is a plain dB gain (-12..+12), so
                        // setting the plugin level never touches the harmony gain-match;
                        // the limiter detects true peaks through a 4x reconstruction filter, so it
                        // looks ahead by its group delay (8 samples) and that latency IS declared to
                        // the host, on top of the pitch shifter's.
                        , std::make_unique<juce::AudioParameterFloat> (
                              "output_trim", "Output Trim",
                              juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f)
                        , std::make_unique<juce::AudioParameterBool> (
                              "limiter_enable", "Limiter Enable", true)
                        // Soft saturation on the master output, after every voice has been
                        // blended and BEFORE the shared reverb return: the tail stays clean,
                        // only the voice is driven. Drive 0 is a transparent bypass -- the
                        // shaper curve tanh(k*x)/tanh(k) is NOT unity at k = 1, so the stage
                        // skips its whole pass instead of evaluating it. Loudness is matched
                        // by a measured makeup table (the shaper loses up to 2.6 dB RMS at
                        // full drive), never by an estimate, and the stage runs at 2x with a
                        // half-band filter so harmonics above fs/2 cannot fold back. Warmth
                        // moves the warm pole between 8 kHz (bright) and 5 kHz (warm). The
                        // default is OFF: existing projects must not change sound on load.
                        , std::make_unique<juce::AudioParameterBool> (
                              "saturation_enable", "Saturation Enable", false)
                        , std::make_unique<juce::AudioParameterFloat> (
                              "saturation_drive", "Saturation Drive",
                              juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.35f)
                        , std::make_unique<juce::AudioParameterFloat> (
                              "saturation_tone", "Saturation Warmth",
                              juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.40f)
                       // De-esser: tames the sibilance of the corrected lead. One knob
                       // (Intensity, 0..1) drives the detector threshold and the depth
                       // cap together (-38 x amount dB, 19 x amount dB), the way a
                       // hardware de-esser's Intensity knob does. The band centre is
                       // NOT a parameter: it comes from voice_type, the register the
                       // user already sets for pitch detection, so a low voice is
                       // de-essed around 5 kHz and a soprano around 8.4 kHz. The
                       // detector listens ONLY to that band (2-pole high-pass + 2-pole
                       // low-pass), so plosives and low vowels cannot trigger it; the
                       // reduction is applied broadband. Same shape as the sibling
                       // effects: the knob keeps its value when the switch is off.
                       , std::make_unique<juce::AudioParameterBool> (
                             "deess_enable", "De-Esser Enable", false)
                       , std::make_unique<juce::AudioParameterFloat> (
                             "deess_amount", "De-Esser Intensity",
                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.65f)
                       // Tempo-synced ping-pong delay on the corrected lead voice, added after the de-esser in
                       // the effects bus. The division knob is a fraction OF A BEAT, ordered by repetition
                       // length (shortest first): {"1/8T","1/8","1/4T","1/8D","1/4","1/2T","1/4D","1/2","1/2D"}
                       // map to {1/3, 0.5, 2/3, 0.75, 1.0, 4/3, 1.5, 2.0, 3.0} beats, so the copy length is
                       // (60/bpm) x fraction -- with ARA metadata the delay follows the host tempo exactly,
                       // without ARA it follows the plugin's own BPM register. "D" is a dotted note, "T" a
                       // triplet; every plain value has both siblings, and the table is sorted by duration so the
                       // knob walks from the shortest slap to the longest repeat. The default "1/4" sits at
                       // index 4 (the state stores the choice INDEX, so the ordering is part of the mapping).
                       // The short end stops at 1/8T on purpose: the DSP's comb floor is 40 ms, and 1/8T is the
                       // shortest value that stays above it at every legal tempo (50 ms at 400 BPM), so no
                       // labelled division is ever silently clamped. Two lines cross-feed each other (the L tail
                       // lands in the R line),
                       // which alternates echoes left/right; the feedback loop carries a 150 Hz one-pole low-cut so a
                       // long ring cannot build sub-bass mud or DC. Mix is the wet of the echo and the dry voice stays
                       // at unity: this stage ADDS, it never steals the lead (see IEffect.h). No latency is declared for
                       // it -- an echo does not put the voice behind itself, same rule as Saturation declaring nothing
                       // for its FIR group delay. Default OFF and defaults 0.30/0.35/1-4: loading an old project on a
                       // new build changes nothing audible, exactly the de-esser and Saturation precedent.
                        , std::make_unique<juce::AudioParameterBool> (
                              "delay_enable", "Delay Enable", false)
                        , std::make_unique<juce::AudioParameterChoice> (
                              "delay_division", "Delay Division",
                              juce::StringArray { "1/8T", "1/8", "1/4T", "1/8D", "1/4",
                                                  "1/2T", "1/4D", "1/2", "1/2D" }, 4)
                        , std::make_unique<juce::AudioParameterFloat> (
                              "delay_mix", "Delay Mix",
                              juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.30f)
                        , std::make_unique<juce::AudioParameterFloat> (
                              "delay_feedback", "Delay Feedback",
                              juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.35f)
                      // 2026-07-24 (Deprecation): FlexTune and Attack-Aware
                      // APVTS parameters are KEPT for preset compatibility
                      // (so existing presets don't lose their values) but
                      // their default values are forced to "off/0" and the
                      // corresponding DSP code in processBlock is disabled.
                      // Future re-implementation can re-enable them by
                      // changing the defaults back to their previous values
                      // and re-enabling the DSP code in processBlock.
                      // ----------------------------------------------------------------
                      // FlexTune: deadband around the target note (0-100 cents)
                      // DEPRECATED: default forced to 0 (no deadband). See above.
                      , std::make_unique<juce::AudioParameterFloat> (
                            "flex_tune", "FlexTune",
                            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f)
                      // Humanize: random pitch fluctuations (0-50 cents)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "humanize", "Humanize",
                            juce::NormalisableRange<float> (0.0f, 50.0f, 1.0f), 10.0f)
                      // Correction mode: Modern (false) / Transparent (true)
                      , std::make_unique<juce::AudioParameterBool> (
                            "correction_mode", "Correction Mode", true)
                      // Hard Tune: the "Auto-Tune as an instrument" mode. It bypasses
                      // the 80 ms anti-jitter speed floor, the pitch shifter's
                      // ratio-jump duck (the pre-break swell on every semitone) and
                      // the formant preserver, and it analyses pitch with the short
                      // window (2048/512, 3-frame median) so a note change is
                      // reported in ~56 ms instead of ~218 ms. It is a separate mode
                      // rather than a Speed value because it changes the ENGINE
                      // path, not a time constant. Default off: every existing
                      // session and every preset that does not name it keeps the
                      // current behaviour.
                      , std::make_unique<juce::AudioParameterBool> (
                            "hard_tune", "Hard Tune", false)
                      // Vibrato preservation: 0% = classic instantaneous correction,
                      // 100% = correction against the smoothed center pitch so the
                      // vibrato modulation survives.
                      , std::make_unique<juce::AudioParameterFloat> (
                            "vibrato_preserve", "Vibrato Preserve",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.20f)
                      // Attack-aware correction: enable + release time (ms). When
                      // enabled, the correction is eased off on note onsets/transients.
                      // DEPRECATED: default forced to false (disabled). See above.
                      , std::make_unique<juce::AudioParameterBool> (
                            "attack_aware", "Attack-Aware Correction", false)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "attack_release", "Attack Release",
                            juce::NormalisableRange<float> (10.0f, 300.0f, 1.0f), 60.0f)
                      // UI Theme: 0 = Dark (default), 1 = Light
                      , std::make_unique<juce::AudioParameterInt> (
                            "ui_theme", "UI Theme", 0, 1, 0)
                      // UI Language: 0 = English (default), 1 = French, 2 = German, 3 = Spanish, 4 = Japanese, 5 = Chinese
                      , std::make_unique<juce::AudioParameterInt> (
                            "ui_language", "UI Language", 0, 5, 0)
                      // UI waveform: show/hide overlay (0 = hidden, 1 = shown, default 1)
                      , std::make_unique<juce::AudioParameterInt> (
                            "ui_show_waveform", "Show Waveform", 0, 1, 1)
                      // UI waveform display type: 0 = Line, 1 = Mirror, 2 = Spectral (default)
                      , std::make_unique<juce::AudioParameterInt> (
                            "ui_waveform_type", "Waveform Display Type", 0, 2, 2)
                      // UI auto-center pitch display (0 = off, 1 = on, default 0)
                      , std::make_unique<juce::AudioParameterInt> (
                            "ui_auto_center", "Auto-Center Pitch", 0, 1, 0)
                      // Visualizer zoom range (Hz, log scale)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "viz_fmin", "Visualizer Min Hz",
                            juce::NormalisableRange<float> (16.0f, 2000.0f, 0.1f), 50.0f)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "viz_fmax", "Visualizer Max Hz",
                            juce::NormalisableRange<float> (100.0f, 8372.0f, 0.1f), 1500.0f)
                      // Morph: A/B crossfade position (0 = source slot A, 1 = target slot B).
                      // Made a host-automatable parameter so the DAW can automate the morph.
                      , std::make_unique<juce::AudioParameterFloat> (
                            "morph_amount", "Morph",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f)
    };
}
