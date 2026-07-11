# ARA2 Waveform Overlay - Implementation Plan

> Status: Planning  
> Last updated: 2026-07-08

## Goal

Display the host DAW's audio waveform behind the pitch curves in the Live visualizer when running in ARA mode (Studio One, Cubase, REAPER, etc.).

## Current State

- `PitchVisualizer::setWaveformOverlay(samples, numSamples, sampleRate)` **is called** from `PluginEditor::timerCallback()` (guarded by the `showWaveform` hamburger-menu toggle) and forwarded to both the visualizer and the curve editor.
- `PitchVisualizer::paintWaveformOverlay()` renders the waveform correctly when data is provided
- `AudioProcessorARAExtension` is inherited by the processor
- `ARADocumentControllerSpecialisation` is implemented (minimal)
- The cached waveform is a **mono downmix of the input audio captured in `processBlock()`** (all modes), NOT data read from an ARA content reader. The ARA content-reader extraction path described below is **not yet implemented/used**.

## JUCE ARA2 Audio Access Pattern

In JUCE's ARA2 integration, audio data is accessed through:

```
Processor (AudioProcessorARAExtension)
  -> getPlaybackRegions()
    -> each ARAPlaybackRegion has:
      -> getAudioSource() -> getSampleRate(), getChannelCount(), getFrameCount()
      -> getContentReaderForDefinition(ARAContentDefinition, startSample, sampleCount)
        -> ARAContentReader with readAudioData() method
```

## Implementation Steps

### Step 1: Store Waveform Data in Processor (PluginProcessor.h/.cpp)

The members already exist in `OpenVoxTunerAudioProcessor` (implemented):
```cpp
// ARA waveform cache (actually a mono downmix of input audio)
juce::AudioBuffer<float> araWaveformBuffer;
double araWaveformSampleRate = 44100.0;
bool araWaveformReady = false;
juce::CriticalSection araWaveformLock;
```
They are populated by a mono downmix capture in `processBlock()` (see Step 2),
and exposed to the editor via `copyAraWaveform()` / `isAraWaveformReady()`.

### Step 2: Capture Mono Downmix in processBlock (PluginProcessor.cpp)

The current implementation does **not** use an ARA content reader. Instead,
early in `processBlock()` (before DSP modifies the buffer, in all modes), it
captures a mono downmix of the input audio:

1. Read the input buffer's channels and sample count
2. Copy channel 0 into `araWaveformBuffer` (sized to 1 channel)
3. Add the remaining channels and apply an equal gain (`1.0 / numCh`) to downmix
4. Store `currentSampleRate` as `araWaveformSampleRate`
5. Set `araWaveformReady = true`

Under lock:
```cpp
const int numSamples = buffer.getNumSamples();
const int numCh = buffer.getNumChannels();
if (numSamples > 0 && numCh > 0)
{
    const juce::CriticalSection::ScopedLockType sl (araWaveformLock);
    araWaveformBuffer.setSize (1, numSamples, false, false, true);
    araWaveformBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
    for (int ch = 1; ch < numCh; ++ch)
        araWaveformBuffer.addFrom (0, 0, buffer, ch, 0, numSamples);
    araWaveformBuffer.applyGain (0, 0, numSamples, 1.0f / (float) numCh);
    araWaveformSampleRate = currentSampleRate;
    araWaveformReady = true;
}
```

> Note: ARA content-reader extraction (`getPlaybackRegions()` /
> `createContentReaderForDefinition`) is **not yet used**. If you want the
> overlay to reflect the DAW's clip data (rather than live input), that path
> remains future work.

### Step 3: Forward to Visualizer in Editor (PluginEditor.cpp)

In `timerCallback()`, when the hamburger "Show Waveform" (`showWaveform`) is
enabled, copy the cached buffer and forward it to both editors:
```cpp
juce::AudioBuffer<float> waveform;
double sr = 44100.0;
processorRef.copyAraWaveform (waveform, sr);
if (waveform.getNumSamples() > 0)
{
    const float* data = waveform.getReadPointer (0);
    if (pitchVisualizer != nullptr)
        pitchVisualizer->setWaveformOverlay (data, waveform.getNumSamples(), sr);
    if (curveEditor != nullptr)
        curveEditor->setWaveformOverlay (data, waveform.getNumSamples(), sr);
}
```

### Step 4: Render in Visualizer (PitchVisualizer.cpp)

The existing `paintWaveformOverlay()` already handles rendering. The waveform is drawn as a light blue semi-transparent fill behind the pitch curves.

Additional improvements:
- Map waveform samples to the visualizer's time/pitch coordinate system
- Use the ARA playback region's time range to align the waveform with the visualizer's scroll position
- Handle multiple regions (concatenated or overlaid)

### Step 5: Time Alignment

The visualizer scrolls based on transport time (from `setPlayheadTime()`). The waveform must be aligned to the same timeline:
- Each `ARAPlaybackRegion` has `getLocationStart()` / `getLocationEnd()` (in playback frames)
- Convert these to the visualizer's x-axis coordinate system
- Only render the visible portion of the waveform

## Files to Modify

| File | Changes |
|------|---------|
| `PluginProcessor.h` | Waveform cache members + lock (already present) |
| `PluginProcessor.cpp` | Capture mono downmix in `processBlock()` (already present); ARA content-reader extraction not yet used |
| `PluginEditor.cpp` | Forward waveform data to visualizer/curve editor in `timerCallback()` (already present) |
| `PitchVisualizer.h` | Update `setWaveformOverlay()` signature (time alignment) |
| `PitchVisualizer.cpp` | Improve `paintWaveformOverlay()` for ARA time alignment |

## Risks and Mitigations

1. **Performance**: Reading ARA content on every processBlock is expensive.  
   *Mitigation*: Read once per region change, cache the result. Only re-read when the region boundary changes.

2. **DAW compatibility**: Not all DAWs expose audio through ARA content readers.  
   *Mitigation*: Check `isBoundToARA()` before attempting. Graceful fallback to pitch-only display.

3. **Thread safety**: ARA content readers may not be real-time safe.  
   *Mitigation*: Read on a background thread, use double-buffering with lock.

4. **Multiple regions**: Some projects have many overlapping regions.  
   *Mitigation*: For MVP, only display the first/primary region's waveform.

## Testing Plan

1. Test in Studio One with a single vocal clip
2. Verify waveform alignment with pitch curves
3. Test with multiple regions (should show primary only)
4. Test in non-ARA mode (should show nothing, no crash)
5. Performance profiling: ensure <1ms overhead in processBlock
