# DSP API Reference

All DSP modules live in the **`ovtdsp::`** namespace (chosen instead of `dsp`
to avoid ambiguity with `juce::dsp`). This page documents the real public
classes/methods as found in the headers under `Source/dsp/`.

## Pitch detection

### `ovtdsp::IPitchDetector` (interface)

| Member | Description |
|--------|-------------|
| `virtual void prepare(double sampleRate, int blockSize)` | Initialize with sample rate and block size. |
| `virtual void reset()` | Clear internal state. |
| `virtual float detectPitch(const float* samples, int numSamples)` | Return fundamental frequency in Hz, or `0.0` if unvoiced. |
| `virtual void setThreshold(float t)` | Detection sensitivity (`0.01–0.99`; lower = stricter). |
| `virtual float getThreshold() const` | Current threshold. |
| `virtual juce::String getName() const` | Human-readable algorithm name. |

### `ovtdsp::YinPitchDetector : public IPitchDetector` (active)

YIN implementation (de Cheveigne & Kawahara). Extras over the interface:

- `void setFrequencyRange(float minHz, float maxHz)` — narrows the search range
  (used by the Voice Type selector). Safe to call from the audio thread.

## Quantization

### `ovtdsp::ScaleQuantizer`

| Member | Description |
|--------|-------------|
| `void setKey(int keyInSemitones)` | Set tonic (0=C … 11=B). |
| `void setScale(ovtdsp::Scale scale)` | Set mode/scale (Chromatic=0 … Custom=13). |
| `void setCustomIntervals(const juce::Array<int>&)` | Set the 12 custom notes (no key offset); ignored unless `Scale::Custom`. |
| `float quantize(float hzIn) const` | Return the nearest in-scale Hz, or `hzIn` if none. |
| `const juce::Array<int>& getScaleIntervals() const` | Active scale notes (semitones 0..11 relative to C). |
| `bool isCustom() const` | Whether `Scale::Custom` is active. |

Free helpers:

- `float hzToSemitones(float hz)` / `float semitonesToHz(float semi)` — A4 = 440 Hz.

### `ovtdsp::Scale` (enum)

`Chromatic=0, Major, MelodicMinor, HarmonicMinor, NaturalMinor, MajorPentatonic,
MinorPentatonic, Blues, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Custom=13`.

## Pitch shifting

### `ovtdsp::IPitchShifter` (interface)

| Member | Description |
|--------|-------------|
| `virtual void prepare(double sampleRate, int maximumBlockSize)` | Initialize. |
| `virtual void reset()` | Flush internal state (FIFOs). |
| `virtual void process(juce::AudioBuffer<float>&, float ratio, float f0)` | Process in place (`ratio` = transposition, `f0` needed by PSOLA). |
| `virtual int getLatencySamples() const` | Internal latency in samples. |

### `ovtdsp::PitchShifter : public IPitchShifter` (PSOLA)

PSOLA is the **only** pitch shifter. Public members:

| Member | Description |
|--------|-------------|
| `void process(buffer, float pitchRatio, float formantRatio, float f0)` | In-place process with a distinct formant ratio. |
| `void process(input, output, float pitchRatio, float formantRatio, float f0)` | Out-of-place variant. |
| `void setLatencyMs(float)` | Set latency, clamped to 8–40 ms. |
| `int getLatencySamples() const` | Current latency in samples. |
| `void setAttackTimeMs(float)` | Voice-onset attack time (default 30 ms). |
| `void setAttackEnvelopeEnabled(bool)` | Enable/disable internal onset fade (coordinate with an external helper). |
| `void setExternalAttackGain(float gain, float blockDurSec)` | External block-level attack-gain driver (negative = disable). |
| `void setExternalAttackTauSeconds(float)` | TC of the external attack smoother (default 0.015 s). |
| `void resetExternalAttackGain()` | Snap smoother to 1.0 and disable external driver. |
| `void resetSoft()` | Reset state without re-arming the startup fade-in (session seek/preset). |
| `void forceCreateTestGrain()` | Debug hook to force a single test grain. |

## Speed / retarget smoothing

### `ovtdsp::RetargetEnvelope`

| Member | Description |
|--------|-------------|
| `void prepare(double sampleRate)` | Initialize. |
| `void reset()` | Reset state. |
| `void setSpeed(float ms)` | Retarget time in milliseconds. |
| `float processSample(float targetRatio)` | Smooth a single sample of ratio. |
| `float processBlock(float targetRatio, int numSamples)` | **Buffer-size-independent** block smoothing (recommended). |

## Formant preservation

### `ovtdsp::FormantPreserver`

| Member | Description |
|--------|-------------|
| `enum class Mode` | `Legacy`, `MultiFormant` (F1–F4), `Allpass` (F1–F4 allpass cascade). |
| `enum class Strategy` | `Current` (partial `1/sqrt(r)`, male-default centers), `P0` (full `1/r`, voice-type-aware centers). |
| `void prepare(double, int)` / `void reset()` | Lifecycle. |
| `void process(juce::AudioBuffer<float>&, float ratio)` | Apply formant shift (must run before PSOLA). |
| `setEnabled/isEnabled`, `setFormantShift(float semitones)` | Enable + manual shift in semitones. |
| `setMode/getMode`, `setStrategy/getStrategy` | Mode/strategy selectors. |
| `setQMultiplier(float)`, `setSmoothingAlpha(float)` | Resonance Q and biquad-coefficient smoothing. |
| `setVoiceType(int)` | Voice type (0=Universal…5=Soprano) for P0 centers. |
| `setFormant(int index, float freqHz, float q, float gainDb)` | Configure a specific formant (MultiFormant only). |

### `ovtdsp::LpcFormantPreserver` (P1 / P2)

LPC cross-synthesis formant preservation (the "exact" approach).

| Member | Description |
|--------|-------------|
| `enum class Mode` | `C0` (P1: plain LPC cross-synthesis), `C1Hybrid` (P2: pre-emphasis + LPC interpolation + hybrid fallback). |
| `explicit LpcFormantPreserver(int order = 18)` | Construct with LPC order. |
| `void prepare(double, int)` / `void reset()` | Lifecycle. |
| `void setOrder(int)` | Change the LPC order. |
| `void process(juce::AudioBuffer<float>& out, const juce::AudioBuffer<float>& reference, float ratio, Mode)` | Re-color `out` (post-shift) using `reference` (pre-shift) formants, in place. |

## Harmony

### `ovtdsp::HarmonyEngine`

| Member | Description |
|--------|-------------|
| `enum class HarmonyType` | `None` + 21 types (`ThirdBelow`, `ThirdAbove`, `ThirdBelowAbove`, `Fourth*`, `Fifth*`, `Octave*`, `VocalStack3/4`, `PowerChord`, `ParallelThird`, `Drone`, `Unison2`, `UnisonOctaves4`). |
| `void prepare(double sampleRate)` | Store sample rate for oscillator increments. |
| `juce::Array<float> getHarmonyNotes(float baseFreq, const juce::Array<int>& scaleIntervals, HarmonyType) const` | Compute per-voice frequencies. |
| `int getScaleDegree(float freq, int key, const juce::Array<int>&) const` | Degree of a frequency within the scale. |
| `void renderHarmonies(inputFreq, harmonyFrequencies, volume, sampleRate, outputBuffer, key, scaleIndex, blend, toneMode, toneColor, gateActive)` | Synthesize voices into the buffer. |
| `static juce::String getHarmonyName(HarmonyType)` | Display name. |
| `static int getHarmonyVoiceCount(HarmonyType)` | Number of generated voices. |
| `void setVoiceGate(bool)` | Voice gating (note on/off). |
| `void setEnvelopeTimes(float attackMs, float releaseMs)` | Per-voice attack/release. |
| `bool isActive() const` | Whether any voice is sounding/releasing. |

## Effects

### `ovtdsp::IEffect` (interface)

| Member | Description |
|--------|-------------|
| `virtual juce::String getId() const` | Unique id (e.g. `"reverb"`). |
| `virtual juce::String getName() const` | Human-readable name (e.g. `"Reverb"`). |
| `virtual void prepare(double sampleRate, int maximumBlockSize)` | Prepare. |
| `virtual void reset()` | Flush delays/reverb tails. |
| `virtual void process(juce::AudioBuffer<float>& buffer, bool enabled, float wetMix)` | Apply in place. `wetMix` is a **dry/wet crossfade**: mix 0 leaves the input bit-identical, mix 1 replaces it with the effect's output. A knob wired to this argument must be a crossfade — an additive wet ("dry + wet * mix") never reaches fully wet and reads as "the effect is inaudible at full mix". |
| `virtual void processSend (juce::AudioBuffer<float>& sendBuffer, float sendAmount)` | Optional: tap this buffer into the effect's own send line, **in parallel** with the dry path. Defaulted to a no-op, so an insert-only effect needs no change. |
| `virtual void processReturn (juce::AudioBuffer<float>& masterBuffer, float duckGain)` | Optional: add the effect's send tail into the master buffer, scaled by `duckGain`. Defaulted to a no-op. Summed **additively**, so it declares no latency. |

Effects are registered on one of the three tap points (`ovtdsp::EffectsBus`): the lead
voice, the harmony bus, or the master output. Each bus owns its own instances, so the
same class can run twice with different fixed settings; `enabled` and `wetMix` are
computed by the caller's wiring callback from APVTS parameters.

A send is not an insert: the tapped buffer keeps its full dry level and nothing is
attenuated, so a send tap never declares latency. The processor drives sends and returns
directly (the bus wiring callback stays inside `apply`), and the return is summed into the
master buffer before the output trim + limiter.

### `ovtdsp::EffectsBus`

| Member | Description |
|--------|-------------|
| `void addEffect(std::unique_ptr<IEffect>)` | Register an effect on this bus (registration order is kept). |
| `void prepare(double sampleRate, int maxBlockSize)` | Prepare every registered effect. |
| `void reset()` | Reset every registered effect. |
| `void apply(juce::AudioBuffer<float>& buffer, const Wiring& wiring)` | Run every effect in place on the tapped buffer. |
| `int size() const` | Number of registered effects. |

`Wiring` is `std::function<bool (IEffect&, bool& enabled, float& wetMix)>`: it computes
the enable flag and wet mix for one effect (and may configure the instance), returning
`false` to skip it for this block. The plugin owns `leadBus`, `harmonyBus` and `masterBus`.

### `ovtdsp::ReverbEffect : public IEffect`

One reverb room, driven either as a bus insert (`process`) or as a shared send/return room
(`processLeadSend` / `processHarmonySend` / `processReturn`). The processor runs **two
instances**, both in shared-bus mode, tuned from `ReverbTuning.h`:

- `setRoomSize(float)` / `setDamping(float)` / `setStereoWidth(float)` — reverb tuning.
- Wraps `juce::Reverb`; smooths the master enable gain and every send amount over 25 ms to
  avoid clicks, and declares **no latency** (a send adds a tail, it does not delay the voice).
- **Two rooms, four sends.** ROOM (`kReverbRoomTuning`: 0.35 / 0.50 / width 1.0, RT60 ≈
  1.18 s) keeps the lead present; HALL (`kReverbHallTuning`: 0.70 / 0.30 / width 1.0, RT60 ≈
  2.43 s) is the **legacy Main room restored bit for bit** — the sound the single old Mix knob
  delivered. Each room is fed by one lead send and one harmony send, the DAW bus model: a send
  adds to the room and never attenuates the dry path.
  - `processLeadSend (buffer, amount)` — adds the lead to the send line through a one-pole
    low-cut (`ovtdsp::OnePoleLowCut`, 150 Hz) so body low-end does not ring the room.
  - `processHarmonySend (harmony, amount)` — adds the blended harmony, unfiltered.
  - `processReturn (master, duckGain)` — runs the room on the send line and sums the tail into
    the master buffer, scaled by `duckGain * kReturnTrim` (`kReturnTrim = 0.7`, ≈ -3 dB); then
    clears the send line, so each block's sends are consumed exactly once. The trim keeps a
    unity send from sitting at +5 dB over the voice ahead of the trim + limiter, where it would
    be clamped every block and pump.
- `process (buffer, enabled, wetMix)` — the insert form (dry/wet crossfade) kept for the
  `IEffect` contract and still tested; the bus pair above is what the plugin uses.
- Send line and wet scratch are sized in `prepare()`: a send tap allocates nothing in the
  audio callback. A mono source adds its channel to both sides of the send line; a mono
  master averages the stereo return.
- The return skips the reverb pass when the send line is silent and the tail has decayed,
  which is what keeps sleep-mode CPU at ~1 %.

### `ovtdsp::ReverbDucker`

The sidechain ducker that guards both room returns, keyed by the **corrected lead** (taken
before the lead bus effects, so the de-esser and the tempo echoes cannot re-trigger it — the
old gate-linked ducking moved only when the noise gate closed, i.e. never while singing).

- `prepare(double sampleRateHz)` / `reset()` — coefficients and envelope; `reset()` re-arms at
  unity so a transport restart never carries a ducked room from the previous take.
- `processKey(const juce::AudioBuffer<float>&)` — per-sample stereo-linked **peak** envelope,
  attack `kAttackMs = 15`, release `kReleaseMs = 300`. Silent and mono key blocks are handled;
  nothing allocates.
- `processSilence(int numSamples)` — keeps the release running when no key arrives, so a
  stopped transport lets the rooms bloom back instead of freezing a duck.
- `getDuckGain(float depth)` — `[0, 1]`, `depth 0 -> 1.0` (true bypass); the envelope is
  normalised against `kKeyReferencePeak = 0.2` (-14 dBFS), so `depth 1` at or above that peak
  ducks by `kMaxDepthDb = 12`.
- Header-only (`Source/dsp/ReverbDucker.h`), so the processor and the tests share one
  implementation with no extra translation unit.

### `ovtdsp::DelayEffect : public IEffect`

Tempo-synced stereo ping-pong delay, registered as a **lead-bus insert after the De-Esser** (chain order
DeEsser → Delay). The lead send tap runs **after** `leadBus.apply`, so the echoes are fed into both shared rooms
through it — no second room, no extra parameter pair.

- `setBpm(float)` / `setDivision(int index)` / `setMix(float)` / `setFeedback(float)` (clamped ≤ 1.0).
- Division indices 0..4 map to **beat fractions** {0.5, 0.75, 1.0, 1.5, 2.0}, labels {"1/8","1/8.","1/4","1/4.","1/2"};
  `copySamples = round((60/bpm) * fraction * sr)`, so halving the tempo doubles the copy. Clamped to
  [floor = max(1, round(0.04*sr)) = 1920 samples at 48 kHz, 6 s], with lines sized once in `prepare()`: nothing is
  allocated inside the audio callback.
- **In-place additive send, dry path at unity** — not a dry/wet crossfade: `out = in + rampedMix * feedback tail`, so
  full mix does not attenuate the voice and the stage never reads as "inaudible at full mix". The declared latency is
  unchanged (**zero**), `setLatencySamples` is never touched by this stage.
- **Cross-coupling L→R / R→L**: input on L echoes first in R, successive passes alternate sides; a centred source
  spreads because of the cross-feedback, not because of the input.
- Each line's feedback path runs through `OnePoleLowCut` at **150 Hz**, so DC drains inside the loop instead of being
  stored (a pole placed at DC). The wet path has no low-pass filter.
- Copy changes are handed over by a **two-tap crossfade** over `kRampSeconds = 0.025`: the tap being read is frozen
  and the new distance is faded in, so the read pointer never glides. Ramping the pointer itself pitch-shifts the
  tail (measured: a 0.107 worst sample-to-sample jump on a 1/4 → 1/2 change, against 0.0058 with the crossfade).
  The enable gain is one-pole, so toggling power never clicks; when disabled and `bypassGain` has decayed past the
  floor, **the buffer is not touched at all** (`isBypassed()` true) while the lines keep feeding.
- Diagnostics: `getCopySamples()`, `getTargetCopySamples()`, `getLineCapacity()`, `isBypassed()`.

Parameters (ids verbatim): `delay_enable` (bool, default off), `delay_division` (choice index, default 2 = "1/4"),
`delay_mix` (default **0.30**), `delay_feedback` (default **0.35**). Tempo comes from `getAraTempo()` when an ARA source is bound,
and from host / standalone BPM when it is not.

### `ovtdsp::OutputLimiter`

Not an `IEffect`: the last stage of the master bus, applied after every effect.

- `void prepare(double sampleRate, int maxBlockSize)`, `void reset()`.
- `void setTrimDb(float)` — output gain in dB, clamped to `-12 .. +12`.
- `void setLimiterEnabled(bool)` — off leaves a pure trim (no true-peak limiting, no soft clip).
- `void process(juce::AudioBuffer<float>& buffer)` — trim, true-peak limit and soft clip, in place, channels linked.
- `static constexpr float ceilingLinear = 0.891f` (= **-1 dBFS**, read on the reconstructed true peak).
- `static constexpr float kSafetyMargin = 0.975f`, `targetLinear = ceilingLinear * kSafetyMargin` (=-1.22 dBFS):
  the gain closes on this target, because the soft clip shapes the output and adds ringing the input-side
  detector cannot see; closing on the ceiling itself measured true peaks above -1 dBFS.
- `int getLatencySamples()` — the stage's declared lookahead (8 samples, the FIR group delay),
  added to the pitch shifter's latency by `applyLatencyMode()`.
- **True-peak detector**: a 65-tap unnormalized sinc FIR at 4x oversampling; each phase group sums
  to unity gain (partition of unity), so a full-scale bandlimited sine reconstructs at its own amplitude.
  The emitted sample is the phase-0 reconstruction, i.e. the centre tap: the stage is a delay of exactly
  `getLatencySamples()`, and phases 1..3 read samples up to the current input — that is the lookahead.

### `ovtdsp::NoiseGate`

- `void prepare(double sr)`, `setEnabled(bool)`, `setThresholdDb(float)`,
  `bool isEnabled()`, `float getCurrentGain()`, `void process(juce::AudioBuffer<float>&)`.
- RMS-based, sample-accurate, with hysteresis and smooth attack/release.

## Graphic-mode curve

### `ovtdsp::PitchPoint` (struct)

`{ double time; float pitch; }` — time in seconds, pitch in Hz, with
`==`/`!=` operators.

### `ovtdsp::PitchCurve`

Sorted list of `PitchPoint` with linear interpolation between points
(evaluated by binary search, O(log N)).

| Member | Description |
|--------|-------------|
| `int addOrUpdatePoint(double time, float pitch)` | Add or replace nearest point; returns its index. |
| `bool removePointNear(double time, double toleranceSec = 0.05)` | Remove nearest point within tolerance. |
| `void clear()` / `int getNumPoints()` | Reset / count. |
| `const PitchPoint& getPoint(int) const` / `PitchPoint& getPoint(int)` | Direct access. |
| `void setPointPitch(int, float)` / `setPointTimeAndPitch(int&, double, float)` / `setMultiplePointsTimeAndPitch(...)` | Editing helpers (keep time-sorted). |
| `float getPitchAt(double time, float defaultValue = 0.0f) const` | Evaluate the curve at `time`. |
| `static float snapToIntervals(float hz, const juce::Array<int>& intervals)` | Snap to an explicit interval set. |
| `std::unique_ptr<juce::XmlElement> toXml() const` / `void fromXml(const juce::XmlElement&)` | Serialization. |
| `void loadPreset(const juce::String& presetName)` | Load a factory curve preset (`"default"`, `"spoken"`, `"lyric"`, `"rap"`, `"robot"`). |
| `setStepMode/setSnapEnabled/setSnapToGridEnabled` + getters | Editing state (persisted with the curve). |

## Preset morphing

### `ovtdsp::MorphState` (struct)

Snapshot of all interpolable plugin parameters (continuous floats, discrete
ints, booleans) plus a `PitchCurve` and a `name`.

### `ovtdsp` morph helpers (inline)

| Function | Description |
|----------|-------------|
| `MorphState captureState(juce::AudioProcessorValueTreeState&, const ovtdsp::PitchCurve&, const juce::String& name)` | Capture all parameters into a state. |
| `juce::StringArray getMorphParameterIds()` | All parameter IDs a morph can drive. |
| `void applyInterpolatedState(params, source, target, float morphAmount, const juce::StringArray* exclude)` | Apply interpolation; continuous lerp, discrete/bool step at 50%; `exclude` skips externally-automated params. |

## Note utilities

### `ovtdsp::NoteUtils.h` helpers

| Function | Description |
|----------|-------------|
| `float hzToMidiFloat(float hz)` | Hz → MIDI float. |
| `float midiToHz(float midi)` | MIDI → Hz. |
| `float hzToCents(float hzDetected, float hzTarget)` | Cents offset (positive = detected above target). |
| `int midiToOctave(int)` / `int midiToNoteInOctave(int)` | Octave / note-in-octave. |
| `const char* noteInOctaveName(int)` | Short name (C, C#, …, B). |
| `juce::String hzToNoteName(float hz)` | Full name (e.g. `"F3"`), `"--"` if invalid. |
| `struct NoteInfo` | `name`, `targetName`, `midi`, `targetMidi`, `octave`, `noteInOct`, `cents`, `valid`. |
| `NoteInfo describePitch(float hzIn, float hzTarget)` | Note + cents display data used by the visualizer. |
