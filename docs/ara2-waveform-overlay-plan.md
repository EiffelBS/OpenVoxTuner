# ARA2 Waveform Overlay - Implementation Plan

> Status: Planning  
> Last updated: 2026-07-08

## Goal

Display the host DAW's audio waveform behind the pitch curves in the Live visualizer when running in ARA mode (Studio One, Cubase, REAPER, etc.).

## Current State

- `PitchVisualizer::setWaveformOverlay(samples, numSamples, sampleRate)` exists but is **never called**
- `PitchVisualizer::paintWaveformOverlay()` renders the waveform correctly when data is provided
- `AudioProcessorARAExtension` is inherited by the processor
- `ARADocumentControllerSpecialisation` is implemented (minimal)

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

Add members to `OpenVoxTunerAudioProcessor`:
```cpp
// ARA waveform cache
juce::AudioBuffer<float> araWaveformBuffer;
double araWaveformSampleRate = 44100.0;
bool araWaveformReady = false;
juce::CriticalSection araWaveformLock;
```

### Step 2: Extract Audio in processBlock (PluginProcessor.cpp)

In `processBlock()`, when ARA is active:
1. Get playback regions via `getPlaybackRegions()`
2. For the first region, create a content reader
3. Read audio data into a temporary buffer
4. Downmix to mono if stereo
5. Store in `araWaveformBuffer` under lock
6. Set `araWaveformReady = true`

Key JUCE ARA API calls:
```cpp
auto regions = getPlaybackRegions();
if (!regions.empty())
{
    auto* region = regions[0];
    auto* source = region->getAudioSource();
    auto reader = source->createContentReaderForDefinition(
        ARAContentDefinition::audioSampleData,
        region->getLocationStart(),
        region->getLocationEnd() - region->getLocationStart());
    // reader->readAudioData() into buffer
}
```

### Step 3: Forward to Visualizer in Editor (PluginEditor.cpp)

In `timerCallback()`, when ARA is active:
```cpp
if (processorRef.araWaveformReady && pitchVisualizer != nullptr)
{
    // Under lock, copy the waveform buffer
    juce::AudioBuffer<float> temp;
    {
        const juce::SpinLock::ScopedLockType sl(processorRef.araWaveformLock);
        temp.makeCopyOf(processorRef.araWaveformBuffer);
    }
    pitchVisualizer->setWaveformOverlay(
        temp.getReadPointer(0),
        temp.getNumSamples(),
        processorRef.araWaveformSampleRate);
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
| `PluginProcessor.h` | Add waveform cache members, lock |
| `PluginProcessor.cpp` | Extract audio in `processBlock()` via ARA content reader |
| `PluginEditor.cpp` | Forward waveform data to visualizer in `timerCallback()` |
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
