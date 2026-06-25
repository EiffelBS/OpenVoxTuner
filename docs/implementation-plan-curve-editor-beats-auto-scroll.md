# Implementation Plan — Curve Editor: Customizable Measures & ARA Auto-Scroll

> Date: 2026-06-24
> Based on feasibility analysis with Jerome.
> Approach: time-signature-aware measures (3/4, 4/4, 6/8, etc.) with ARA/variable support.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Feature 1 — Customizable Measures](#2-feature-1--customizable-measures)
   - 2.1 What changes for the user
   - 2.2 Architecture & data flow
   - 2.3 Time signature resolution strategy
   - 2.4 ARA multi-signature support
   - 2.5 Files to modify
   - 2.6 Implementation steps
3. [Feature 2 — ARA Auto-Scroll](#3-feature-2--ara-auto-scroll)
   - 3.1 What changes for the user
   - 3.2 Architecture & scrolling algorithm
   - 3.3 Files to modify
   - 3.4 Implementation steps
4. [Testing & Validation](#4-testing--validation)
5. [Project roadmap update](#5-project-roadmap-update)

---

## 1. Overview

Two independent features that share some infrastructure (time signature awareness):

| Feature | Effort | Difficulty | Dependencies |
|---------|--------|------------|--------------|
| 1. Customizable measures | ~2-3 h | Medium | Time signature reader |
| 2. ARA Auto-Scroll | ~1.5-2 h | Easy | Feature 1 (time axis) |

Both features require **no restructuring** of the existing coordinate system. The
`timeVisible` variable is already parametric — all paint loops `for (t = 0 to timeVisible)`
adapt automatically.

---

## 2. Feature 1 — Customizable Measures

### 2.1 What changes for the user

**Before:** A fixed ruler of 16 beats (4 measures of 4/4). No way to change it.
**After:** A ComboBox allowing the user to choose **1, 2, 4, or 8 measures** visible.
The ruler adapts to the DAW's current time signature.

Examples of the new ruler display:

**4/4, 4 measures visible:**
```
|  M1  |  M2  |  M3  |  M4  |
1.1 1.2 1.3 1.4 2.1 2.2 ...
```

**6/8, 4 measures visible:**
```
|  M1      |  M2      |  M3      |
1.1 . 1.2 . 1.3 . 2.1 . 2.2 . ...
```
(6 eighth-notes per bar, shown as 12 PPQ subdivisions per bar)

**3/4, 4 measures visible:**
```
|  M1  |  M2  |  M3  |  M4  |
1.1 1.2 1.3 2.1 2.2 2.3 ...
```

### 2.2 Architecture & data flow

```
┌─────────────────────────────────────────────────────────────────────┐
│  PluginProcessor (audio thread)                                       │
│                                                                       │
│  ARA mode: reads ARAContentTypeBarSignatures → TimeSignature list      │
│  VST3 mode: reads getPlayHead()->getPosition()->getTimeSignature()    │
│                                                                       │
│  Stores current TimeSignature in atomic (thread-safe for UI reader)   │
└──────────────────────────┬──────────────────────────────────────────┘
                           │ getCurrentTimeSignature()
                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│  PluginEditor (UI timerCallback, 30 Hz)                              │
│                                                                       │
│  Reads time signature from processor.                                 │
│  Calls: curveEditor->setTimeSignature(num, den)                      │
│  Calls: curveEditor->setMeasuresVisible(measuresCombo)               │
└──────────────────────────┬──────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│  PitchCurveEditor (paint, 30 Hz)                                     │
│                                                                       │
│  measuresVisible = 4 (user choice)                                    │
│  timeSignature { 6, 8 } (from DAW/ARA)                                │
│                                                                       │
│  ppqPerBar = numerator * (4.0 / denominator)  → 6 * 0.5 = 3.0       │
│  timeVisible = measuresVisible * ppqPerBar → 4 * 3.0 = 12.0         │
│  beatUnit = 4.0 / denominator  →  4/8 = 0.5 PPQ (eighth note)       │
│                                                                       │
│  Ruler paint loop:                                                    │
│    for t = 0 to timeVisible step beatUnit:                            │
│      bar = floor(t / ppqPerBar) + 1                                   │
│      beatInBar = (t % ppqPerBar) / beatUnit + 1                      │
│      label = sprintf("%d.%d", bar, beatInBar)                        │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.3 Time signature resolution strategy

The plugin reads the time signature from two sources, with the following priority:

| Priority | Source | Available in | Multiple changes? |
|----------|--------|-------------|-------------------|
| 1 | ARA `kARAContentTypeBarSignatures` | ARA mode only | Yes (full timeline) |
| 2 | `AudioPlayHead::getPosition()->getTimeSignature()` | VST3 / Standalone | No (current only) |
| 3 | Default 4/4 | Fallback | N/A |

**ARA mode resolution (priority 1):**

```cpp
// In processBlock ARA section (already reads KeySignatures, add BarSignatures)
if (isBoundToARA()) {
    ARA::PlugIn::HostContentReader<ARA::kARAContentTypeBarSignatures> reader(contexts[0]);
    for (int i = 0; i < reader.getEventCount(); ++i) {
        auto* barSig = reader.getDataPtrForEvent(i);
        // Store in a time-sorted list of TimeSignature
        // { position: barSig->position, num: barSig->numerator, den: barSig->denominator }
    }
}
```

**VST3 mode resolution (priority 2):**

```cpp
// In processBlock transport section (already has getPlayHead())
if (auto* playHead = getPlayHead()) {
    auto position = playHead->getPosition();
    if (position.hasValue()) {
        auto sig = position->getTimeSignature();
        if (sig.hasValue()) {
            // Stored as atomic int for numerator, denominator
        }
    }
}
```

**Getting the signature at any PPQ position:**

```cpp
// For VST3 (single current signature, no history):
TimeSignature getTimeSignatureAt(double ppq) {
    if (araBarSignatures.size() > 0)
        return findLastBarSignatureBefore(ppq); // ARA: find by position
    return currentVst3Signature; // VST3: always current
}
```

### 2.4 ARA multi-signature support

ARA exposes bar signatures as a **time-ordered event list** via
`ARAContentTypeBarSignatures`. Each event has:

```cpp
typedef struct ARAContentBarSignature {
    ARAInt32 numerator;       // e.g., 6 for 6/8
    ARAInt32 denominator;     // e.g., 8 for 6/8
    ARAQuarterPosition position; // start time in quarter notes
} ARAContentBarSignature;
```

The implementation stores these in the processor as an array of events sorted by
position. When the editor requests the time signature at a given PPQ, it finds the
last bar signature event before or at that position.

**Storage in PluginProcessor.h:**
```cpp
struct BarSignatureEvent {
    double ppqPosition;     // ARAContentBarSignature::position as double
    int numerator;
    int denominator;
};

// Stored in the processor for thread-safe access
std::vector<BarSignatureEvent> araBarSignatures;
```

**Performance:** Bar signature changes are rare (typically 1-3 per project). A
linear scan over 2-3 events is sub-microsecond.

### 2.5 Files to modify

| File | Change description |
|------|--------------------|
| `Source/PluginProcessor.h` | Add `TimeSignature` storage: `currentNumerator`, `currentDenominator` atomics + `araBarSignatures` vector + `BarSignatureEvent` struct |
| `Source/PluginProcessor.cpp` | Read `ARAContentTypeBarSignatures` in ARA block; read `getTimeSignature()` in VST3 transport block; add accessor methods |
| `Source/PluginEditor.h` | Add `measuresCombo` member + `ComboBoxAttachment`; add `initMeasureBox()` helper |
| `Source/PluginEditor.cpp` | Create ComboBox with "1", "2", "4", "8" measures; bind to processor parameter; call `curveEditor->setMeasuresVisible() + setTimeSignature()` each timer tick |
| `Source/ui/PitchCurveEditor.h` | Add members: `measuresVisible`, `timeSignatureNum`, `timeSignatureDen`; new methods: `setMeasuresVisible(int)`, `setTimeSignature(int num, int den)`; update `timeVisible` calculation |
| `Source/ui/PitchCurveEditor.cpp` | Rewrite ruler paint with time-signature-aware bars and beats; recalculate `timeVisible` on `setMeasuresVisible()` or `setTimeSignature()` |
| `Source/PluginProcessor.cpp` (params) | New `AudioParameterInt` "editor_measures" (1-8, default 4) |

### 2.6 Implementation steps (detailed)

#### Step 1 — Time signature infrastructure in PluginProcessor

1.1 Add atomics and struct in `PluginProcessor.h`:
```cpp
std::atomic<int> currentTimeSigNumerator { 4 };
std::atomic<int> currentTimeSigDenominator { 4 };

struct BarSignatureEvent {
    double ppqPosition;
    int numerator;
    int denominator;
};
std::vector<BarSignatureEvent> araBarSignatures;
juce::CriticalSection araBarSigLock;
```

1.2 Add accessors:
```cpp
int getCurrentTimeSigNumerator() const { return currentTimeSigNumerator.load(); }
int getCurrentTimeSigDenominator() const { return currentTimeSigDenominator.load(); }
void getTimeSignatureAt(double ppq, int& num, int& den) const;
```

1.3 In `processBlock()` ARA section (around line 570), add:
```cpp
// Read bar signatures from ARA
{
    ARA::PlugIn::HostContentReader<ARA::kARAContentTypeBarSignatures> barReader(contexts[0]);
    juce::ScopedLock lock(araBarSigLock);
    araBarSignatures.clear();
    for (int i = 0; i < barReader.getEventCount(); ++i) {
        auto* barSig = barReader.getDataPtrForEvent(i);
        araBarSignatures.push_back({ (double)barSig->position, barSig->numerator, barSig->denominator });
    }
    // Update current from first event if any
    if (!araBarSignatures.empty()) {
        currentTimeSigNumerator.store(araBarSignatures[0].numerator);
        currentTimeSigDenominator.store(araBarSignatures[0].denominator);
    }
}
```

1.4 In `processBlock()` transport section (around line 701), add non-ARA fallback:
```cpp
if (!isBoundToARA()) {
    auto sig = position->getTimeSignature();
    if (sig.hasValue()) {
        currentTimeSigNumerator.store(sig->numerator);
        currentTimeSigDenominator.store(sig->denominator);
    }
}
```

#### Step 2 — Parameter for user choice

2.1 In `PluginProcessor` constructor, add parameter:
```cpp
std::make_unique<juce::AudioParameterInt>("editor_measures", "Editor Measures", 1, 8, 4)
```

2.2 Add atomic pointer:
```cpp
std::atomic<float>* editorMeasuresParam = nullptr;
```
...and retrieve in constructor:
```cpp
editorMeasuresParam = parameters.getRawParameterValue("editor_measures");
```

#### Step 3 — PitchCurveEditor changes

3.1 In `PitchCurveEditor.h`, add:
```cpp
void setMeasuresVisible(int measures);
void setTimeSignature(int numerator, int denominator);
```
And members:
```cpp
int measuresVisible = 4;
int timeSigNum = 4;
int timeSigDen = 4;
```

3.2 In `PitchCurveEditor.cpp`, implement:
```cpp
void PitchCurveEditor::setMeasuresVisible(int measures) {
    measuresVisible = juce::jlimit(1, 8, measures);
    recalculateTimeVisible();
    repaint();
}

void PitchCurveEditor::setTimeSignature(int num, int den) {
    timeSigNum = num;
    timeSigDen = den;
    recalculateTimeVisible();
    repaint();
}

void PitchCurveEditor::recalculateTimeVisible() {
    double ppqPerBar = timeSigNum * (4.0 / timeSigDen);
    timeVisible = measuresVisible * ppqPerBar;
}
```

3.3 Rewrite ruler paint loop (replace the existing `for (double t = 0.0...timeVisible)` block):
```cpp
const double beatUnit = 4.0 / timeSigDen;         // e.g., 0.5 for 6/8 (eighth note)
const double ppqPerBar = timeSigNum * beatUnit;    // e.g., 3.0 for 6/8

for (double t = 0.0; t <= timeVisible; t += beatUnit)
{
    const double x = timeToX(t);
    bool isBarStart = (std::abs(std::fmod(t, ppqPerBar)) < 0.001);
    bool isBeat = true;  // every step of beatUnit is a beat

    // Vertical line in grid
    g.setColour(kGridColour.withAlpha(isBarStart ? 0.6f : (isBeat ? 0.3f : 0.1f)));
    g.drawVerticalLine(static_cast<int>(x), rulerH, static_cast<float>(b.getHeight()));

    // Ruler ticks
    if (isBarStart) {
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.drawVerticalLine(static_cast<int>(x), rulerH - 4.0f, rulerH);

        int bar = static_cast<int>(t / ppqPerBar) + 1;
        g.setFont(11.0f);
        g.drawText("M" + juce::String(bar), static_cast<int>(x) + 4, 0, 40, rulerH,
                   juce::Justification::centredLeft);
    } else {
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.drawVerticalLine(static_cast<int>(x), rulerH - 2.0f, rulerH);
    }
}
```

#### Step 4 — PluginEditor ComboBox

4.1 In `PluginEditor.h`, add:
```cpp
juce::ComboBox measuresBox;
juce::Label measuresLabel;
std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> measuresAttachment;
```

4.2 In `PluginEditor::PluginEditor()` (or a helper), create the combo:
```cpp
measuresLabel.setText("Measures", juce::dontSendNotification);
measuresLabel.setJustificationType(juce::Justification::centred);
measuresLabel.setColour(juce::Label::textColourId, kText);
measuresLabel.setFont(juce::Font(12.0f, juce::Font::bold));
addAndMakeVisible(measuresLabel);

measuresBox.addItemList({"1", "2", "4", "8"}, 1);
measuresBox.setSelectedItemIndex(2, juce::dontSendNotification); // 4 by default
measuresBox.setColour(juce::ComboBox::backgroundColourId, kBgPanel);
measuresBox.setColour(juce::ComboBox::textColourId, kText);
measuresBox.setColour(juce::ComboBox::outlineColourId, kAccentSoft);
addAndMakeVisible(measuresBox);

measuresAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
    processorRef.getParameters(), "editor_measures", measuresBox);
```

4.3 Position the ComboBox in the Curve Editor toolbar area (in `resized()`, near
the existing tools like `snapButton`, `presetsButton`, etc. — only visible in
Curve Editor mode).

4.4 In `timerCallback()`, propagate values:
```cpp
// Update curve editor time signature and measures
if (curveEditor != nullptr) {
    int num = processorRef.getCurrentTimeSigNumerator();
    int den = processorRef.getCurrentTimeSigDenominator();
    curveEditor->setTimeSignature(num, den);
    curveEditor->setMeasuresVisible(measuresBox.getSelectedId());
}
```

---

## 3. Feature 2 — ARA Auto-Scroll

### 3.1 What changes for the user

**Before:** The playhead moves visually inside the Curve Editor but once it goes
past the right edge, the view stays fixed. The user must manually scroll the
playhead's button to see later beats.

**After:** When auto-scroll is enabled, the view follows the playhead during
DAW playback. The playhead stays at ~75% of the visible width. When playback
stops, the view stays where it is.

An **Auto-Scroll toggle button** is added to the Curve Editor toolbar. It is
enabled by default when ARA is active.

### 3.2 Architecture & scrolling algorithm

**Scrolling algorithm (smooth continuous):**

The playhead position is `cachedTransportTime` (PPQ). The scroll offset is
calculated to keep the playhead at 75% of the visible width:

```cpp
// In timerCallback or setPlayheadTime:
if (autoScrollEnabled && isPlaying) {
    double targetScroll = playheadTime - timeVisible * 0.75;
    scrollOffset = juce::jmax(0.0, targetScroll);
    repaint();
}
```

**Coordinate system changes:**

```cpp
double PitchCurveEditor::timeToX(double t) const {
    const int pianoW = pianoKeyboard.getWidth();
    const int plotW = getWidth() - pianoW;
    // Translate absolute time to view with scroll offset
    double viewT = t - scrollOffset;
    return pianoW + (viewT / timeVisible) * plotW;
}

double PitchCurveEditor::xToTime(float x) const {
    const int pianoW = pianoKeyboard.getWidth();
    const int plotW = juce::jmax(1, getWidth() - pianoW);
    double viewT = ((x - pianoW) / plotW) * timeVisible;
    return viewT + scrollOffset;
}
```

**Auto-scroll limits:**
- Start of project: `scrollOffset = 0` (never goes negative)
- No hard limit at end (user can scroll past the last point)
- When playback stops, scroll stays in place

**Activation logic:**
```cpp
if (autoScrollEnabled) {
    bool isPlaying = (playheadTime != lastPlayheadTime); // simple heuristic
    // In ARA mode, use playback state from host
    if (isPlaying) {
        // apply scroll formula above
        // hide the playhead visual when it's beyond the right edge
    } else {
        // playback stopped -> keep current scroll, don't reset
    }
}
```

**Overlap with playhead paint:**
```cpp
double displayPlayhead = playheadTime;
if (displayPlayhead >= scrollOffset && displayPlayhead <= scrollOffset + timeVisible) {
    const float x = timeToX(displayPlayhead);
    g.setColour(juce::Colours::red.withAlpha(0.8f));
    g.drawVerticalLine(x, rulerH, b.getHeight());
}
```

### 3.3 Files to modify

| File | Change description |
|------|--------------------|
| `Source/PluginProcessor.h` | Add `AudioParameterBool` "auto_scroll" |
| `Source/PluginProcessor.cpp` | Add parameter declaration, get pointer |
| `Source/ui/PitchCurveEditor.h` | Add members: `scrollOffset`, `autoScrollEnabled`, `lastPlayheadTime`; methods: `setAutoScroll(bool)`, `setPlayheadTime()` override |
| `Source/ui/PitchCurveEditor.cpp` | Modify `timeToX()`, `xToTime()`, `paint()` ruler loop, `paint()` playhead, `setPlayheadTime()` scroll logic |
| `Source/PluginEditor.h` | Add `autoScrollButton` toggle |
| `Source/PluginEditor.cpp` | Create toggle button in Curve Editor toolbar; bind to parameter; update curve editor each timer tick |

### 3.4 Implementation steps (detailed)

#### Step 1 — PitchCurveEditor members

In `PitchCurveEditor.h`, add:
```cpp
double scrollOffset = 0.0;
bool autoScrollEnabled = false;
double lastPlayheadTime = -1.0;

void setAutoScroll(bool enabled);
void checkAutoScroll();
```

Override `setPlayheadTime` (already exists) to add auto-scroll logic.

#### Step 2 — Scroll logic

In `PitchCurveEditor.cpp`:
```cpp
void PitchCurveEditor::setPlayheadTime(double time) {
    bool isPlaying = (time != lastPlayheadTime && time > 0.0);
    lastPlayheadTime = time;
    playheadTime = time;

    if (autoScrollEnabled && isPlaying) {
        double targetScroll = time - timeVisible * 0.75;
        scrollOffset = juce::jmax(0.0, targetScroll);
    }

    repaint();
}

void PitchCurveEditor::setAutoScroll(bool enabled) {
    autoScrollEnabled = enabled;
    if (!enabled && scrollOffset > 0.0) {
        // Optionally reset scroll to 0 when disabling? No, keep position.
    }
}
```

#### Step 3 — Coordinate system update

In `PitchCurveEditor::timeToX()`:
```cpp
double viewT = t - scrollOffset;
return pianoW + (viewT / timeVisible) * plotW;
```

In `PitchCurveEditor::xToTime()`:
```cpp
double viewT = ((x - pianoW) / plotW) * timeVisible;
return viewT + scrollOffset;
```

#### Step 4 — Ruler paint update

The ruler loop must now iterate over the visible range, not from 0:
```cpp
const double beatUnit = 4.0 / timeSigDen;
const double ppqPerBar = timeSigNum * beatUnit;

double rulerStart = scrollOffset;
double rulerEnd = scrollOffset + timeVisible;

// Align rulerStart to the nearest beat for clean grid lines
double alignedStart = std::floor(rulerStart / beatUnit) * beatUnit;

for (double t = alignedStart; t <= rulerEnd; t += beatUnit)
{
    if (t < rulerStart) continue; // skip positions before view start
    const double x = timeToX(t);
    // ... rest of ruler painting same as Feature 1 ...
}
```

#### Step 5 — PluginEditor toggle button

In PluginEditor, create the toggle button in the Curve Editor toolbar:
```cpp
autoScrollToggle.setButtonText("Auto-Scroll");
autoScrollToggle.setColour(juce::ToggleButton::textColourId, kText);
autoScrollToggle.setColour(juce::ToggleButton::tickColourId, kAccent);
autoScrollToggle.setTooltip("Auto-scroll follows the playhead during playback");
addAndMakeVisible(autoScrollToggle);

autoScrollAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
    processorRef.getParameters(), "auto_scroll", autoScrollToggle);
```

In `timerCallback()`:
```cpp
curveEditor->setAutoScroll(autoScrollToggle.getToggleState());
```

---

## 4. Testing & Validation

| Test case | How to verify | Feature |
|-----------|--------------|---------|
| Select 2 measures, sing in 4/4 | Ruler shows M1, M2 with 4 beats each | F1 |
| Select 4 measures in 6/8 | Ruler shows M1-M4 with 6 eighth-note ticks each | F1 |
| Change from 4 measures to 1 | View shrinks, no visual artifacts | F1 |
| Load project with 3/4 signature | Ruler shows 3 beats per bar, labels correct | F1 |
| ARA project with time signature change mid-song | Ruler adapts after the change point | F1 |
| Toggle auto-scroll ON in Standalone | Playhead scrolls the view, stays at 75% | F2 |
| Toggle auto-scroll OFF | Playhead moves but view stays fixed | F2 |
| Drag points while auto-scrolling | Drag still works, coordinates correct | F2 |
| Start playback at bar 5 in ARA | View scrolls to show bar 5 and beyond | F2 |
| Resize plugin window (600-1920px) | Layout adapts, ruler still readable | Both |
| Load preset, restart DAW | Measures choice restored from state | F1 persistence |

---

## 5. Project roadmap update

The following new entries will be added to `roadmap.md`:

### Phase 14 — Curve Editor: Customizable Measures

- [ ] **Time signature infrastructure**: Read `ARAContentTypeBarSignatures` (ARA) and `AudioPlayHead::getTimeSignature()` (VST3) in `PluginProcessor`
- [ ] **Parameter `editor_measures`**: `AudioParameterInt` 1-8, default 4, persisted
- [ ] **Ruler rewrite**: Time-signature-aware bars/beats labels (M1, M2...)
- [ ] **ComboBox in toolbar**: Measures selector (1, 2, 4, 8) in Curve Editor
- [ ] **ARA multi-signature**: Bar signature changes mid-project in ARA mode
- [ ] **Validation**: Visual check on 3/4, 4/4, 6/8, 12/8 with 1-8 measures

### Phase 15 — Curve Editor: ARA Auto-Scroll

- [ ] **Auto-scroll algorithm**: Smooth continuous scroll, playhead at 75%
- [ ] **Coordinate update**: `timeToX`/`xToTime` with `scrollOffset`
- [ ] **Toggle button**: Auto-Scroll in Curve Editor toolbar
- [ ] **Parameter `auto_scroll`**: `AudioParameterBool`, persisted
- [ ] **Default ON in ARA**: Auto-detect ARA mode
- [ ] **Validation**: Drag during scroll, resize, playback start/stop

### Files created

- `docs/implementation-plan-curve-editor-beats-auto-scroll.md`