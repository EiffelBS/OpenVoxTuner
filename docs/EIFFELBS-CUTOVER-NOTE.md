# OpenVoxTuner × eiffelbs-ui cutover notes

OpenVoxTuner now consumes the shared EiffelBS design system
([eiffelbs-ui](https://github.com/EiffelBS/eiffelbs-ui)) version **v0.12.28**
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
- **`NOMINMAX` was required from v0.12.26; it is not required by v0.12.27.** The
  v0.12.26 umbrella carried `Waveform.h` and `ThumbnailPool.h`, which need
  `juce::AudioThumbnail`, so they pulled `<juce_audio_utils/juce_audio_utils.h>`
  into every consumer TU. That compiled `juce_audio_devices`, whose
  `juce_UMPEndpoint.h` calls `std::min` **without** parentheses; the Windows
  `min`/`max` function-like macros expand that into `std::(...)` and MSVC fails
  (C2589 + C2059). OpenVoxTuner's TUs include `<windows.h>` without `NOMINMAX`
  (the ARA SDK and the VST3 plugin-client chain), so the define had to be carried
  by the `OpenVoxTuner` and `OpenVoxTunerTests` targets. JUCE sets `NOMINMAX`
  before its own `windows.h` include, which is why the library smoke stayed green
  while a consumer that reaches `windows.h` first did not. In v0.12.27 the two
  audio widgets are **opt-in includes**, so the library compiles no audio module
  in this project's TUs; OpenVoxTuner keeps `NOMINMAX` as protection for its own
  bare `<windows.h>` includes, and that is now a measured statement: the tests
  target builds and passes with the define removed.

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
- Active-tab colour `#FF1A9AF0`: **captured on the real widget, dark theme**
  (2026-10-09). The standalone was launched with
  `Invoke-CimMethod Win32_Process.Create` (`Start-Process` hangs for a
  workspace binary), located through UIA (`AutomationElement.FromHandle` -
  no tree walk, `RootElement.FindFirst` also hangs), brought in front by
  minimising the maximised browser for a few seconds with `WScript.Shell`
  (`AppActivate`, `SendKeys("% n")` then `"n"`, then `AppActivate` back to
  restore it), and grabbed with `Graphics.CopyFromScreen`. The full-screen
  PNG holds **6624 exactly `#FF1A9AF0` pixels** and **0 `#FF1565C0`** pixels,
  and the zoomed crop of the tab strip shows the active "Live" pill filled
  with that exact accent while "Curve Editor" stays unlit - the button is
  solid accent, no gradient, no local `brighter()` hack. Evidence stays out
  of git in `.dbg/`: `ovt-standalone-dark.png` (2056x1290) and
  `ovt-tab-pill-crop.png`. The user's `OpenVoxTuner.settings` was backed up
  before the captures and verified byte-identical afterwards (the standalone
  was terminated, so it never rewrote it).
  The **light** theme was then verified by the author on his own screen, which
  closes the only step that needed a human eye: on 2026-10-09 the agent
  relaunched the standalone for that purpose and he opened the gear menu ->
  Interface -> Theme -> Light himself. The active tab read the light accent
  `#FF1565C0` correctly and he reported the test OK. The light value is pinned
  twice over independently of that look - the smoke asserts that exact hex and
  `Theme.h` is byte-identical between the two pins - so this step confirms with
  the eye a value already measured, rather than measuring a new one.
- Consumer-context bug found while bumping (see the library fix in the bump
  log): OpenVoxTuner's translation units include `<windows.h>` **without**
  `NOMINMAX` (the ARA SDK and the VST3 plugin-client chain), so `std::max`
  and `std::numeric_limits<int>::max()` in four library header sites failed
  to compile (C4003 / C2589). Fixed in the library with the parenthesised
  call form, and the library smoke now compiles in that polluted context
  first - a green smoke on its own was no longer sufficient proof.

## Verification record (v0.12.26 bump)

- **Gap measured before the pin moved.** The pinned tag was `v0.12.7`
  (`f263048`, published 2026-10-09); upstream latest was `v0.12.26`
  (`d2dfd1a`) - **19 tags / 24 commits ahead**, `git diff --stat v0.12.7..v0.12.26`
  = 27 files, **+2713 / -33**. Nine components added (Avatar, ChatBubble,
  DisclosureChevron, ExpansibleFrame, IconTextButton, MenuChevronButton,
  PillTabSwitch, ThumbnailPool, Waveform), `DataList.h` +77, `IconButton.h`
  +120. The two-line edits in Theme / Fonts / LookAndFeel / Sidebar /
  StatusBar / ToggleSwitch are **comment-only** (`daa9a75`, "Scrub
  private-project names from the public tree").
- **No functional delta for OpenVoxTuner.** A grep over `Source/` finds zero
  references to any of the added components and none to `DataList`,
  `IconButton`, `LogWindow`, `Sidebar`, `HelpBubble`, `FramedBody`,
  `FileGripButton`, `ToggleSwitch`. The project consumes `ebs::LookAndFeel`,
  `ebs::Theme` (+ `setTheme` / `currentTheme` / `isDark`), `ebs::Knob`,
  `ebs::MorphSlider`, `ebs::PowerToggle` and the colour/font helpers - all
  unchanged in that range. The bump is a pin, not a feature.
- **The pin was decorative until the cache was cleared.** The local CMake cache
  held `FETCHCONTENT_SOURCE_DIR_EIFFELBS-UI:PATH` pointing at
  `.dbg\eiffelbs-ui-v0127`, and that override **outranks** `GIT_TAG`: a build
  configured against the stale copy compiles that copy. Cleared with
  `cmake -B build "-DFETCHCONTENT_SOURCE_DIR_EIFFELBS-UI="` (EXIT 0), FetchContent
  cloned `v0.12.26` into `build/_deps/eiffelbs-ui-src`; `git describe` there
  reads **v0.12.26**, so the pin is now authoritative and public.
- **First build after the pin was RED, and the green test run that followed was
  a lie.** A failed link leaves the previous exe on disk, so running it printed
  `Result: 136 OK, 0 FAILED` from a stale binary. Check the build tail before
  trusting any green run.
- **The red was the Windows macro trap, one level deeper than the v0.12.7 fix.**
  `juce_UMPEndpoint.h(86)` calls `std::min` unparenthesised; with `windows.h`
  macros live MSVC reports C2589 + C2059 in `Source/PluginProcessor.cpp` and in
  `test/Main.cpp`. JUCE's own source cannot be patched here (no fork), so the
  consumer kills the macros: `NOMINMAX` added to `OpenVoxTuner` and
  `OpenVoxTunerTests`. Side effect, all good: the hand-written `max`-macro
  workaround in `Source/dsp/BlockAwareOnePole.h` is now dead protection.
- **Fresh-build verification (2026-10-15).** `OpenVoxTunerTests` rebuilt and run
  in-workspace under the current full-access file policy: **136 OK, 0 FAILED**,
  exit 0. Full tree (`cmake --build build --config Release`, every target) green
  with **no MSVC warnings**. The standalone was relaunched and captured at
  `.dbg/ui_check_v01226.png` (1102x628, `PrintWindow` flag 2): the Effects block
  renders row 1 Gate / Compress / Reverb / Formant and row 2 Doubler / De-Esser,
  dark theme, knobs and keyboard intact.
- **Commit `2512651`** (CMakeLists only: pin + two `NOMINMAX` defines + the
  cache-override comment). Local on `chore/eiffelbs-ui-0.12.7`, **not pushed**.
- **Open thread, upstream — CLOSED on 2026-10-15:** the umbrella dragged an audio
  module into every consumer TU of a header-only design system. Keeping
  `Waveform.h` and `ThumbnailPool.h` out of `eiffelbs.h` as opt-in includes
  removes that tax for every plugin project. Done: library commit `1d32bf4`, tag
  `v0.12.27`, pushed; see the record below.

## Verification record (v0.12.27 bump)

- **The fix is upstream, not in this repository.** Library commit `1d32bf4`
  ("Umbrella: the two audio widgets are opt-in, not in eiffelbs.h") removes
  `Waveform.h` and `ThumbnailPool.h` from `eiffelbs.h`, with the reason written at
  the top of the umbrella and repeated at the top of both headers. A host that
  wants them includes the header directly and links `juce::juce_audio_utils`. Tag
  `v0.12.27` (`1d32bf4`) was pushed to `origin/main` together with the commit; the
  library's `project(... VERSION ...)` finally reads `0.12.27`, where it had been
  reading `0.12.7` since that release.
- **Library smoke: PASS, 0 failures**, built and run against the local JUCE 8.0.8
  checkout (`-DEIFFELBS_UI_JUCE_DIR=C:/JUCE -DEIFFELBS_UI_BUILD_SMOKE_TEST=ON`).
  The suite never touched `Waveform` or `ThumbnailPool`, so it now compiles
  strictly less than before and the polluted-context proof (the smoke includes
  `<windows.h>` bare, with no `NOMINMAX`, before the umbrella) still holds. PNG
  receipt `%TEMP%\eiffelbs-ui-smoke.png` (9369 bytes).
- **OpenVoxTuner repinned to `v0.12.27`.** FetchContent re-populated:
  `git describe` in `build/_deps/eiffelbs-ui-src` reads **v0.12.27**, and the
  populated umbrella contains no `Waveform.h` / `ThumbnailPool.h` include. A grep
  over `Source/` finds zero includes of those two headers — every `waveform` hit in
  this project is OVT's own ARA overlay (`ovt::drawWaveformOverlay`, the
  `ui_show_waveform` / `ui_waveform_type` parameters), which lives in
  `Source/ui/OVTTheme.h` and needs only `juce_dsp`.
- **The consumer-side mitigation is now measured, not assumed.** The
  `NOMINMAX` define was removed from `OpenVoxTunerTests`, the tree reconfigured
  and rebuilt: **green**, and the freshly built exe ran **136 OK, 0 FAILED** (exit
  0). So `eiffelbs-ui` no longer forces the define on this project. It was then
  restored — OVT's own TUs include `<windows.h>` bare through the ARA SDK and the
  VST3 plugin-client chain, so killing the macros stays cheap protection — and the
  full tree rebuilt green with **no MSVC warnings**, tests re-run fresh at
  **136 OK, 0 FAILED**.
- **Standalone UI captured on the new pin:** `.dbg/ui_check_v01227.png`
  (1102x628, `PrintWindow` flag 2, PID 102024). Dark theme; Effects row 1 Gate /
  Compress / Reverb / Formant, row 2 Doubler / De-Esser; keyboard, piano, Morph
  strip and tabs intact. The PNG is **byte-identical in size** to the v0.12.26
  capture (60262 bytes), consistent with zero functional delta: the bump removed a
  compile-time cost, not a widget.
- **OVT commit is local only** (pin + the two `NOMINMAX` comments), on
  `chore/eiffelbs-ui-0.12.7`, per the standing decision not to push this
  repository.

## Verification record (v0.12.28 bump)

- **This bump is a feature, not a pin.** Library commit `7c374e9` ("IconButton:
  viewer-toolbar glyph set, ported from OpenVoxTuner") adds **11 shapes** to
  `ebs::IconButton`: `zoomIn`, `zoomOut`, `chevronUp`, `chevronDown`, `chevronLeft`,
  `chevronRight`, `grid`, `step`, `power`, `trash`, `save`. The Feather SVG strings are
  verbatim from OpenVoxTuner's local drawable table, so the glyphs this plugin already
  drew are now shared. `IconButton.h` 540 â†’ **558** lines (92+/2âˆ’). `tests/Smoke.cpp`
  L234â€“258 renders **the whole 39-shape enum** with the comment "The WHOLE enum: a new
  shape cannot ship without an ink check", and the gate asserts every shape paints
  visible ink: **PASS, 0 failures**, receipt `%TEMP%\eiffelbs-ui-smoke.png`
  (18738 bytes). Commit + lightweight tag `v0.12.28` pushed to `origin/main`;
  `git ls-remote --tags` confirms `refs/tags/v0.12.28 = 7c374e9b735d8d8673b83ff501b64893afd59956`.
- **The repin is authoritative.** `GIT_TAG v0.12.28` in `CMakeLists.txt`, the stale
  `FETCHCONTENT_SOURCE_DIR_EIFFELBS-UI` cache override cleared, and `git describe`
  inside `build/_deps/eiffelbs-ui-src` reads **v0.12.28**.
- **Five consumer swaps, all agreed in advance.**

  | Widget | Site | Note |
  | --- | --- | --- |
  | `ebs::PillTabSwitch` | Live / Curve Editor tabs | Replaces the local pill widget; self-measured width, wheel scroll. |
  | `ebs::IconButton` | 14 buttons: toolbar zoom/scroll/reset, global undo/redo, preset prev/next/save, Options, Preset Gallery, menu, rewind, Curve Editor chips | Dead hook `refreshDrawableButtonIcons()` deleted; SVG table trimmed to the six strings still drawn. |
  | `ebs::DisclosureChevron` | Advanced chevron | `VerticalTextButtonLF` (triangle from `getToggleState()`) deleted; host keeps owning `advancedExpanded`. |
  | `ebs::ToggleSwitch` | `useVoiceButton` | Drop-in typing for `juce::ToggleButton`; caption label kept to its left. |
  | `ebs::LogView` | Debug window MIDI log half | Tail-follow + drop counter, same 10 Hz refresh from `proc.getLastSentMidiNoteForChannel(1..9)`. |
  | `ebs::sanitizeForShaping` | Preset combo + gallery cards | Displayed names only; the raw name stays the lookup key. |

- **Six buttons stay `juce::DrawableButton` deliberately**: bypass, midiOut, snap,
  snapGrid, stepMode, play. They paint a solid accent pill when ON and
  `ebs::IconButton` has no accent-fill ON background, so swapping them would delete the
  active-state signal.
- **Build and tests:** `OpenVoxTuner` SharedCode target green (**0 errors**);
  `OpenVoxTunerTests` rebuilt and run â€” **136 OK / 0 FAILED**, exit 0. Standalone linked
  and captured at `.dbg/ui_check_swaps.png` (1102x628, `PrintWindow` flag 2, PID 88220):
  pill, five toolbar glyphs, gallery grid icon, global undo/redo, settings gear, collapsed
  Advanced chevron and the Use Voice switch all render on the real widget.
- **Owed to a human click:** Curve Editor chips, the expanded Advanced panel and the Debug
  window need a mouse; synthetic input is blocked on this machine, so those three surfaces
  are verified by compile + code review only.
- **OVT commits stay local** on `chore/eiffelbs-ui-0.12.7`, per the standing decision not
  to push this repository.

## Bump log (append a line per bump)

| Date | Pin | Notes |
| --- | --- | --- |
| 2026-08-27 | v0.4.2 | Cutover pin; note drifted (announced v0.3.0). |
| 2026-10-08 | v0.12.7 | Tag resolved with `git ls-remote` unavailable (`SEC_E_NO_CREDENTIALS`) via the GitHub tags API: v0.12.6 (`2615cb9`) was the last published tag; v0.12.7 was the next local release. README of the library refreshed first (it had not been updated for several versions), then the Windows `min`/`max` macro fix (`f263048`) was folded into the same release and the tag re-pointed. Offline builds used `-DFETCHCONTENT_SOURCE_DIR_EIFFELBS-UI=.dbg\eiffelbs-ui-v0127` (a clean `git archive` export of the tag). Build cost: 118 s + 113 s incremental Release (Visual Studio 17 2022; MSBuild had skipped the two largest TUs on the first pass - `PluginEditor.obj`, `PluginProcessor.obj` - so their objects were deleted and forced). The bump needs **no** OVT code change beyond the pin itself: see the toggle constat above. Published 2026-10-09: `git push origin v0.12.7` (tag only - the remote `main` deliberately stayed at `2615cb9`, an untagged later commit by another session was NOT carried). Costs of the published-pin proof: 24 s configure + 78 s test build, 112/112 again. |
| 2026-10-15 | v0.12.26 (`d2dfd1a`) | Pin moved 19 tags / 24 commits. Nine components added, `DataList.h` +77, `IconButton.h` +120; the Theme/Fonts/LookAndFeel/Sidebar/StatusBar/ToggleSwitch edits are comment-only (`daa9a75`). OpenVoxTuner consumes none of the new widgets, so the only code change is the pin plus `NOMINMAX`. The umbrella gained `Waveform.h` + `ThumbnailPool.h`, which pull `<juce_audio_utils>` and compile `juce_audio_devices` in every consumer TU; JUCE's unparenthesised `std::min` in `juce_UMPEndpoint.h(86)` then broke `Source/PluginProcessor.cpp` and `test/Main.cpp` (C2589 + C2059). Fixed by defining `NOMINMAX` on `OpenVoxTuner` and `OpenVoxTunerTests`. The stale local `FETCHCONTENT_SOURCE_DIR_EIFFELBS-UI` override was cleared so the `GIT_TAG` pin is authoritative. Fresh build: 136/136 tests, full tree green with no warnings, standalone UI captured in `.dbg/ui_check_v01226.png`. OVT commit `2512651`, local only. |
| 2026-10-15 | v0.12.27 (`1d32bf4`) | Upstream fix of the v0.12.26 coupling, made in the library: `Waveform.h` and `ThumbnailPool.h` are **out of the umbrella** `eiffelbs.h` and are opt-in includes, because they pull `<juce_audio_utils>` and compile `juce_audio_devices` in every consumer TU of a header-only design system. Library smoke PASS 0 failures against the local JUCE 8.0.8 checkout; commit + tag pushed to `origin`. OVT repinned: FetchContent re-populated to v0.12.27, `OpenVoxTunerTests` built and ran **136/136** with `NOMINMAX` **removed** (the proof that the library no longer forces it), define then restored, full tree green with no MSVC warnings, standalone captured in `.dbg/ui_check_v01227.png`. OVT commit local only. |
| 2026-10-15 | v0.12.28 (`7c374e9`) | Upstream feature contributed from this project: 11 `IconButton` shapes (zoomIn / zoomOut / chevronUp / chevronDown / chevronLeft / chevronRight / grid / step / power / trash / save), Feather strings verbatim from OVT's drawable table; `IconButton.h` 540 â†’ 558 lines; smoke now renders the whole 39-shape enum (PASS 0 failures, receipt 18738 bytes). Commit + tag pushed. Consumer: repinned, five swaps applied â€” `PillTabSwitch`, 14 `IconButton`s (dead hook `refreshDrawableButtonIcons()` deleted), `DisclosureChevron` for the Advanced chevron, `ToggleSwitch` on `useVoiceButton`, `LogView` for the Debug MIDI log, plus `ebs::sanitizeForShaping` on displayed preset names. Six accent-pill ON buttons kept as `juce::DrawableButton` (IconButton has no accent-fill ON state). Build green 0 errors, tests 136/136, standalone captured `.dbg/ui_check_swaps.png`. OVT commits local only. |

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
