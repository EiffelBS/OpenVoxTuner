# Changelog - 2026-07-10

## Refactor: A/B Slot Persistence — XML to MorphState

### Overview
Refactored the A/B comparison slot persistence layer from XML-based storage to direct `MorphState` storage. This eliminates the exponential growth problem that occurred when A/B slots embedded full XML plugin states that recursively included each other.

### Problem
Previously, saving an A/B slot would serialize the entire plugin state (parameters + pitch curve + nested A/B slot data) as an XML blob stored in a `juce::XmlElement`. When slots were saved repeatedly, the embedded XML would grow exponentially because each save included the previous slot state, which itself included the state before that, and so on.

### Solution
A/B slots now store a flat `atdsp::MorphState` struct directly in the processor. For persistence across sessions, the MorphState fields are serialized as compact flat XML attributes (`AB_A` and `AB_B` child elements) in `getStateInformation`, and restored from the same flat attributes in `setStateInformation`. This produces a fixed-size serialization regardless of how many times slots are saved.

### Changes

#### `Source/PluginProcessor.h`
- Added `#include "dsp/PresetMorpher.h"` for MorphState access
- Replaced `setAbSlotXml()` / `getAbSlotXml()` with `setAbSlotMorphState()` / `getAbSlotMorphState()` / `hasAbSlotData()`
- Replaced `std::unique_ptr<juce::XmlElement> abSlotAxml / abSlotBxml` members with `atdsp::MorphState abSlotAMorph / abSlotBMorph` + `bool abSlotHasData[2]`

#### `Source/PluginProcessor.cpp`
- `getStateInformation()`: Now persists A/B MorphStates as compact flat XML attributes (`AB_A`/`AB_B` child elements with 24 scalar attributes each). Fixed-size output regardless of save count.
- `setStateInformation()`: Restores A/B MorphStates from flat XML attributes. Reads each field with safe defaults matching the MorphState constructor values.

#### `Source/PluginEditor.cpp`
- `restoreSlot` lambda: Simplified from ~70 lines of XML parsing to 8 lines that read directly from the processor's MorphState via `getAbSlotMorphState()`.
- `saveSlot()`: Removed the XML building block (`paramState.createXml()`, `slot.state` assignment, `setAbSlotXml()`). Now only captures MorphState, sets hasData/name, and persists via `setAbSlotMorphState()`.

### Files Modified
- `Source/PluginProcessor.h` — MorphState storage, new A/B API
- `Source/PluginProcessor.cpp` — Flat attribute serialization/deserialization
- `Source/PluginEditor.cpp` — Simplified restoreSlot and saveSlot
