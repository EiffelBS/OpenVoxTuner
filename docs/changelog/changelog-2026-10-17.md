# Changelog — 2026-10-17

## Fix — The output limiter is a true-peak limiter at −1 dBFS, not a sample-level ceiling at −0.175 dBFS

### Context

The user reported the impression that the limiter behaved "like a gain/volume booster
and did not stop clipping", and asked for the usual behaviour: a limiter that keeps the
signal from reaching 0 dBFS, with a ceiling around −1 dB (soft clip or brickwall). An audit
of `ovtdsp::OutputLimiter` measured what the stage actually did, at 48 kHz / blockSize 512.

The old header comment claimed the `ceilingLinear = 0.98f` ceiling was "~ −1.68 dBFS". It is not:
`20*log10(0.98) = -0.175 dBFS`. The measurements, with a 4x oversampled reconstruction filter as
the true-peak estimator (unnormalized sinc, phase groups summing to unity gain):

| Material through the limiter | sample peak | reconstructed true peak | overshoot |
|---|---|---|---|
| full-scale 200 Hz sine, trim +6 dB | −0.18 dBFS | 0.9800 = −0.18 dBFS | 0.00 dB |
| full-scale 1 kHz square, trim 0 dB | −0.18 dBFS | 1.2478 = **+1.92 dBFS** | **2.10 dB** |
| full-scale broadband noise burst, trim 0 dB | −0.18 dBFS | 1.6640 = **+4.42 dBFS** | **4.60 dB** |
| full-scale sine, trim +6 dB, limiter OFF | 1.9953 = **+6.00 dBFS** | — | — |

So a real brickwall did exist (instantaneous attack, channels linked, one-pole release), but it sat
at −0.175 dBFS **on stored samples only**. Broadband edges expose inter-sample overshoot: holding a
−1 dBFS true peak on the noise material needs a sample ceiling of 0.5248 (−5.60 dBFS). The "booster"
feeling is explained by the trim being applied *before* the limiter, so raising `output_trim` raises
the level up to the ceiling and the release pumping makes it read as volume.

The consented fix: a **fixed −1 dBFS ceiling applied to the reconstructed true peak**, plus a
**soft-clip safety stage after the gain**. No new user knob — the ceiling is not exposed.

### Changes

- **`Source/dsp/OutputLimiter.h`**: `ceilingLinear = 0.891f` (= −1 dBFS, honest comment). True-peak
  detection through a **65-tap** unnormalized sinc FIR at 4x oversampling (`kOversamplingFactor = 4`,
  odd tap count so the centre tap lands on the sample grid and the group delay is a whole number of
  base samples). A 33-tap window was tried first and **under-reported** the overshoot: it saw +2.4 dB
  where a wider estimator saw +4.4 dB, so closing at −1 dBFS against that number left true peaks above
  the ceiling. Polyphase decomposition: tap `k` contributes to output phase `p` when `k % 4 == p`,
  reading base sample `j = n - (k - p) / 4` — lag `(k - p) / kOversamplingFactor` behind the **newest**
  input sample, so phases 1..3 see samples up to the present and phase 0 sees `x[n]`. Phase groups are
  built once in `prepare()` (`std::vector<Tap>`, never in `process()`), unnormalized — sinc obeys the
  partition of unity `sum_j sinc(x + j) = 1`, so each phase group is unity gain and a full-scale
  bandlimited sine reconstructs at its own amplitude. Normalizing by the coefficient sum (~3.904) was
  the original bug: it attenuated by 4x, so the true peak read *below* the sample peak.
- **Emitted sample**: the phase-0 reconstruction sum `y_0(n) = x[n - kLatencySamples]`, i.e. the centre
  tap. The stage is therefore a delay of exactly `getLatencySamples()`: a crest entering at input index
  `S` leaves the output buffer at index `S + L`.
- **Delay line**: per-channel ring of 32 samples (`kHistoryMask = 31`), covering lags 0..16 behind the
  newest input. Zero allocation in the audio callback.
- **Gain target**: the gain closes on `targetLinear = ceilingLinear * kSafetyMargin` (0.975) =
  **-1.22 dBFS**, not on the ceiling. The detector reads the crest on the **input** side, while the soft
  clip shapes the **output** and adds ringing the detector cannot see (measured +0.19 dB on noise,
  +0.07 dB on a square): closing directly on `ceilingLinear` measured true peaks at −0.85 dBFS (square)
  and −0.31 dBFS (noise). With the margin, measured true peaks land below −1 dBFS.
- **Envelope**: instantaneous attack on the detected true peak, one-pole release (120 ms), channels linked.
  The dead `attackCoeff` (computed and never used) is gone; the comment now says what the code does.
  Meter accessors (`currentGain()`, `currentOutputPeak()`, `currentGainReductionDb()`) report the target-based
  gain, so the UI meter shows what the stage actually does.
- **Soft clip after the gain**: `u = |y| / ceiling`; transparent below the knee (`kneeFraction = 0.95`,
  engaging at ~ −1.4 dBFS), above it `ceiling * sign(u) * (k + (1 - k) * tanh((u - k)/(1 - k)))`. C1 continuous
  at the knee, bounded by the ceiling asymptote, applied only when the limiter is on. The knee was raised
  from 0.85 to 0.95 so the stage shapes less and limits residual ringing more.
- **Declared latency**: `getLatencySamples()` returns **8 samples** (~0.17 ms at 48 kHz) = `(65 - 1) / 2 / 4`.
  `applyLatencyMode()` declares `pitchShifter->getLatencySamples() + outputLimiter.getLatencySamples()`, so the
  host compensates it; the limiter's value is a constant, independent of the latency mode, and is never added
  twice (`setLatencySamples` is not called again in `prepareToPlay`).

### Tests

- **New `test/dsp/OutputLimiterTruePeakTest.cpp`** (registered in `test/Main.cpp`, 17 assertions): pins the
  ceiling at −1.00 dBFS ± 0.01; self-checks the estimator's phase sums against unity; measures sample peak vs
  true peak for a sine, a square wave and an LCG noise burst; asserts the reconstructed true peak stays at or
  below `ceilingLinear` after the fix; asserts that an impulse emerges exactly `getLatencySamples()` samples
  later with nothing non-zero before it. Verbatim from the green run (`Result: 180 OK, 0 FAILED`):

```
A: ceiling linear 0.8910 = -1.002 dBFS headroom
B: trim +6 dB on full-scale audio -> sample peak 0.8670 = -1.24 dBFS, true peak 0.8670 = -1.24 dBFS, gain reduction 7.18 dB
C: full-scale square through the limiter -> sample peak -2.18 dBFS, true peak 0.8756 = -1.15 dBFS, overshoot 1.02 dB
D: full-scale noise burst -> sample peak -3.70 dBFS, true peak 0.8882 = -1.03 dBFS, overshoot 2.67 dB
E: trim +6 dB with the limiter OFF -> sample peak 1.9953 = 6.00 dBFS
F: declared latency 8 samples = 0.17 ms at 48 kHz
```

- **`test/dsp/EffectsBusTest.cpp`**: the limiter cases were written against a non-lookahead stage. They now
  account for the declared 8-sample delay — the crest lands at `spikeIndex + kLatencySamples`, and the unity-gain
  window compares the output to the **delayed** input (`input.getSample (0, i - L)`) — and every numeric
  expectation references `OutputLimiter::ceilingLinear` / `targetLinear`. The expected crest is
  `softClip(targetLinear)` through a shared `ebxCrest()` helper: a value sitting on the target is above the knee,
  so it lands at 0.867037, not at `targetLinear` = 0.868725. The DC-output tolerance was widened to 2e-2 because
  a finite 65-tap window truncates the unity sum (measured 0.1637 vs 0.1504).

### Docs

- `docs/architecture/dsp-pipeline.md`, `docs/default-parameters.md`, `docs/reference/dsp-api.md`: the
  "non-lookahead / no latency / −1.68 dBFS" claims are replaced by the true-peak description, the −1 dBFS
  ceiling, the safety margin, and the declared **8-sample** lookahead.
- Comments in `Source/PluginProcessor.cpp` (parameter block, master tap, latency site) and the three
  "non-lookahead peak guard" comments (`PluginEditor.h`, `PluginEditor.cpp`, `PluginProcessor.h`) now say what
  the stage does: true-peak detection, a declared 8-sample lookahead, soft clip after the gain.
- `tooltip.limiter_enable` rewritten in all six locales (EN/FR/DE/ES/JA/ZH) to describe true-peak detection at
  −1 dBFS, the soft clip, and the declared 8-sample (~0.17 ms) lookahead. Bytes verified: no BOM, strict UTF-8,
  no U+FFFD.

## UI — The reverb is its own frame next to Effects, with a header row for the room names

### Context

The user asked for it directly: *"S'il y a autant de réglage propre à la reverb, je pense qu'on devrait les
mettre dans une nouvelle frame à côté de la frame Effects: Reverb lead + send et reverb Harmonies + send. Et
sans doute le duck si ça ne s'applique qu'à la reverb."* The lead room, the harmony room and their sends were
cells of the Effects block, so opening Effects was the only way to reach them.

The "if it applies only to the reverb" condition was **measured before moving Duck**: `duck_depth` is consumed at
exactly one audio site, `sharedReverb.processReturn (buffer, 1.0f - duckDepth * (1.0f - duckGateGain))` in
`PluginProcessor.cpp`, and the lead reverb's wet is ducked by `noiseGate.getCurrentGain()` directly — never through
this knob. So Duck belongs to the Reverb frame, not to Effects.

Consented design choices: the frame sits **right of Effects** (bottom row reads correction | effects | reverb |
scale | harmony); folded it keeps **both rooms**, and the sends + duck fold away; the fold is **persisted**; at
minimum width the two room toggles become **one header row** because "Harmony Reverb" was clipped to an ellipsis,
and horizontal scrolling of the bottom band is **accepted** rather than shrinking any floor.

### Changes

- **`Source/PluginEditor.h/.cpp`**: new `ebs::ExpansibleFrame reverbFrame`, registered on `bottomBand` immediately
  after `effectsFrame`. Ten widgets are reparented into it — `reverbEnableButton`, `reverbMixSlider`,
  `padReverbEnableButton`, `padReverbMixSlider`, `leadSendSlider/Label`, `padSendSlider/Label`,
  `duckDepthSlider/Label` — and the effectsFrame child loop excludes them. The send room (Lead Send, Harmony Send,
  Duck) is `setVisible(false)` when folded; the two rooms stay visible in both states.
- **Header row**: the two room toggles form one 18 px strip across the top of the frame (`effectPowerH`), with the
  mix knobs hanging under them via a local `placeRoomKnob`. The lead name fits one column in every locale, so it
  stays inside column 1; the harmony header runs from its own column to the right edge when open (74 + 8 + 74 = 156 px),
  which is why opening the send room costs no extra width for a label. Root cause of the clip, measured in
  `eiffelbs/LookAndFeel.h`: `drawToggleButton` paints a tick well of `jmin(16, height*0.7)`, then centred-left text
  in `fontToggleButton()` (14 pt) inside `bounds.withTrimmedLeft(size + 6)` — a 74 px cell leaves ~52 px of text,
  while "Harmony Reverb" needs ~95 and "Réverb harmonies" ~110. JUCE clips a ToggleButton label with an ellipsis; it
  never wraps.
- **Locale-measured folded width**: new `reverbHeaderNeed(text) = 18 + ebs::fontToggleButton().getStringWidth(text) + 2`,
  used by `updateBottomBandWidths()` to size the two folded columns from the loaded language, and by `resized()` to lay them out.
  Open width stays a pure cell count (3 columns = 286 px). EN asks ~225 px folded, FR ~232 px: a short locale stays compact, no locale clips.
- **Persistence**: the fold is stored as root attribute `reverbExpanded` mirroring `harmonyExpanded` —
  `getReverbExpanded()` / `setReverbExpanded()`, written on state save, read on restore (default open), and written by
  the preset path. It is UI state, not a parameter, so it never enters the morph state. Round trip proven: patching
  `reverbExpanded="0"` in `%APPDATA%\OpenVoxTuner\OpenVoxTuner.settings` makes the standalone start folded.
- **Chevron tooltip**: new key `tooltip.reverb_more` in all six locales, naming what folds away (Lead Send, Harmony Send, Duck).
- **Latent repaint gap closed**: `effectsFrame` and `reverbFrame`, with their chevrons, were missing from the theme-switch
  repaint list, so a light/dark switch left their old ink on screen.

### Measurements at the minimum host width (client 1100 px)

| Frame | folded / closed | open | floor |
|---|---|---|---|
| Correction | 226 | 386 | 226 |
| Effects | 204 | 286 | 204 |
| Reverb (EN) | ~1155 total row | 286 | locale-measured |
| Root/Scale | — | 236 preferred | 200 |
| Harmony | 294 | 384 preferred | 300 |

With Reverb open the floors sum to **1216 px** (226 + 204 + 286 + 200 + 300) against a 1100 px client, and ~1155 px
folded in English: the row scrolls. Adding a frame in both fold states cannot fit under hard floors, so scrolling is
the accepted answer — no floor was lowered, and Harmony keeps clipping at the right edge with its scroll cue.

### Verification

Screenshots taken through `tools/capture_reverb_frame.ps1` (which patches `reverbExpanded` via
`tools/set_standalone_fold.ps1`, so every shot doubles as the persistence round trip), English + French, open + folded,
at 1400 px outer (no scroll) and at the 1132 px minimum: **"Harmony Reverb" / "Réverb harmonies", "Envoi lead",
"Envoi harmonies" and "Duck" all read in full — no ellipsis** in any state, including the tightest case.

### Docs

- `docs/architecture/ui.md`: the bottom row is documented as the band order correction | effects | **reverb** | scale | harmony,
  with what each frame holds and which folds persist. The stale `block1Bounds … block4Bounds` sentence was removed.
- `docs/implementation-roadmap.md`: entries **UI.19** (Reverb frame split out of Effects) and **UI.20** (room names as a header row).

## UI - Rooms stacked, Duck in its own column, Limit visible when Effects is folded

### Context

Three layout questions came from the screenshots of the new Reverb frame: *"Pourquoi « Limit » n'apparaît pas
sous Doubler en mode fermé…"*, *"Pourquoi Harmony Reverb n'est pas en dessous Reverb (à renommer en Lead Reverb)
puisqu'il y a de la place ?"* and *"Duck devrait être sur une 3eme colonne."* The consented answers: **stack the two
rooms + Duck in column 3**, **keep Limit visible when folded**, keep the word "lead" in every locale (EN "Lead Reverb",
FR "Réverb lead", DE "Reverb Lead", ES "Reverb lead", JA リード・リバーブ, ZH 主唱混响), and accept horizontal scroll
at minimum width.

### Changes

- **Folded Reverb is ONE column**: `stackedW = jmax(kEffectsColW, leadNeed, harmNeed)`, split into two room cells of
  `(h - rowGap) / 2` — Lead Reverb header + mix knob on top, Harmony Reverb header + mix knob below. The folded width
  is the widest room name, not the sum of both.
- **Open Reverb is three columns**: column 1 Lead Reverb + Lead Send, column 2 Harmony Reverb + Harmony Send,
  column 3 Duck alone (`placeLabelledKnob`). `placeRoomColumn` paints a room cell: 18 px header strip, mix knob sized
  by the remaining height minus the send row's floor (`sendLabelH + 2 + 30 = 46`), then the labelled send.
- **Width model** in `updateBottomBandWidths()`: open `chevron(28) + 2*kEffectsPad + leadNeed + harmNeed + duckNeed + 2*8`,
  folded `chevron + 2*kEffectsPad + jmax(leadNeed, harmNeed)`; every need floored at the 74 px cell. "Duck" (37 px of text)
  fits inside the floor, so its column costs exactly 74 px.
- **Effects keeps the limiter when folded**: the `setVisible (effectsOpen)` calls on `limiterEnableButton`,
  `outputTrimSlider` and `outputMeter` are deleted. Column 2 is Doubler on top, Output Limit + meter strip below, in
  BOTH states; only Formant/De-Esser and Saturation fold away to the right, so the folded width stays two columns (204 px).

### Measurements at the minimum host width (client 1100 px)

Label text widths measured with Segoe UI 14 pt (the `ebs::fontToggleButton()` face on Windows), header need =
`18 + text + 2`, column floor 74 px. GDI+ `MeasureString` is an upper bound on JUCE `getStringWidth`, so the frame
lands narrower than the strict sum: EN folded measures ~156 px (stacked column 108 px) against a 132 px need.

| Locale | "Lead Reverb" | harmony name | Duck | folded Reverb frame | open Reverb frame |
|---|---|---|---|---|---|
| EN | 83 -> need 103 | "Harmony Reverb" 112 -> 132 | 37 -> floor 74 | ~156 px (measured on screen) | ~383 px |
| FR | 80 -> 100 | "Réverb harmonies" 120 -> 140 | 74 | ~178 px | ~383 px |
| DE | 83 -> 103 | "Harmonie-Hall" 98 -> 118 | 74 | ~166 px | ~383 px |
| ES | 80 -> 100 | "Reverb armonías" 112 -> 132 | 74 | ~180 px | ~383 px |
| JA | リード・リバーブ 94 -> 114 | リバーブ 53 -> 73 | 74 | ~162 px | ~383 px |
| ZH | 主唱混响 69 -> 89 | 和声混响 69 -> 89 | 74 | ~137 px | ~383 px |

Frame edges read off ruler overlays on the bottom-band crops: folded Reverb EN ~156 px (stacked column 108 px), FR
~178 px (column 130 px); open ~383 px in both locales, where interior 335 px = three columns + two 8 px gaps.

At minimum width the row content is correction ~199 + effects 204 + reverb EN ~156 / FR ~178 + scale ~220 + harmony
floor 300, plus four 6 px gaps: **~1180 px against a 1100 px client**, so the band scrolls and Harmony clips at the
right edge with its cue. Accepted (UI.17 rule): no floor was lowered to fit a frame added in both fold states.

### Verification

Eight OS captures of the standalone, EN + FR, folded + open, minimum outer 1132x639 (client 1100) and wide 1400:
`ui_check_reverb_{folded,open}_{en,fr}_{min,wide}.png`, bottom-band crops `crop_reverb_*.png` (200 px tall so the header
row is inside), ruler overlays `ruler_*.png`. Every crop shows Lead Reverb above Harmony Reverb in one folded column,
three open columns with Duck alone on the right, no clipped header in EN or FR, and Doubler + Output Limit + meter under
a folded Effects frame. The Duck column is a label + knob only (zoomed check: no stray toggle glyph). Release rebuild 0
errors; tests **180 OK / 0 FAILED**.

### Docs

- `docs/architecture/ui.md`: the Reverb bullet now describes stacked rooms folded and three columns open, and the Effects
  bullet states that Doubler + Output Limit + meter stay visible in both states.
- `docs/implementation-roadmap.md`: entry **UI.21** with the measured widths and floors.

## UI - A title spine on every bottom-row frame, Key & Scale second, everything folded (2026-10-17)

Requested: a vertical band on the left of each frame carrying its name, hugging the rounded corner; Key/Scale moved to
second position in the row; every expansible frame starting folded.

- **Library (`ebs::FrameTitleBand`, eiffelbs-ui `v0.15.13`)**: a left vertical title spine, captioning the frame in UPPERCASE, bold, letters tracked apart by `ebs::kBandLetterSpacing` (1.2 px between glyphs) and drawn at 50 % opacity (`ebs::kBandTextAlpha = 0.50f`). Half strength plus tracking is what a caption needs: tight small capitals blur into one stroke, the bright version pulled the eye off the knobs, and this is exactly what the human asked for after testing both by hand in the app. The spine stays **17 px** wide for every locale - tracking stretches a name ALONG the spine only, never across it, and uppercase bold glyphs standing in it are 13 pt at 13 px thick, so two pixels clear at each edge make 17 -
  a longer translated name pays in LENGTH along the spine (glyphs shrink 13 pt to 9 pt), never in width. JUCE has no
  rotated-text API, so the band measures its glyphs with `GlyphArrangement::getBoundingBox` and draws them through an
  `AffineTransform::rotation (-halfPi)`: baseline reading bottom-to-top, glyphs facing left (book-spine convention).
  It fills row-by-row against the parent frame path so the tint HUGS the rounded corner; `reduceClipRegion(Path)` was
  tested and does NOT exclude the corner. The curve is now shared: `ebs::frameCornerRadius` is exposed above
  `drawGroupFrame`, which uses it, so band and frame are the same radius (6 px). Separator is a solid 1 px column - an
  antialiased thin line blended. Disabled state paints `textDim()`, never bright ink.
- **Consumer**: one spine per bottom-row frame, as a CHILD of that frame, added with `addAndMakeVisible`; titles come from
  five new keys `frame.title.correction` / `.keyscale` / `.effects` / `.reverb` / `.harmony` in all six locales and ride a
  new `translatableBands` list next to `translatableLabels`, refreshed by `refreshLabels()` like every label. The band is
  the frame name only - existing knob captions and headers stay exactly where they are.
- **Order**: registration order IS left-to-right, so Key & Scale moves to position 2; Final order is
  Correction | Key & Scale | Effects | Reverb | Harmony. Harmony stays the only growable frame.
- **Width model**: every block preference AND floor grows by `ebs::FrameTitleBand::bandWidth()` (17 px, down from 26): correction,
  effects, reverb, scale and harmony open/closed.
- **Content areas**: each interior removes the spine with `removeFromLeft` BEFORE `.reduced (pad)` - at the correction,
  effects, reverb and harmony interiors, and host-side for `scaleFrame`, because `ebs::FramedBody` has no `interior()`.
- **All frames start folded**: the restore defaults of `harmonyExpanded` and `reverbExpanded` were true, so a fresh install
  opened Harmony and Reverb. Both are now false, and an absent attribute (a state file written before the fold existed)
  also reads false - so a new install and an old session open the same narrow row, which is what pays for the spine room.

### Validation

- Release rebuild 0 errors, exit 0; tests **180 OK / 0 FAILED**, exit 0, `[ OK ] OutputLimiterTruePeak (17 assertions)`.
- Standalone at minimum outer width 1132x639 (client 1100): the bottom row reads **Correction | Key & Scale | Effects | Reverb | Harmony** with a spine on each frame, folded content intact (Gate/Doubler/Compress/Limit + GR/TRIM under folded Effects, Lead Reverb above Harmony Reverb under folded Reverb), and Harmony clipped by the window with the hatched scroll cue. Captures `band_en_min.png` / `band_en_min3.png`, bottom-band crop `band_en_row3.png` (200 px tall).
- Re-check after the style tweak: standalone relaunched and resized to minimum outer width 1132x639, full window `ui_min_1705.png`, bottom-row crop `band_en_row5.png` (200 px tall). The row reads CORRECTION | KEY & SCALE | EFFECTS | REVERB | HARMONY in tracked capitals at half strength, chevrons visible on every frame, Harmony clipped by the window with the hatched scroll cue - a human eyeball check on screen, no synthetic input needed.
- Wide row at 1400x650 (`band_en_wide_row.png`): every spine hugs the 6 px corner, each chevron is visible, and no knob collides with a band.
- The locale fit is proven offline in the library, where the band lives: `eiffelbs-ui-smoke` **PASS (0 failures)**. Uppercase bold glyph thickness stays 13 px at 13 pt and 9 px at 9 pt for every shipped title - Latin, French accents, Japanese kana and Chinese ideographs all measure identically, so ONE constant carries them. Tracking is measured as a difference: "REVERB" draws 46 px against 36 px for the glyphs alone, the 1.2 px gaps between six letters. Measured lengths short=41 / long@13pt=135 / long@9pt=102 with `bandWidth()==17`. Ink is counted as pixels changed against a title-less render of the same spine: 212 in a 170 px spine, 258 in a shrunk 84 px one, separator 158 px over the trim, an empty band at 0 ink, a disabled band dimmed with bright-ink 0 and changed-pixel 2268. Against the 164 px budget the four shipped locale titles give length@13pt / length@9pt / ink: FR "Tonalité et gamme" 123/94/622, FR "Réverbération" 108/82/620, JA "キーとスケール" 157/117/711, ZH "根音与音阶" 101/76/399 - all fit at full size, the tightest leaving 7 px, no shrink needed. The promise is measured per shipped name: every one keeps all its letters at the minimum size inside a real row; a stress title needs 102 px where an 84 px spine has 78, so its last letters fall off - captions stay short by design and the assertion prints that limit. Receipts `eiffelbs-title-band.png` and `eiffelbs-title-band-locales.png`, copied as `.dbg/band_lib_en.png` and `.dbg/band_lib_locales.png`. The app row itself was verified on screen in every language by the human, who accepted the caption style there; the tracked half-strength English row was re-checked live at minimum width.
- Limitation measured on this machine: launching the standalone from patched settings lands the root fold attributes (the row starts folded) but does NOT apply persisted parameter values - a state file decoded after launch carries `ui_language="5.0"` and `scale="3.0"`, saved back verbatim at close, while the screen shows English labels and Scale "Chromatic". Live per-locale screenshots are therefore not reachable through settings patching; an OS capture of a foreign-language row (language menu + chevrons driven by hand) is owed to a manual session.

## UI - Accent-free spine capitals, room labels Lead / Harmony / Ducking, Limit swapped with Formant (2026-10-17)

Asked verbatim: *"Il ne faut pas d'accent pour les textes en majuscule. Maintenant qu'on a les titres des frames, il
 faudrait changer le label des composants de REVERB : - Reverb => Lead - Harmony reverb => Harmony - Duck => Ducking —
 Pour la frame Effects, je voudrais swapper Limit et Formant (applicable en replié et ouvert). Pour la frame Corrections,
 je me demande si TUNING ne serait pas plus approprié. Tu en penses quoi ?"* — accepted as *"a. complet"*,
 *"b. ok sur toutes les langues supportées"*, and the frame name stays **Correction** (the human typed "Corrections",
 plural; the shipped key `frame.title.correction` carries the singular, which is what every locale reads).

- **Captions carry no accents (`ebs::bandCaps()`, eiffelbs-ui `v0.15.14`).** JUCE `toUpperCase()` casing-changes only ASCII
  letters, so live in the app FR `Réverbération` upper-cased to `RéVERBéRATION` and `Tonalité et gamme` to
  `TONALITé ET GAMME`. The helper de-accentifies BOTH cases — à–å→A, è–ë→E, ì–ï→I, ò–ö→O, ù–ü→U, ç/Ç→C, ñ/Ñ→N,
  ß and ẞ (U+1E9E) → "SS" — because JUCE leaves U+00DF as it is. Ligatures (Œ, Æ), CJK and Cyrillic pass through untouched:
  uppercase is a no-op for ideographs, so one rule serves every locale. The helper drives the paint AND the thickness
  measurement (`bandGlyphThickness` runs on `bandCaps(text)`), so every consumer measures exactly what it draws. Additive:
  patch bump + annotated tag `v0.15.14`, commit `b5f7bdb` on branch `ui-title-band` — **local, not pushed**.
- **Room labels renamed in all six locales** (frame titles, tooltips and sends untouched):
  - lead room toggle `label.reverb_btn`: **Lead** (EN/FR/DE/ES), **リード** (JA), **主唱** (ZH);
  - harmony room toggle `label.pad_reverb_btn`: **Harmony** (EN), **Harmonie** (FR/DE), **Armonías** (ES), **ハーモニー** (JA), **和声** (ZH);
  - ducking knob `label.duck_btn`: **Ducking** (four latin locales), **ダッキンィ** (JA), **闪降** (ZH).
  The Chinese lead term was delegated to me (*"Je ne connais pas le chinois donc je te laisse trancher"*): **主唱**, picked
  because the shipped tooltips already say it (`跟随主唱`, `主唱与生成的和声之间的平衡`), so label and tooltip now carry one word.
- **A shipped mistranslation, found while measuring:** the Japanese ducking knob read `ドッキンィ` — first glyph U+30C9 ド,
  "docking", the ship-to-ship word, not audio ducking. It is now `ダッキンィ` (first glyph U+30C0 ダ), patched byte-wise with
  counts verified before (`bad_count 1`) and after (`good_after 1`, file size unchanged at 199455 bytes, strict UTF-8 decode,
  no BOM, zero U+FFFD).
- **Effects swaps Limit and Formant in BOTH states.** Column 2 is now Doubler over Formant; the Output Limit toggle, trim knob
  and meter strip move into the open-only column 3 above the De-Esser. The consequence is accepted and written where it lives:
  a folded frame hides the CONTROL, never the processing — `limiter_enable` keeps guarding every sample at −1 dBFS, the meter
  refresh keeps running while its widgets are hidden. This reverses the UI.21 promise that "the Output Limit stays visible
  while Effects is folded" (*"a. complet"*). Width changes nothing: `numEffectCols = effectsOpen ? 4 : 2` counts cells, so
  Effects stays ~204 px closed and open widths are unchanged. The frame-title idea TUNING was not adopted — the block hosts
  speed/amount, vibrato, humanize and voice type, corrections to how pitch is read, wider than tuning.
- **Labels measured, not eyeballed (eiffelbs-ui `v0.15.15`).** The consumer sizes every Reverb column from
  `reverbHeaderNeed(text) = 18 px power strip + label width at the 14 pt toggle font + 2 px edge`, with cell floor
  `kEffectsColW = 74`. Measured through real JUCE glyphs in the library smoke, locale by locale (lead / harmony / ducking):
  EN 43/63/59, FR 43/66/59, DE 43/66/59, ES 43/67/59, JA 41/54/54, ZH 34/34/34 px. **Every need rides the fixed floor**:
  the widest ask in any shipped locale is Spanish "Armonías" at 67 px, 7 px inside a cell, and ideographs ask LESS than Latin
  (two and three glyphs carry what four to eight letters say). So a folded Reverb frame stays ONE fixed column in every
  language — no rename widens it, no header clips. The smoke now asserts exactly that
  (`check (widestLead <= 74 && widestHarmony <= 74 && widestDuck <= 74)`), plus `bandCaps()` checks for FR
  "REVERBERATION", "TONALITE ET GAMME", ES "CORRECCION", the German marks (umlaut K→KUR, cedilla→CA, ñ→ANO, ß→STRASSE) and
  byte-exact ideograph pass-through. Version `0.15.15`, commit + annotated tag **local only** — no push was approved for this repo.

### Validation

- Library smoke rebuilt after a CMake reconfigure (the version line changed) and run: fresh link line confirmed, then
  **`eiffelbs-ui smoke: PASS (0 failures)`**, logs `.dbg/smoke_cfg_v01515.txt`, `.dbg/smoke_build_v01515.txt`, `.dbg/smoke_v01515.txt`.
- OVT Release rebuild 0 errors, exit 0; tests **180 OK / 0 FAILED**, exit 0, `[ OK ] OutputLimiterTruePeak (17 assertions)`.
- Standalone relaunched from the freshly built binary at minimum outer width 1132x639 (client 1100), all three fold attributes set to 0 by
  `tools/capture_effects_folded.ps1`: `.dbg/effects_folded_min.png` reads **CORRECTION | KEY & SCALE | EFFECTS | REVERB | HARMONY**
  in tracked accent-free capitals at half strength, folded Effects shows Gate/Doubler over Compress/Formant with **no Output Limit
  knob, toggle or meter strip**, and the Reverb frame stacks "Lead" above "Harmony", both names fully inside their column. Baseline
  `.dbg/ui_min_1705.png` keeps the pre-swap pairing (Compress/Limit + GR/TRIM) for comparison.
- The open Effects row cannot be driven from settings — synthetic input is blocked and the Effects fold is deliberately not persisted — so the
  open state is proven by code and by the width model; language rendering and the open columns are checked in-app by the human, who asked for no extra captures (*"Pas besoin de rentrer dans le détail et faire des captures"*).
