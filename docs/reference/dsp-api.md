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

!!! note "Legacy"
    `ovtdsp::PitchDetector` (in `PitchDetector.h`) is a **legacy YIN detector
    superseded by `YinPitchDetector`** and is no longer used by the pipeline.

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
| `virtual void process(juce::AudioBuffer<float>& buffer, bool enabled, float wetMix)` | Apply in place. |

### `ovtdsp::ReverbEffect : public IEffect`

- `setRoomSize(float)` / `setDamping(float)` — reverb tuning.
- Wraps `juce::Reverb`; smooths the master enable gain to avoid clicks.

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
