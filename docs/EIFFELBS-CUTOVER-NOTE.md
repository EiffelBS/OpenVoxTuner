# OpenVoxTuner × eiffelbs-ui cutover notes

OpenVoxTuner now consumes the shared EiffelBS design system
([eiffelbs-ui](https://github.com/EiffelBS/eiffelbs-ui)) version **v0.12.7**
through CMake `FetchContent`, exactly like OpenTimbre. There are no local
shims: the plugin links the library and calls `ebs::` directly.

(Version drift history, for the record: this note used to announce v0.3.0
while the pin had silently moved to v0.4.2. The pinned version above is now
kept exact on every bump - the last entry of the bump log below is always
the live pin.)

## What the link provides

`target_link_libraries(... eiffelbs-ui)` carries onto every consumer target:

- the ABI-critical JUCE defines shared by BOTH plugin binaries inside one
  DAW process (`OpenVoxTuner` **and** its companion `OpenVoxKey`):
  `JUCE_STRING_UTF_TYPE=8`, `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`;
- `/std:c++20` (library `cxx_std_20`) and MSVC `/utf-8`;
- **since v0.12.7**, `juce::juce_gui_extra` (the library's LogWindow lives
  there). Every target that links `eiffelbs-ui` must also list
  `juce::juce_gui_extra` in its own module list, otherwise configuration
  fails with `target juce::juce_gui_extra not found`. The three OpenVoxTuner
  targets (`OpenVoxTuner`, `OpenVoxKey`, `OpenVoxTunerTests`) all do; the
  `juce_gui_extra` entries in `CMakeLists.txt` carry a comment saying so -
  do not remove them as "unused".

## Offline / vendored builds

Point FetchContent at a local checkout instead of GitHub:

```sh
cmake -B build -DFETCHCONTENT_SOURCE_DIR_EIFFELBS-UI=C:\path\to\eiffelbs-ui
```

The variable name keeps the HYPHEN of the declared dependency name -
this is intentional and is a recurring typo trap.

Two operational traps met during the v0.12.7 bump:

- Changing the value of `FETCHCONTENT_SOURCE_DIR_EIFFELBS-UI` (or the
  `GIT_TAG`) does **not** re-populate an already-populated FetchContent
  entry. The build keeps consuming the stale `build/_deps/eiffelbs-ui-src`
  copy. Delete that directory (and the `-subbuild` sibling) and re-run
  configure so the override takes effect.
- With the override set, FetchContent uses the source directory **directly**
  and `build/_deps/eiffelbs-ui-src` disappears - that is expected, the
  include paths then point at the override directory.

## Migration shape

| Legacy | Now |
| --- | --- |
| `ovt::` colour/font helpers (~200 references) | `ebs::` equivalents (byte-identical palette, proven by the library smoke suite) |
| `ui::OVTLookAndFeel` (456-line LookAndFeel) | deleted; replaced by ~20-line local subclass `OvtLookAndFeel : ebs::LookAndFeel` |
| Magic-name power buttons (`setName("PowerButton")`) | typed `ebs::PowerToggle` widgets (8 live instances) |
| Name-keyed Morph painting in the L&F | self-painting `ebs::MorphSlider` |
| 23 rotary sliders | `ebs::Knob` (per-site style setters still win where text boxes are used) |
| 11 zero-use theme tokens | deleted (`inputColour`, `harmonyColour`, `piano*`, `headerBg/headerAccent`, `fontTarget`, `fontCents`, `fontCurveHelp`) |

### Kept deliberately local

- i18n system (`ovt::tr` / `ovt::Keys` / languages) - app concern;
- `WaveformDisplayType` + `drawWaveformOverlay` - depends on `juce_dsp` FFT,
  too heavy for a design-system header;
- app-specific font sizes in `OVTFonts.h`;
- widgets that are product UI rather than design-system surface
  (PresetGallery, Piano/Scale keyboards, Curve/Pitch visualisers).

## Pixel-parity strategy

Rendering must be identical to the pre-cutover build. The only legacy deltas
versus the library defaults are restored through the official theme hook -
no painter forks. Since the v0.4.2 pin (`33d4b44`, which dropped the
duplicated checkbox styling) the local subclass carries exactly TWO
overrides:

```cpp
struct OvtLookAndFeel final : ebs::LookAndFeel
{
    juce::Colour widgetThemeColour (int id) override
    {
        switch (id)
        {
            // Only the legacy solid-accent tab pill stays app-specific;
            // checkbox wells now come from the library default (v0.4.1+).
            case tabActiveFillColourId:   return ebs::accent();             // solid pill vs soft tint
            case tabActiveTextColourId:   return juce::Colours::white;
            default:                      return {};
        }
    }
};
```

Everything else inherits stock `ebs::LookAndFeel` painting.

## Toggle-button contract (`55c43ba`): OpenVoxTuner constat

Since v0.7.2 a `TextButton` that toggles resolves its ON fill in three
levels: an explicit `buttonOnColourId` wins outright, a chip that only set
`buttonColourId` keeps its exact fill (an accent outline is the active-state
signal), and a bare button switches to the solid accent. **No local
`brighter()` hack may re-implement it.**

Audited against the bump target - OpenVoxTuner exposes **no site that
changes rendering under this contract**, so the bump needed no button code
change:

| Site | Kind | Why it is not affected |
| --- | --- | --- |
| `updateButton` (`PluginEditor.cpp`) | `TextButton`, not a toggle | Only a click command; no ON state is painted. |
| `buttonA` / `buttonB` (AB choice) | `TextButton` with full `paint()` override | The library L&F is never called; its `setColour` lines are dead code. |
| `snapButton` / `snapGridButton` / `stepModeButton` | `juce::DrawableButton` | Not a `TextButton`; painted project-locally. |
| `pianoRollButton` (`PitchCurveEditor.cpp`) | toggle chip | Already sets an explicit `buttonOnColourId` (`0xff4caf50`) - the contract's winning level. |
| `advancedButton` / `correctionModeButton` | toggle chips | Both already set `buttonOnColourId` explicitly. |
| `PianoKeyButton` (`ScaleKeyboardComponent.cpp`) | `juce::ToggleButton` | Not a `TextButton`. |

The one toggle that matters, the Piano Roll chip, is pinned by
`test/ui/LookAndFeelToggleTest.cpp` (rest state: zero green pixels; ON
state: more than 400 green pixels through the real painting path).

## Verification record (v0.12.7 bump)

- Library smoke: **PASS, 0 failures** (includes the previously red
  "explicit TextButton fill survives the toggled state" case and the accent
  hexes `#FF1A9AF0` dark / `#FF1565C0` light).
- OVT builds: `OpenVoxTuner` (VST3 + Standalone), `OpenVoxKey` (VST3),
  `OpenVoxTunerTests` - all clean, **0 warnings, 0 errors**, offline
  (`FETCHCONTENT_SOURCE_DIR_EIFFELBS-UI` pointing at the v0.12.7 export).
  Incremental wall cost of the bump: 118 s, then 113 s for the second pass
  (see the bump log below for why there were two).
- Unit tests: `OpenVoxTunerTests` **112/112 PASS** (the gate was 111/111 at
  the v0.4.2 pin; the extra one is `LookAndFeelToggleTest`, the Piano Roll
  contrast regression that shipped with the library's toggle contract).
- **The pin is now public and proven resolvable.** `v0.12.7` (`f263048`) was
  published to `origin` on 2026-10-09; `git clone --depth 1 --branch v0.12.7`
  returns exactly the tree that was compiled here (the only difference is the
  clone's own `.git/` directory). A second configure of this repository was
  then run **without** `FETCHCONTENT_SOURCE_DIR_EIFFELBS-UI`, so that
  FetchContent cloned the tag from GitHub like any external rebuilder would:
  configure 24 s, `OpenVoxTunerTests` build 78 s, and the same **112/112
  PASS** (exit 0). The promise the public pin makes is therefore verified, not
  assumed.
- Active-tab colour `#FF1A9AF0`: proven by construction and by pixels, not
  by an OS screenshot - `Theme.h` is byte-identical between v0.4.2 and
  v0.12.7 (`git diff` empty), the two overrides above are unchanged, and the
  library smoke asserts the exact accent hexes in both themes. An OS-level
  capture of the standalone was attempted (`PrintWindow` and ffmpeg
  `gdigrab`) and is not usable in this environment: the plugin window lives
  on an absent display and the sandbox blocks the tooling. The check stays
  open for a human eye on a real monitor.
- Consumer-context bug found while bumping (see the library fix in the bump
  log): OpenVoxTuner's translation units include `<windows.h>` **without**
  `NOMINMAX` (the ARA SDK and the VST3 plugin-client chain), so `std::max`
  and `std::numeric_limits<int>::max()` in four library header sites failed
  to compile (C4003 / C2589). Fixed in the library with the parenthesised
  call form, and the library smoke now compiles in that polluted context
  first - a green smoke on its own was no longer sufficient proof.

## Bump log (append a line per bump)

| Date | Pin | Notes |
| --- | --- | --- |
| 2026-08-27 | v0.4.2 | Cutover pin; note drifted (announced v0.3.0). |
| 2026-10-08 | v0.12.7 | Tag resolved with `git ls-remote` unavailable (`SEC_E_NO_CREDENTIALS`) via the GitHub tags API: v0.12.6 (`2615cb9`) was the last published tag; v0.12.7 was the next local release. README of the library refreshed first (it had not been updated for several versions), then the Windows `min`/`max` macro fix (`f263048`) was folded into the same release and the tag re-pointed. Offline builds used `-DFETCHCONTENT_SOURCE_DIR_EIFFELBS-UI=.dbg\eiffelbs-ui-v0127` (a clean `git archive` export of the tag). Build cost: 118 s + 113 s incremental Release (Visual Studio 17 2022; MSBuild had skipped the two largest TUs on the first pass - `PluginEditor.obj`, `PluginProcessor.obj` - so their objects were deleted and forced). The bump needs **no** OVT code change beyond the pin itself: see the toggle constat above. Published 2026-10-09: `git push origin v0.12.7` (tag only - the remote `main` deliberately stayed at `2615cb9`, an untagged later commit by another session was NOT carried). Costs of the published-pin proof: 24 s configure + 78 s test build, 112/112 again. |

### Running the test suite in this environment

A binary whose image sits inside the workspace can no longer write to
`%TEMP%` (sandbox rule observed 2026-09-28, and it breaks
`MidiImporterTest`'s temp-file round-trip, which then crashes the suite in
`importFrom` - an access violation on an empty curve, **not** a code
regression). Run the suite from outside the workspace:

```bat
copy build\OpenVoxTunerTests_artefacts\Release\OpenVoxTunerTests.exe %TEMP%\ovt-tests-run\
%TEMP%\ovt-tests-run\OpenVoxTunerTests.exe
```

## Release gate (do not forget)

The eiffelbs-ui repository must be flipped **PUBLIC** before any public
binary release of OpenVoxTuner that consumes it: external FetchContent
clones and AGPL source availability both depend on it. Verified on
2026-10-08 (anonymous GitHub API reads the tags and the README) - **the
gate is satisfied**; it stays listed so a future consumer re-checks it.
