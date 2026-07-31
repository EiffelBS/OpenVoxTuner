# Implementation Plan -- Curve Editor: MIDI Import via Drag-and-Drop

> Date: 2026-07-31
> Based on feasibility analysis with Jerome.
> Approach: FileDragAndDropTarget on PluginEditor + juce::MidiFile parsing + MidiImporter DSP utility.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture & Data Flow](#2-architecture--data-flow)
3. [Step 1 -- MidiImporter DSP Module](#3-step-1--midiimporter-dsp-module)
4. [Step 2 -- FileDragAndDropTarget on PluginEditor](#4-step-2--filedraganddroptarget-on-plugineditor)
5. [Step 3 -- Multi-Channel Selection Dialog](#5-step-3--multi-channel-selection-dialog)
6. [Step 4 -- Hamburger Menu "Import MIDI" Item](#6-step-4--hamburger-menu-import-midi-item)
7. [Step 5 -- PitchCurveEditor Integration](#7-step-5--pitchcurveeditor-integration)
8. [Step 6 -- Undo Support & Step Mode](#8-step-6--undo-support--step-mode)
9. [Step 7 -- Internationalization (i18n)](#9-step-7--internationalization-i18n)
10. [Step 8 -- Documentation & Roadmap Update](#10-step-8--documentation--roadmap-update)
11. [Files Summary](#11-files-summary)
12. [Testing & Validation](#12-testing--validation)
13. [Success Criteria](#13-success-criteria)

---

## 1. Overview

A single feature enabling users to import a MIDI file (.mid) into the Curve Editor
via drag-and-drop or menu, automatically converting MIDI note data into an editable
pitch curve.

| Aspect | Detail |
|--------|--------|
| Effort | ~6-8 h |
| Difficulty | Medium-High |
| Dependencies | None (self-contained feature) |
| JUCE modules used | `juce_audio_basics` (MidiFile, MidiMessageSequence), `juce_gui_basics` (FileDragAndDropTarget) -- already linked |
| New source files | 2 (MidiImporter.h, MidiImporter.cpp) |

**What changes for the user:**

**Before:** The Curve Editor curve can only be drawn by hand, loaded from presets, or
captured from live audio. There is no way to import external MIDI data.

**After:** The user can drag a .mid file from the OS file explorer onto the plugin
window (or use "Import MIDI..." in the Curve Editor hamburger menu). The MIDI notes
are parsed, and a pitch curve is generated in the Curve Editor, ready for further
editing.

---

## 2. Architecture & Data Flow

```
                        User action
                            |
              +-------------+-------------+
              |                           |
     Drag .mid file              Click "Import MIDI..."
     onto plugin window          in hamburger menu
              |                           |
              v                           v
  PluginEditor                   juce::FileChooser
  ::filesDropped()               (file dialog)
              |                           |
              +-------------+-------------+
                            |
                            v
                 ovtdsp::MidiImporter
                 ::analyzeFile(file)
                            |
                            v
                   +--------+--------+
                   |                 |
              1 channel          Multi-channel
                   |                 |
                   v                 v
          Direct import     SelectionDialog popup
          (no dialog)       (user picks channel/strategy)
                   |                 |
                   +--------+--------+
                            |
                            v
                 ovtdsp::MidiImporter
                 ::importFrom(file, strategy, channel)
                            |
                            v
                 ovtdsp::PitchCurve
                 (points: time_sec -> pitch_hz)
                            |
                            v
                 curveEditor->setCurve(newCurve)
                 curveEditor->setStepMode(true)
                 curveEditor->repaint()
```

---

## 3. Step 1 -- MidiImporter DSP Module

**Files:** `Source/dsp/MidiImporter.h` (NEW), `Source/dsp/MidiImporter.cpp` (NEW)

### 3.1 Data Structures

```cpp
namespace ovtdsp
{
    /** Metadata about a single MIDI channel found in a file. */
    struct MidiChannelInfo
    {
        int channel = 0;            // MIDI channel 1-16
        int numNotes = 0;           // total note-on count
        int minNote = 127;          // lowest MIDI note number
        int maxNote = 0;            // highest MIDI note number
        double durationSec = 0.0;   // time of last event - time of first event
    };

    /** Result of analyzing a MIDI file (metadata only, no curve generated). */
    struct MidiImportInfo
    {
        juce::Array<MidiChannelInfo> channels; // one per active channel
        int totalNotes = 0;
        double totalDurationSec = 0.0;
        int numTracks = 0;
        bool isValid = false;
        juce::String errorMessage;
    };

    /** Strategy for reducing polyphonic MIDI to a monophonic curve. */
    enum class MidiExtractStrategy
    {
        HighestNote,       // highest active note at each instant (lead melody)
        LowestNote,        // lowest active note (bass line)
        LoudestNote,       // highest velocity (interpreted as emphasis)
        SpecificChannel    // all notes from a single MIDI channel
    };
}
```

### 3.2 Public API

```cpp
namespace ovtdsp
{
    class MidiImporter
    {
    public:
        /** Analyze a .mid file and return metadata (channels, note counts, etc.).
         *  Does NOT generate a curve. Use this to decide which channel/strategy
         *  to apply before calling importFrom(). */
        static MidiImportInfo analyzeFile (const juce::File& midiFile);

        /** Import a .mid file and return a PitchCurve.
         *  @param midiFile           path to the .mid file
         *  @param strategy           polyphony reduction strategy
         *  @param targetChannel      MIDI channel (1-16), used only with SpecificChannel
         *  @param timeScaleFactor    multiplier for time axis (1.0 = seconds as-is;
         *                            if the Curve Editor uses PPQ beats, the caller
         *                            can pass the PPQ-to-seconds ratio)
         *  @return                   the generated PitchCurve (empty if parse failed)
         */
        static PitchCurve importFrom (
            const juce::File& midiFile,
            MidiExtractStrategy strategy,
            int targetChannel = 1,
            double timeScaleFactor = 1.0);
    };
}
```

### 3.3 Implementation of `analyzeFile`

```cpp
MidiImportInfo MidiImporter::analyzeFile (const juce::File& midiFile)
{
    MidiImportInfo info;

    juce::FileInputStream stream (midiFile);
    if (stream.failedToOpen())
    {
        info.errorMessage = "Cannot open file: " + midiFile.getFullPathName();
        return info;
    }

    juce::MidiFile midiFile;
    if (! midiFile.readFrom (stream))
    {
        info.errorMessage = "Invalid MIDI file format.";
        return info;
    }

    // Convert ticks to seconds
    midiFile.convertTimestampTicksToSeconds();

    info.numTracks = midiFile.getNumTracks();
    info.totalDurationSec = midiFile.getLastTimestamp();

    // Track active channels
    struct ChannelAccumulator { int count = 0; int minN = 127; int maxN = 0;
                                 double first = 1e18; double last = 0.0; };
    std::map<int, ChannelAccumulator> channelMap;

    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        const auto* track = midiFile.getTrack (t);
        if (track == nullptr) continue;

        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& msg = track->getEventPointer (i)->message;
            if (msg.isNoteOn())
            {
                const int ch = msg.getChannel();      // 1-16
                const int note = msg.getNoteNumber();
                const double time = msg.getTimeStamp();

                auto& acc = channelMap[ch];
                acc.count++;
                acc.minN = std::min (acc.minN, note);
                acc.maxN = std::max (acc.maxN, note);
                acc.first = std::min (acc.first, time);
                acc.last  = std::max (acc.last, time);
                info.totalNotes++;
            }
        }
    }

    // Exclude channel 10 (percussion)
    channelMap.erase (10);

    for (auto& [ch, acc] : channelMap)
    {
        MidiChannelInfo ci;
        ci.channel = ch;
        ci.numNotes = acc.count;
        ci.minNote = acc.minN;
        ci.maxNote = acc.maxN;
        ci.durationSec = (acc.last > acc.first) ? (acc.last - acc.first) : 0.0;
        info.channels.add (ci);
    }

    // Sort by channel number
    info.channels.sort ([] (const MidiChannelInfo& a, const MidiChannelInfo& b)
                        { return a.channel < b.channel; });

    info.isValid = (info.totalNotes > 0);
    return info;
}
```

### 3.4 Implementation of `importFrom`

```cpp
PitchCurve MidiImporter::importFrom (
    const juce::File& midiFile,
    MidiExtractStrategy strategy,
    int targetChannel,
    double timeScaleFactor)
{
    PitchCurve curve;

    juce::FileInputStream stream (midiFile);
    if (stream.failedToOpen()) return curve;

    juce::MidiFile mf;
    if (! mf.readFrom (stream)) return curve;

    mf.convertTimestampTicksToSeconds();

    // Phase 1: Extract all note-on/note-off events from the relevant channel(s)
    struct NoteEvent { double time; int note; float velocity; bool isOn; };
    juce::Array<NoteEvent> events;

    for (int t = 0; t < mf.getNumTracks(); ++t)
    {
        const auto* track = mf.getTrack (t);
        if (track == nullptr) continue;

        // Build note-on/off list, optionally matching notes for off
        std::map<int, double> activeNotes; // note -> noteOn time

        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& msg = track->getEventPointer (i)->message;
            const int ch = msg.getChannel();

            // Filter by channel
            bool channelMatch = false;
            if (strategy == MidiExtractStrategy::SpecificChannel)
                channelMatch = (ch == targetChannel);
            else
                channelMatch = (ch != 10); // all non-percussion

            if (! channelMatch) continue;

            if (msg.isNoteOn() && msg.getVelocity() > 0)
            {
                const double t_sec = msg.getTimeStamp() * timeScaleFactor;
                events.add ({ t_sec, msg.getNoteNumber(),
                              (float) msg.getVelocity(), true });
                activeNotes[msg.getNoteNumber()] = t_sec;
            }
            else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
            {
                const double t_sec = msg.getTimeStamp() * timeScaleFactor;
                events.add ({ t_sec, msg.getNoteNumber(), 0.0f, false });
                activeNotes.erase (msg.getNoteNumber());
            }
        }
    }

    // Sort events by time
    events.sort ([] (const NoteEvent& a, const NoteEvent& b)
                 { return a.time < b.time; });

    if (events.isEmpty()) return curve;

    // Phase 2: Build a time-sorted list of active notes at each event instant
    // Using a sweep-line approach
    struct ActiveNote { int note; float velocity; double endTime; };
    std::map<double, std::vector<ActiveNote>> activeAtTime;
    std::map<int, ActiveNote> currentlyActive; // note -> active info

    // Build note-off map for end times
    std::map<std::pair<int, double>, double> noteOffTimes;
    {
        // First pass: pair note-on with note-off
        std::map<int, double> pendingOn;
        for (const auto& ev : events)
        {
            if (ev.isOn)
                pendingOn[ev.note] = ev.time;
            else
            {
                auto it = pendingOn.find (ev.note);
                if (it != pendingOn.end())
                {
                    noteOffTimes[{ ev.note, it->second }] = ev.time;
                    pendingOn.erase (it);
                }
            }
        }
    }

    // Sweep: at each event time, collect active notes and pick one
    double lastPitch = 0.0f;
    double lastTime = -1.0;

    for (int i = 0; i < events.size(); ++i)
    {
        const auto& ev = events.getUnchecked (i);

        // Update active set
        if (ev.isOn)
        {
            double endTime = ev.time + 0.1; // default if no note-off found
            auto key = std::make_pair (ev.note, ev.time);
            auto it = noteOffTimes.find (key);
            if (it != noteOffTimes.end())
                endTime = it->second;

            currentlyActive[ev.note] = { ev.note, ev.velocity, endTime };
        }
        else
        {
            currentlyActive.erase (ev.note);
        }

        // Skip if this is a note-off and no note-ons happened at same time
        if (! ev.isOn && currentlyActive.empty())
            continue;

        // Select the note to use based on strategy
        int selectedNote = -1;

        if (strategy == MidiExtractStrategy::SpecificChannel
            || strategy == MidiExtractStrategy::HighestNote)
        {
            int highest = -1;
            for (auto& [note, info] : currentlyActive)
                highest = std::max (highest, note);
            selectedNote = highest;
        }
        else if (strategy == MidiExtractStrategy::LowestNote)
        {
            int lowest = 128;
            for (auto& [note, info] : currentlyActive)
                lowest = std::min (lowest, note);
            selectedNote = lowest;
        }
        else if (strategy == MidiExtractStrategy::LoudestNote)
        {
            float maxVel = 0.0f;
            for (auto& [note, info] : currentlyActive)
            {
                if (info.velocity > maxVel)
                {
                    maxVel = info.velocity;
                    selectedNote = note;
                }
            }
        }

        if (selectedNote >= 0)
        {
            const float hz = ovtdsp::midiToHz (static_cast<float> (selectedNote));
            const double t = ev.time;

            // Skip if too close to last point (same pitch, < 10 ms)
            if (std::abs (hz - lastPitch) < 1.0f
                && (t - lastTime) < 0.01)
                continue;

            curve.addOrUpdatePoint (t, hz);
            lastPitch = hz;
            lastTime = t;
        }
    }

    return curve;
}
```

### 3.5 Notes on Time Axis

The Curve Editor uses **seconds** as its time axis (confirmed by `PitchPoint::time`
being documented as "secondes" in [PitchCurve.h](file:///c:/Users/User/Documents/trae_projects/OpenVoxTuner/Source/dsp/PitchCurve.h#L18-L19)).
The `MidiFile::convertTimestampTicksToSeconds()` converts MIDI ticks to seconds,
so `timeScaleFactor = 1.0` is the correct default for direct import.

However, if the user's Curve Editor is configured in a PPQ-beat mode in the future,
the `timeScaleFactor` parameter allows the caller to convert seconds to beats.

---

## 4. Step 2 -- FileDragAndDropTarget on PluginEditor

**Files:** `Source/PluginEditor.h` (MODIFY), `Source/PluginEditor.cpp` (MODIFY)

### 4.1 Add inheritance

In `PluginEditor.h`, add `juce::FileDragAndDropTarget` to the class hierarchy:

```cpp
class OpenVoxTunerAudioProcessorEditor
    : public juce::AudioProcessorEditor,
      public juce::FileDragAndDropTarget,          // <-- NEW
      public ui::PitchCurveEditor::Listener,
      public juce::Slider::Listener,
      public juce::Button::Listener,
      public juce::ComboBox::Listener,
      public juce::Timer
```

### 4.2 Declare callbacks (private section of PluginEditor.h)

```cpp
// === MIDI Drag-and-Drop ===
bool isInterestedInFileDrag (const juce::StringArray& files) override;
void filesDropped (const juce::StringArray& files, int x, int y) override;
```

### 4.3 Implement callbacks (PluginEditor.cpp)

```cpp
bool OpenVoxTunerAudioProcessorEditor::isInterestedInFileDrag (
    const juce::StringArray& files)
{
    // Accept any file ending in .mid or .midi
    for (const auto& f : files)
        if (f.toLowerCase().endsWith (".mid") || f.toLowerCase().endsWith (".midi"))
            return true;
    return false;
}

void OpenVoxTunerAudioProcessorEditor::filesDropped (
    const juce::StringArray& files, int /*x*/, int /*y*/)
{
    // Find the first .mid file
    juce::File midiFile;
    for (const auto& f : files)
    {
        juce::File candidate (f);
        if (candidate.existsAsFile()
            && (f.toLowerCase().endsWith (".mid") || f.toLowerCase().endsWith (".midi")))
        {
            midiFile = candidate;
            break;
        }
    }

    if (! midiFile.existsAsFile()) return;

    // Ensure we are on the Curve Editor tab
    if (tabbedComponent.getCurrentTabIndex() != 1)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            "Import MIDI",
            "Please switch to the Curve Editor tab before importing MIDI files.");
        return;
    }

    // Analyze the file
    auto info = ovtdsp::MidiImporter::analyzeFile (midiFile);

    if (! info.isValid)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            "Import MIDI",
            "Error: " + info.errorMessage);
        return;
    }

    // Single non-percussion channel: import directly
    if (info.channels.size() == 1)
    {
        auto curve = ovtdsp::MidiImporter::importFrom (
            midiFile,
            ovtdsp::MidiExtractStrategy::SpecificChannel,
            info.channels.getFirst().channel);

        applyMidiImport (curve, midiFile.getFileName());
        return;
    }

    // Multiple channels: show selection dialog
    showMidiChannelDialog (midiFile, info);
}
```

### 4.4 Helper: applyMidiImport

```cpp
void OpenVoxTunerAudioProcessorEditor::applyMidiImport (
    const ovtdsp::PitchCurve& newCurve,
    const juce::String& sourceName)
{
    if (curveEditor == nullptr) return;
    if (newCurve.getNumPoints() < 2)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            "Import MIDI",
            "The MIDI file contains fewer than 2 notes. "
            "A valid curve requires at least 2 points.");
        return;
    }

    // Register undo snapshot
    curveEditor->beginTransaction ("Import MIDI: " + sourceName);

    // Apply the curve
    curveEditor->setCurve (newCurve);

    // Enable step mode (MIDI notes are discrete, not continuous)
    curveEditor->setStepModeEnabled (true);

    // Notify processor
    notifyChanged();
}
```

---

## 5. Step 3 -- Multi-Channel Selection Dialog

**Files:** `Source/PluginEditor.h` (MODIFY), `Source/PluginEditor.cpp` (MODIFY)

### 5.1 Dialog component (private helper in PluginEditor.cpp)

A simple component shown via `juce::DialogWindow`:

```cpp
struct MidiChannelSelectorComponent : public juce::Component
{
    MidiChannelSelectorComponent (
        const ovtdsp::MidiImportInfo& info,
        std::function<void(ovtdsp::MidiExtractStrategy, int)> onConfirm)
        : importInfo (info), confirmCallback (std::move (onConfirm))
    {
        setSize (400, 280);

        // Strategy combo
        strategyCombo.addItem ("Highest note (lead melody)", 1);
        strategyCombo.addItem ("Lowest note (bass line)", 2);
        strategyCombo.addItem ("Loudest note (by velocity)", 3);
        strategyCombo.addItem ("Specific MIDI channel", 4);
        strategyCombo.setSelectedId (1, juce::dontSendNotification);
        strategyCombo.onChange = [this] { updateChannelVisibility(); };
        addAndMakeVisible (strategyCombo);

        // Channel combo (populated from info)
        for (const auto& ch : info.channels)
        {
            channelCombo.addItem (
                "Channel " + juce::String (ch.channel)
                + " (" + juce::String (ch.numNotes) + " notes)",
                ch.channel);
        }
        if (info.channels.size() > 0)
            channelCombo.setSelectedId (info.channels.getFirst().channel,
                                        juce::dontSendNotification);
        addAndMakeVisible (channelCombo);

        // Buttons
        importButton.setButtonText ("Import");
        importButton.onClick = [this] { onImportClicked(); };
        addAndMakeVisible (importButton);

        cancelButton.setButtonText ("Cancel");
        cancelButton.onClick = [this] {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState (0);
        };
        addAndMakeVisible (cancelButton);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff2d2d2d));
        g.setColour (juce::Colours::white);
        g.setFont (14.0f);
        g.drawText ("Strategy:", 16, 20, 80, 24, juce::Justification::centredLeft);
        g.drawText ("Channel:", 16, 60, 80, 24, juce::Justification::centredLeft);
    }

    void resized() override
    {
        strategyCombo.setBounds (100, 20, 280, 24);
        channelCombo.setBounds (100, 60, 280, 24);
        cancelButton.setBounds (210, 230, 80, 30);
        importButton.setBounds (300, 230, 80, 30);
    }

private:
    void updateChannelVisibility()
    {
        bool showChannel = (strategyCombo.getSelectedId() == 4);
        channelCombo.setVisible (showChannel);
    }

    void onImportClicked()
    {
        auto strategy = static_cast<ovtdsp::MidiExtractStrategy> (
            strategyCombo.getSelectedId() - 1);
        int channel = channelCombo.getSelectedId();
        confirmCallback (strategy, channel);

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (1);
    }

    ovtdsp::MidiImportInfo importInfo;
    std::function<void(ovtdsp::MidiExtractStrategy, int)> confirmCallback;
    juce::ComboBox strategyCombo, channelCombo;
    juce::TextButton importButton, cancelButton;
};
```

### 5.2 Show dialog

```cpp
void OpenVoxTunerAudioProcessorEditor::showMidiChannelDialog (
    const juce::File& midiFile,
    const ovtdsp::MidiImportInfo& info)
{
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Import MIDI: " + midiFile.getFileName();
    opts.component = new MidiChannelSelectorComponent (
        info,
        [this, midiFile] (ovtdsp::MidiExtractStrategy s, int ch)
        {
            auto curve = ovtdsp::MidiImporter::importFrom (midiFile, s, ch);
            applyMidiImport (curve, midiFile.getFileName());
        });
    opts.useNativeTitleBar = true;
    opts.escapeKeyTriggersCloseButton = true;
    opts.resizable = false;
    opts.launchAsync();
}
```

---

## 6. Step 4 -- Hamburger Menu "Import MIDI" Item

**Files:** `Source/PluginEditor.cpp` (MODIFY)

Add a menu item in the Curve Editor options menu section. This is the section
that currently contains "Snap to Scale", "Step Mode", etc.

```cpp
// In the hamburger menu building code, after the existing Curve Editor options:

menu.addSeparator();

// Import MIDI (Curve Editor only)
{
    menu.addItem (ovt::tr(ovt::Keys::kMenuImportMidi), true, false, [this] {
        // Only allow import when on Curve Editor tab
        if (tabbedComponent.getCurrentTabIndex() != 1)
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::AlertWindow::WarningIcon,
                "Import MIDI",
                "Please switch to the Curve Editor tab first.");
            return;
        }

        // Launch file chooser
        auto fileChooserFlags = juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles;

        fileChooser = std::make_unique<juce::FileChooser> (
            "Select a MIDI file to import",
            juce::File(),
            "*.mid;*.midi");

        fileChooser->launchAsync (fileChooserFlags,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    // Reuse the same pipeline as filesDropped
                    filesDropped ({ file.getFullPathName() }, 0, 0);
                }
            });
    });
}
```

### 6.1 FileChooser member

In `PluginEditor.h`, add:
```cpp
std::unique_ptr<juce::FileChooser> fileChooser;
```

---

## 7. Step 5 -- PitchCurveEditor Integration

**Files:** `Source/ui/PitchCurveEditor.h` (MODIFY), `Source/ui/PitchCurveEditor.cpp` (MODIFY)

### 5.1 New public method

In `PitchCurveEditor.h`:
```cpp
/** Import a pitch curve from an external source (e.g. MIDI file).
 *  Records an undo snapshot so the import can be undone with Ctrl+Z. */
void importMidiCurve (const ovtdsp::PitchCurve& newCurve);
```

In `PitchCurveEditor.cpp`:
```cpp
void PitchCurveEditor::importMidiCurve (const ovtdsp::PitchCurve& newCurve)
{
    // Register undo state before modifying
    CurveEditAction* action = new CurveEditAction (this);
    curve = newCurve;
    action->setAfter();
    registerUndoableAction (action);

    // Force step mode for MIDI (discrete notes, not continuous)
    curve.setStepMode (true);

    // Reset snap and grid for clean editing after import
    curve.setSnapEnabled (true);
    curve.setSnapToGridEnabled (true);
    snapEnabled = true;
    snapToGridEnabled = true;

    // Auto-adjust view to fit the imported curve
    if (curve.getNumPoints() >= 2)
    {
        const double lastTime = curve.getPoint (curve.getNumPoints() - 1).time;
        const double firstTime = curve.getPoint (0).time;
        const double span = lastTime - firstTime;

        // Adjust measures to fit (minimum 2, maximum 32)
        int needed = static_cast<int> (std::ceil (span / 4.0)) + 1;
        needed = juce::jlimit (2, 32, needed);
        setMeasuresVisible (needed);

        // Scroll to start
        scrollOffset = 0.0;
    }

    repaint();
    notifyChanged();
}
```

---

## 8. Step 6 -- Undo Support & Step Mode

The undo is handled through the existing `CurveEditAction` mechanism in
`PitchCurveEditor`. The `importMidiCurve` method (Step 5) already captures
the before/after states, so Ctrl+Z / Ctrl+Y will work automatically.

Step mode is forced ON because MIDI notes are discrete (note-on at time T,
note-off at time T+d), which maps naturally to the step/palier interpolation
of the PitchCurve. Linear interpolation between imported notes would create
unwanted glissando artifacts.

---

## 9. Step 7 -- Internationalization (i18n)

**Files:** `Source/ui/OVTLanguages.h` (MODIFY)

Add the following language keys and translations:

| Key | EN | FR | DE | ES | JA | ZH |
|-----|----|----|----|----|----|----|
| `kMenuImportMidi` | "Import MIDI..." | "Importer MIDI..." | "MIDI importieren..." | "Importar MIDI..." | "MIDIをインポート..." | "导入MIDI..." |
| `kDialogTitleMidiImport` | "Import MIDI" | "Importer MIDI" | "MIDI importieren" | "Importar MIDI" | "MIDIインポート" | "导入MIDI" |
| `kMidiErrorCannotOpen` | "Cannot open file" | "Impossible d'ouvrir le fichier" | "Datei kann nicht geoeffnet werden" | "No se puede abrir el archivo" | "ファイルを開けません" | "无法打开文件" |
| `kMidiErrorInvalidFormat` | "Invalid MIDI file format" | "Format de fichier MIDI invalide" | "Ungueltiges MIDI-Dateiformat" | "Formato de archivo MIDI no valido" | "無効なMIDIファイル形式" | "无效的MIDI文件格式" |
| `kMidiErrorTooFewNotes` | "The MIDI file contains fewer than 2 notes" | "Le fichier MIDI contient moins de 2 notes" | "Die MIDI-Datei enthaelt weniger als 2 Noten" | "El archivo MIDI contiene menos de 2 notas" | "MIDIファイルに2音未満です" | "MIDI文件包含少于2个音符" |
| `kMidiErrorWrongTab` | "Please switch to the Curve Editor tab first" | "Veuillez d'abord basculer sur l'onglet Editeur de courbes" | "Bitte wechseln Sie zuerst zum Kurven-Editor-Tab" | "Por favor, cambie primero a la pestana Editor de curvas" | "まずカーブエディタタブに切り替えてください" | "请先切换到曲线编辑器选项卡" |
| `kMidiStrategyHighest` | "Highest note (lead melody)" | "Note la plus aigue (melodie principale)" | "Hoechste Note (Hauptmelodie)" | "Nota mas aguda (melodia principal)" | "最も高い音（主旋律）" | "最高音（主旋律）" |
| `kMidiStrategyLowest` | "Lowest note (bass line)" | "Note la plus basse (ligne de basse)" | "Tiefste Note (Basslinie)" | "Nota mas grave (linea de bajo)" | "最も低い音（ベースライン）" | "最低音（贝斯线）" |
| `kMidiStrategyLoudest` | "Loudest note (by velocity)" | "Note la plus forte (par velocite)" | "Laeuteste Note (nach Velocity)" | "Nota mas fuerte (por velocidad)" | "最も強い音（ベロシティ）" | "最强音（按力度）" |
| `kMidiStrategyChannel` | "Specific MIDI channel" | "Canal MIDI specifique" | "Bestimmter MIDI-Kanal" | "Canal MIDI especifico" | "特定のMIDIチャンネル" | "指定MIDI通道" |
| `kMidiBtnImport` | "Import" | "Importer" | "Importieren" | "Importar" | "インポート" | "导入" |

The key declarations should be added in the `ovt::Keys` namespace alongside
existing keys. The translations follow the existing pattern in OVTLanguages.h.

---

## 10. Step 8 -- Documentation & Roadmap Update

**Files:** `docs/implementation-roadmap.md` (MODIFY), `docs/changelog-2026-07-31.md` (CREATE/MODIFY)

### Roadmap addition

Add to `docs/implementation-roadmap.md` in a new section:

```markdown
## 4b. UI / GUI - Curve Editor: MIDI Import

- [ ] **MidiImporter DSP module**: Analyze and convert .mid files to PitchCurve
      (`MidiImporter.h/.cpp`, `MidiImportInfo`, `MidiExtractStrategy`, `analyzeFile`, `importFrom`)
- [ ] **Drag-and-drop file import**: `FileDragAndDropTarget` on PluginEditor,
      accepts .mid/.midi files, generates pitch curve in Curve Editor
- [ ] **Menu import**: "Import MIDI..." item in Curve Editor hamburger menu,
      launches juce::FileChooser filtered on *.mid
- [ ] **Multi-channel selection dialog**: UI popup when MIDI file contains multiple
      active channels (strategy selection: highest/lowest/loudest/channel)
- [ ] **Polyphonic reduction**: Convert chord/polyphonic MIDI content to a single
      pitch curve line using configurable strategy
- [ ] **Undo support**: Import is registered as an undoable action (Ctrl+Z)
- [ ] **Step mode auto-enable**: Step mode activated after MIDI import (discrete notes)
- [ ] **Auto-fit view**: Measures and scroll adjusted to fit imported curve duration
- [ ] **i18n**: All dialog strings translated (EN/FR/DE/ES/JA/ZH)
- [ ] **Validation**: Monophonic, polyphonic, multi-channel, invalid files, DnD + menu
```

---

## 11. Files Summary

| File | Action | Step | Lines Est. |
|------|--------|------|------------|
| `Source/dsp/MidiImporter.h` | **CREATE** | 1 | ~80 |
| `Source/dsp/MidiImporter.cpp` | **CREATE** | 1 | ~220 |
| `Source/PluginEditor.h` | **MODIFY** | 2,3,4 | +25 |
| `Source/PluginEditor.cpp` | **MODIFY** | 2,3,4 | +150 |
| `Source/ui/PitchCurveEditor.h` | **MODIFY** | 5 | +5 |
| `Source/ui/PitchCurveEditor.cpp` | **MODIFY** | 5 | +30 |
| `Source/ui/OVTLanguages.h` | **MODIFY** | 7 | +60 |
| `CMakeLists.txt` | **MODIFY** | 1 | +2 |
| `docs/implementation-roadmap.md` | **MODIFY** | 8 | +15 |
| `docs/changelog-2026-07-31.md` | **MODIFY** | 8 | +10 |
| **Total** | | | **~597 lines** |

---

## 12. Testing & Validation

| # | Test Case | How to Verify | Expected Result |
|---|-----------|--------------|-----------------|
| 1 | Drag monophonic .mid (1 channel, single notes) onto plugin | Drop file on plugin window while on Curve Editor tab | Curve generated with points matching each MIDI note |
| 2 | Drag polyphonic .mid (chords) onto plugin | Drop file containing simultaneous notes | Selection dialog appears; importing "Highest" gives lead melody |
| 3 | Drag multi-channel .mid onto plugin | Drop file with channels 1 and 2 active | Selection dialog shows both channels; selecting Channel 1 imports only that channel |
| 4 | Drag .mid with percussion (channel 10) | Drop file with drums + melody | Percussion channel excluded; melody imported directly |
| 5 | Drag invalid/non-MIDI file | Drop a .txt or corrupt file | Error message "Invalid MIDI file format" |
| 6 | Drag .mid when on Live tab | Drop file while on Live tab | Warning: "Please switch to Curve Editor tab" |
| 7 | Import via hamburger menu | Click Options > Import MIDI... > select .mid file | Same result as drag-and-drop |
| 8 | Undo after import (Ctrl+Z) | Import MIDI, then press Ctrl+Z | Curve reverts to pre-import state |
| 9 | Step mode after import | Import MIDI, check step mode button | Step mode is ON (toggle shows active) |
| 10 | View auto-fit | Import a long .mid file (32 beats) | Measures auto-adjusted to show full curve |
| 11 | .midi extension | Drag a file with .midi extension | Accepted and imported correctly |
| 12 | Empty MIDI file (no notes) | Drop a .mid with 0 note events | Error: "fewer than 2 notes" |
| 13 | Very short MIDI (1 note) | Drop a .mid with 1 note | Error: "fewer than 2 notes" |
| 14 | MIDI with note-on but no note-off | Drop truncated MIDI | Notes handled gracefully (default end time) |
| 15 | Import same file twice | Import, then import again | Curve replaced; undo reverts to previous import |

---

## 13. Success Criteria

| # | Criterion | Pass Condition |
|---|-----------|----------------|
| 1 | Monophonic import | Points match source MIDI note count and frequencies |
| 2 | Polyphonic reduction (highest) | Correct lead melody extracted from chords |
| 3 | Polyphonic reduction (lowest) | Correct bass line extracted |
| 4 | Multi-channel selection | User can select any active channel |
| 5 | Channel 10 exclusion | Percussion notes never appear in curve |
| 6 | Undo (Ctrl+Z) | Import is fully reversible |
| 7 | Step mode | Automatically enabled post-import |
| 8 | View auto-fit | Curve visible without manual scroll/zoom |
| 9 | Error handling | Clear error messages for all failure modes |
| 10 | Drag-and-drop | Works on Windows (primary target) |
| 11 | Menu import | FileChooser works and triggers same pipeline |
| 12 | i18n | All strings translated in 6 languages |
| 13 | Performance | Import < 500 ms for files with < 1000 notes |
| 14 | No crashes | Invalid inputs produce errors, never crashes |
