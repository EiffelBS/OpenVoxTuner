# Multi-Engine Pitch Shifting Architecture

> **📁 ARCHIVED (2026-07-11):** Historical document. OpenVoxTuner now uses **a single pitch-shifting engine (PSOLA)**. The RubberBand and SoundTouch engines have been removed from the codebase. This multi-engine architecture was evaluated and then abandoned in favor of a unified PSOLA solution.

This implementation allows dynamically switching between several pitch-shifting engines on the fly.

## 1. Common Interface: `IPitchShifter`
All engines implement the `IPitchShifter` interface (`Source/dsp/IPitchShifter.h`) which guarantees interchangeability.

```cpp
class IPitchShifter
{
public:
    virtual ~IPitchShifter() = default;
    virtual void prepare (double sampleRate, int maximumBlockSize) = 0;
    virtual void reset() = 0;
    virtual void process (juce::AudioBuffer<float>& buffer, float ratio, float f0) = 0;
    virtual int getLatencySamples() const = 0;
};
```

## 2. Implemented engines

### 2.1. RubberBand (`RubberBandPitchShifter`)
- **Quality**: Very high.
- **Latency**: Moderate.
- **Licence**: GPL v2+.
- **Characteristics**: Default engine, very smooth, no desynchronization at large buffer. Adapted with a FIFO system because it requires fixed blocks of 512 samples.

### 2.2. SoundTouch (`SoundTouchPitchShifter`)
- **Quality**: High (good for voice and music in general).
- **Latency**: Lower / Variable.
- **Licence**: LGPL.
- **Characteristics**: Static compilation of the source files included in `external/soundtouch/source/SoundTouch`. Requires an interleaved buffer for processing.

### 2.3. PSOLA Legacy / Delay-Line Crossfade (`PitchShifter`)
- **Quality**: Fair (more pronounced "robotic" / Chorus effect, delay-line style).
- **Latency**: Low (about 50ms fixed).
- **Licence**: Custom (In-house).
- **Characteristics**: Historically based on PSOLA, the implementation was completely rewritten to use a robust "Delay-Line Crossfade" type algorithm (close to WSOLA). It uses a sliding circular buffer with two read heads and a (Hann) windowing synchronous to the phase. This completely removes the cuts and artefacts from faulty pitch-tracking of the old algorithm.

## 3. How to add a new engine
To add an engine "X":
1. Create the files `Source/dsp/XPitchShifter.h` and `.cpp`.
2. Make `XPitchShifter` inherit from `IPitchShifter`.
3. Implement the virtual methods.
4. In `Source/PluginProcessor.cpp`, add the option to the `AudioParameterChoice` creation:
   ```cpp
   std::make_unique<juce::AudioParameterChoice> (
       "engine", "Engine", juce::StringArray { "RubberBand", "SoundTouch", "PSOLA (Legacy)", "MoteurX" }, 0)
   ```
5. In `PluginProcessor::processBlock`, add a `case 3:` to the dynamic selection switch.
6. In `Source/PluginEditor.cpp`, add the corresponding line to the ComboBox:
   ```cpp
   engineBox.addItem ("MoteurX", 4);
   ```

## 4. User selection
The change is made via the "Engine" UI parameter (which is a `juce::AudioParameterChoice`). When it changes, the `PluginProcessor` releases the old engine, allocates the new one and immediately calls `prepare()` before continuing the block processing.
