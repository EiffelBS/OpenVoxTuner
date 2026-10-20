# Changelog — 2026-10-20

## Feature — Every control explains itself: plain-language tooltips, and the Delay gets its own

Requested: *"Division et Feedback n'ont pas de tooltip. Le contenu des tooltips doit expliquer le
comportement du composant de manière simple pour un utilisateur lambda, pas de référence à
"l'ancienne version" ou autre chose lié au développement. L'utilisateur n'a besoin que de savoir à
quoi sert le composant et quel est son effet."*

- **The Delay explains itself**: four new tooltips (switch, mix, note value, repeats) in the six
  shipped languages. DIVISION and FEEDBACK had no tooltip at all - a user had to guess what they
  did. MIX had only the activity-dot sentence.
- **Every tooltip rewritten in plain language**, in the six languages: the reverb of the "older
  versions" (the Hall tooltip), the `1/sqrt(1+N)` formula of the gain match, the `Haas delay` and
  the `allpass` / `Pre-warp` jargon of the formant strategy, the "third axis, orthogonal to
  FlexTune" of the attack correction, the CPU note of the voice type, the "before the tuning" and
  the "after the voice shaping" signal-chain remarks, the controls the row no longer carries
  (Flex, Attack-Aware) and the "shared bridge" wording of the key source. A tooltip now says what
  the control does and what it changes, and stops.
- **The English table no longer holds four French sentences** (the curve-editor tooltips) and the
  **Chinese table no longer misses four entries** (the curve-editor tooltips and two MIDI-learn
  labels): a missing entry does not raise an error, it silently falls back to English.
- **The activity-dot sentence is no longer repeated** on every language change: the dot sentences
  are appended after the control's own tooltip has been restated, not to the already-appended one.
- **New test** `test/ui/TranslationCompletenessTest.cpp`: every English string must exist in the
  six shipped languages, no tooltip may carry a development phrase, no tooltip may be empty. The
  suite goes from 209 to 212 groups.

Verification: build green, suite 212 groups / 0 failures, standalone relaunched and the four Delay
tooltips read back from the running plugin.

## Fix — The Harmony switch names were cut in half, and the captions of disabled controls stayed bright

Two things the user spotted on screen after the unified caption size:

- **"Pourquoi Follow Lead et Gain Match sont tronqués à gauche et droite alors qu'il y a de la place ?"**
  A power switch was sized from the TEXT it carries (the mixed-case name at the 14 pt toggle font,
  52 px for "Follow Lead") while it DRAWS the smallcaps caption (58.5 px at 11 pt): capitals are
  wider than a mixed-case name, so the button clipped its own name — the last letter of FOLLOW LEAD
  and GAIN MATCH was cut in half. `powerButtonWidth()` now measures the name it draws
  (`ovt::spine::trackedLength` at the caption size), so the button is exactly as wide as the caption
  it carries.
- **"les labels de composants désactivés (Drive, warmth, Division...) devraient être mis en opacité
  0.5 également"**: a caption now follows its control — at the caption level (50 %) when the control
  is off or disabled with it, at full strength when it is live
  (`OpenVoxTunerAudioProcessorEditor::applyCaptionDimming()`). The smallcaps captions follow the same
  rule (`ovt::spine::captionColour`): ONE quiet level for "not live" in the whole row, the dimmed
  power glyph being what still tells "off" from "unreachable". Before, a disabled knob kept a
  full-strength caption while its own switch sat quiet, so the cell read "off" and "active" at once.

Verification: build green, suite 209 groups / 0 failures, standalone relaunched and the row checked at
1500 px: FOLLOW LEAD and GAIN MATCH read in full, and Drive, Warmth, Mix, Division and Feedback now
sit at 50 % with their modules off.

## Feature — One caption size for the whole bottom row (11 pt)

Requested after seeing the smallcaps land: *"Ok, je pense qu'on devrait quand même aligner la taille de
police."* The row had ended up saying the same thing at FOUR sizes — the smallcaps captions at 9 pt,
the vertical Room/Hall captions at 13 pt, the secondary captions at 11 pt and the knob captions at
13 pt — and the smallcaps had become the smallest text of the row.

- **ONE size, 11 pt nominal (9.35 px rendered on macOS, the 0.85 platform scale)**: the switch
  names, the captions above a knob or a combo and the vertical Room/Hall names
  (`ovt::spine::kCaptionFontSize`, `Source/ui/OVTSpineCaption.h`) now share the size of the frames'
  normal captions, which are all 11 pt regular (`ovt::fontCaption()`, `Source/ui/OVTFonts.h` — it
  replaces `fontLabelPlain()` / `fontLabelSmallPlain()`, now one face). The smallcaps keep their
  identity through the WEIGHT (bold, tracked 1.0 px, 50 % at rest, full strength on an engaged
  switch), not through a smaller size.
- **The vertical captions join the family**: `ovt::VerticalCaption` used to start from the spine's
  13 pt and shrink, so ROOM/HALL were the largest captions in the row; they now start from the
  caption size and only shrink if their own column is too short (`spine::fittedCaptionSize`).
- **The Effects cell grows 74 → 82 px**: the cell carries the power glyph (10.8 px), the gap
  (8 px) and the switch name, and the longest name in any shipped locale — the Japanese
  フォルマント, 61.1 px at 11 pt — needs 63.2 px of room. At the old 9 pt the row could not grow
  the smallcaps at all (that name fitted with 0.3 px to spare), so aligning the sizes meant
  widening the cell, not shrinking the type. The Reverb voice columns ride the same floor. The row
  now asks for 1416 px of client width (1368 before).
- Measured at 11 pt, every caption strip still holds its name in every shipped locale: Speed 73.5
  px (ES) in 86, Amount 53.8 (DE) in 86, Root 50.5 (ES) in 56, Scale 45.1 (DE) in 140, Key/Scale
  Detection 130.4 (FR) in 156, switch names 61.1 (JA) in 63.2. The German `Geschw. (ms)`
  abbreviation stays (the long form was 98 px at 9 pt and 122 px at 11 pt).

Verification: build green, suite 209 groups / 0 failures, standalone relaunched and the whole row
checked at 1500 px (all five frames visible): the smallcaps captions, the switch names and the
normal captions all read at one size, the ROOM/HALL captions now match their neighbours, the
Modern/Transparent pill is still the only bold non-caps label.

## Feature — Smallcaps captions in Correction and Key & Scale, regular weight for every other frame caption

Requested on the same morning: *"Il faut appliquer le même format de police 'smallcaps' dans les
frames Correction (Speed et Amount) et key/scale (Root, Scale, Key/Scale Detection). Ensuite, tout ce
qui n'est pas à ce format 'smallcap', dans les frames, peut rester sur leur police actuelle mais
doivent passer de gras à normal (exclure la pill de la frame Correction)."*

- **Same caption family across the row**: `ovt::HorizontalCaption` (`Source/ui/OVTSpineCaption.h`)
  draws the Correction `Speed (ms)` / `Amount` captions and the Key & Scale `Root`, `Scale`,
  `Key/Scale Detection` captions in the power-switch names' typography — one size for all
  (`spine::kCaptionFontSize` = the spines' 9 pt floor), the switch names' tighter tracking
  (`spine::kCaptionLetterSpacing` 1.0 px), at the spines' quiet 50 % (`ebs::kBandTextAlpha`).
  `Key/Scale Detection` names a switch, so it brightens to full text colour while detection is ON,
  read from the power button at paint time. Justification is honoured horizontally (centred, or left
  beside the glyph) and always centred vertically.
- **Fit checked in every locale**: the fixed 9 pt fits every shipped translation in its own strip
  (worst cases measured: Speed 54 px in 86, Amount 54 in 86, Root 47 in 56, Scale 46 in 114,
  Key/Scale Detection 127 in 156). German `Geschwindigkeit (ms)` measured 98 px in an 86 px strip,
  so it becomes the standard abbreviation `Geschw. (ms)`: a caption is never cut mid-word for a
  layout reason, and the strip keeps its one shared glyph size.
- **Regular weight for the non-caps captions**: every other label inside the bottom-row frames keeps
  its current face (13 pt knob captions, 11 pt secondary captions, 8-11 pt fitted half-column
  captions) but loses bold: `ovt::fontLabelPlain()` / `ovt::fontLabelSmallPlain()` (new in
  `Source/ui/OVTFonts.h`) replace `fontLabel()` / `fontLabelSmall()` in the knob, combo and title
  labels of Correction, Effects, Reverb and Harmony; `fitLabelToWidth()` now fits the plain face.
  Bold is left to the three voices that are design decisions: the frame spines, the smallcaps
  captions, and the Modern/Transparent pill of the Correction frame (explicitly excluded).
- Nothing moves: the label strips keep their heights and widths, and `useVoiceLabel`'s measured
  width in the Harmony layout simply re-measures narrower in regular weight.

Verification: `ninja OpenVoxTuner_All` green, suite 209 groups / 0 failures, standalone relaunched
and the whole row checked at 1420 px (all five frames visible): Speed/Amount/Root/Scale/Key-Scale
Detection in smallcaps; Drive, Warmth, Mix, Division, Feedback, Use Voice, Vocal Stack, Room/Hall
captions and the switch names unchanged in size; the pill still bold.

## Fix — One caption size for every power-switch name

Reported on the first morning test of the caps build: *"Les tailles de police que tu as appliquées
sont inconsistantes, parfois grosses, parfois petites, c'est pas très cohérent ou lisible. Utilise la
taille de police de 'FOLLOW LEAD' en réduisant un tout petit peu l'espacement de caractères."*

The first build fitted each switch name to its own cell, spine-style: a short `GATE` reached 13 pt
while `SATURATION` sat at 9 pt in the cell beside it, `COMPRESS`/`FORMANT` landed at 11-12 and
`FOLLOW LEAD`/`GAIN MATCH` at 9. In a vertical spine that per-name shrink is invisible (each name
runs up its own edge); in a horizontal row of switches it is exactly the incoherence being reported.

- **One size for all**: `ovt::PowerToggleCaps` now draws every name at `ebs::kBandMinFontSize`
  (9 pt), which is the size the longest names — `FOLLOW LEAD`, `SATURATION` — already settled at.
  The glyph size no longer depends on the word or the language, so the row reads as one caption
  family.
- **Tracking reduced a little**: `kCaptionLetterSpacing` = 1.0 px instead of the spines' 1.2, a
  switch name being read as a word at eye level rather than as an epithet running up a narrow column.
- **Fitting is gone from this widget**: the room beside the glyph is no longer measured against each
  name; the fixed size fits every translated label in every switch (longest cell: `SATURATION` at
  53.0 px inside 55.2 px), and the clip that was already there still guards a hypothetical overflow
  without ever re-sizing the text. `spine::fittedSize/layout` keep taking the tracking explicitly, so
  the vertical Reverb captions still use the spines' 1.2 px recipe unchanged.
- The state cue is untouched: 50 % at rest (`ebs::kBandTextAlpha`), full text colour when engaged,
  dimmed when disabled.

Verification: `ninja OpenVoxTuner_All` green, standalone relaunched, band checked on screen
(`GATE`, `DOUBLER`, `LIMIT`, `SATURATION`, `DELAY`, `COMPRESS`, `FORMANT`, `DE-ESSER` all one size,
nothing clipped); suite 209 groups, 0 failures (paint only).

## Feature — Power-switch names use the frame spines' caption typography

Asked right after the vertical captions, while looking at the same row: *"Je pense qu'on devrait
appliquer les smallcaps à tous les labels associés à un bouton power (sans changer leur
orientation)."* Chosen scope: the names the power buttons carry themselves — horizontal, at the
spines' 50 % strength.

- **New `Source/ui/OVTSpineCaption.h`** (the day's first version was `OVTVerticalCaption.h`, which
  held only the vertical case): the spine recipe now lives in one `ovt::spine` helper namespace —
  `capsOf` (`ebs::bandCaps`), `trackedLength`, `fittedSize` (13 pt down to the spines' 9 pt floor)
  and `layout` (tracked glyphs) — shared by `ovt::VerticalCaption` (rotated, the Reverb Room/Hall
  amounts) and the new `ovt::PowerToggleCaps` (horizontal, every power strip). One recipe, two
  orientations: captions and frame spines cannot drift apart.
- **`ovt::PowerToggleCaps`** derives from `ebs::PowerToggle` and restyles only the name: uppercase,
  accent-free, tracked, fitted to the room left beside the glyph, so a long translated name pays in
  glyph size instead of widening its cell or clipping. Strength is the switch's own: 50 % at rest
  (the spines' `ebs::kBandTextAlpha`), full text colour when engaged — the cue the EBS widget
  already gave its name — and dimmed when disabled. Behaviour, colour ids and the glowing glyph stay
  the EBS widget's; that glyph block is copied verbatim because `ebs::PowerToggle::paint` draws
  glyph and name in one body. The copy is marked in the file as the block to delete once
  eiffelbs-ui grows a caption style, which is the clean long-term home for this.
- **All fourteen power strips** use it: Gate, Compress, Doubler, Formant, Output Limit, De-Esser,
  Saturation, Delay, Lead (Reverb), Harmony (Reverb), Harmony (frame enable), Follow Lead, Gain
  Match, and the Key/Scale detection switch (the one with no text stays glyph-only, as before).
  No layout width changed: each button keeps the width measured from its name and the caption fits
  itself inside it; in the narrowest 74 px cells the longest names land at the spines' 9 pt floor,
  and nothing clips or overlaps the power glyph. Knob captions that are not switch names (Drive,
  Warmth, Mix, Division, Feedback) keep their existing label style — the request covered the power
  button's own label, and those captions are a separate decision.

Verification: `ninja OpenVoxTuner_All` and the test suite green (209 groups, 0 failures), the
standalone relaunched and the band checked on screen: the switch names read as tracked capitals,
50 % at rest and bright when engaged, each one inside its cell.

## Feature — Knob captions run vertically, in the frame spines' typography

Asked while testing the reverb frame: *"les labels Room et Hall sur la gauche de leurs knobs,
verticaux et SmallCaps, un peu comme le bandeau titre des frames qu'on a ajouté récemment dans
EBS ?"* Followed by: *"Ducking doit rester avec titre au-dessus."* — the first build rotated the
Ducking caption with the amounts; it stays horizontal above its dial, because the ducker is a return
ramp, not an amount a power switch sends.

- **New `Source/ui/OVTSpineCaption.h`** (added as `OVTVerticalCaption.h`, renamed when the
  power-switch captions joined it): a `juce::Label` subclass that paints its text rotated
  bottom-to-top with the design system's own spine recipe — `ebs::bandCaps` (UPPERCASE, accents
  dropped, safe in every locale), `ebs::kBandLetterSpacing` tracking, `ebs::kBandTextAlpha` at
  50 %, glyphs shrinking from `kBandFontSize` towards `kBandMinFontSize` instead of clipping. It
  is NOT an `ebs::FrameTitleBand`: a spine names a whole frame and therefore spends width on a
  tinted strip and an inner trim, while a caption labels one dial — five tinted strips inside the
  Reverb frame would read as five sub-frames. Because it derives from `juce::Label`, it rides the
  existing i18n refresh (`translatableLabels`) with no new keys.
- **Reverb frame**: the four amount captions (Lead/Harmony × Room/Hall) sit vertically at their
  dial's left. The 14 px a horizontal caption used above each dial go back to the dial: the amount
  cells measure 65 px, so the four dials reach the shared `effKnobD` = 54 px instead of the 49 px a
  top caption allowed, matching the Effects block. The Ducking caption stays horizontal above its
  dial (its column keeps the measured-width rule).
- The column width model is unchanged: a caption asks for `ebs::FrameTitleBand::bandWidth()` (17 px)
  and a 54 px dial plus a 2 px gap fits inside the existing `kEffectsColW` = 74 px cell, so no
  locale widens the frame and the folded row still fits the minimum-width client. The Ducking
  column is now sized as one cell instead of by its measured label width.

Verification: `ninja OpenVoxTuner_All` relinks all three formats with no new warnings (the caption
uses `juce::GlyphArrangement::getStringWidth` like the rest of the plugin); suite still 209 test
groups, 0 failures. The change is layout + paint only.

## Fix — Both reverb amounts carry the activity dot, not only the Hall ones

Reported while testing the new Reverb frame: *"seuls les knobs Hall ont les points centraux
(indicateur de signal), pas les Room, ce qui n'est pas logique."* The first build put the voice's
dot on the Hall amount only — the reasoning was that a second dot in the same column would report
the same path twice. On screen that read as a broken control instead: two amount knobs sit side by
side under the same switch, one shows a signal disc and the other looks like a plain dial.

- **`Source/PluginEditor.h`**: `leadRoomSlider` and `harmonyRoomSlider` are now
  `ebs::ActivityKnob`s, like the Hall ones.
- **`Source/PluginEditor.cpp`**: the timer feeds both amounts of a voice from the same path peak
  (`leadPeak` for Lead Room + Lead Hall, `harmonyPeak` for Harmony Room + Harmony Hall), gated by
  the voice switch like every other module dot.
- Both dots of a voice therefore show the same level, which is honest: they are two sends of the
  same voice. The Ducking knob still carries none — it is a return ramp, not a voice.
- `docs/architecture/ui.md` and `docs/architecture/dsp-pipeline.md` updated (thirteen dots now;
  two per Reverb voice column).

Verification: full suite still 209 test groups, 0 failures; the UI change is visual only.

## Feature — The reverb is a shared ROOM + HALL bus pair again, with a sidechain ducker and two voice switches

### Context

The user reported that he could no longer get a satisfying reverb for his own singing
since the reverb implementation changed on `dev`, while the legacy `main` build gave him
the ideal reverb with **one knob**; he also found the new knobs' tooltips obscure
(*"Les Tooltips de ces nouveaux knobs sont également très obscur"*) and said he was lost.
An audit measured what each version actually did:

| Version | User-facing controls | What the wet path really did |
|---|---|---|
| `main` (legacy) | `reverb_enable` + `reverb_mix` (0.30 default) | One insert `ReverbEffect` on the lead, tuned 0.70 / 0.30 / width 1.0 → **RT60 ≈ 2.43 s**. The "Mix" was a **crossfade**, so at 30 % the dry voice was already attenuated to 70 %. |
| `dev` (before this change) | 7 parameters: lead room toggle + mix, harmony room toggle + mix, two sends into a third shared room, gate-linked duck depth | The lead's 5 knobs combined three partly redundant amount stages; the "Mix" of the shared room and the "sends" described the same thing twice, and the ducker followed the **noise gate**, so it never moved while the singer was singing — the opposite of a ducker. |
| This entry | 2 voice switches + 4 amounts named after their room + 1 ducker depth | Two shared rooms (ROOM short + HALL = the legacy room), fed by four sends; the voice is never attenuated; the ducker is keyed by the corrected lead. |

The user consented to the redesign with one change of his own: *"je préfère un toggle On/Off
pour Lead et Harmony (même s'ils sont off par défaut). Ça permet de proposer des paramétrages
recommandés auxquels l'utilisateur accède lorsqu'il active la reverb. Donc, la valeur par défaut
des knobs est ton 'Prêt à chanter' qui sera directement appliqué quand l'utilisateur active
les reverb."*

### Changes

#### Parameters (`Source/PluginProcessor.cpp` / `.h`)

Seven old reverb parameters replaced by seven new ones, named after what the user hears:

| Old | New | Default |
|---|---|---|
| `reverb_enable` | `lead_reverb_enable` | **false** |
| `reverb_mix` | `lead_room_send` + `lead_hall_send` | 0.15 + **0.45** |
| `pad_reverb_enable` | `harmony_reverb_enable` | **false** |
| `pad_reverb_mix` | `harmony_room_send` + `harmony_hall_send` | 0.10 + 0.30 |
| `duck_depth` (gate-linked) | `reverb_duck_depth` (sidechain) | 0.35 |

The switches start **off**, so a fresh session is dry; the amounts already carry the
"ready to sing" pair, so the first switch-on applies a balanced short room + hall behind the
voice instead of silence. A switched-off voice keeps its amounts (greyed knobs), exactly like
the other effects in the block.

#### One home for the sound (`Source/dsp/ReverbTuning.h`, new)

Both room tunings and the default sends live in one header shared by the processor, the
factory presets and the unit tests: `kReverbHallTuning {0.70, 0.30, 1.0}` (the **legacy Main
room, restored bit for bit**, RT60 ≈ 2.43 s) and `kReverbRoomTuning {0.35, 0.50, 1.0}`
(RT60 ≈ 1.18 s). Changing a tuning changes every consumer at once, on purpose.

#### One sidechain ducker (`Source/dsp/ReverbDucker.h`, new)

Header-only `ovtdsp::ReverbDucker`: per-sample stereo-linked **peak** envelope (attack 15 ms,
release 300 ms), normalised against a −14 dBFS vocal peak so the depth knob means the same
thing at any input gain, `kMaxDepthDb = 12`. Its key is the **corrected lead taken before the
lead bus effects** (the de-esser and the tempo echoes cannot re-trigger it). Depth 0 is a true
bypass; the release keeps running in the sleep path (`processSilence`) so a stopped transport
lets the rooms bloom back instead of freezing a duck. No allocation.

#### Processor routing

- `sharedReverb` replaced by `roomReverb` + `hallReverb` (both `ovtdsp::ReverbEffect` in shared
  mode); the lead bus keeps only `DeEsser → DelayEffect`, and the harmony bus instance is gone
  (the harmony path owns its two sends and nothing else).
- `applyLeadSends()` / `applyHarmonySends()` gate each voice's two amounts by its switch and
  always call both rooms, so a ramp settles even when a switch was just turned off.
  The lead send is tapped **after** `leadBus.apply`, so the echoes ring in the same rooms.
- `applyMasterEffects()` computes `reverbDucker.getDuckGain()` and calls both `processReturn`s
  before the output stage. The ducker is updated from the corrected lead **before**
  `applyLeadEffects()` on the live path, and from `processSilence()` on the sleep path.
- Return trim stays `kReturnTrim = 0.7` per room (≈ −3 dB), so the summed returns cannot pump
  the true-peak limiter at unity send.

#### Compatibilité des états et des slots A/B

- **`setStateInformation`**: a `<PARAMETERS>` tree carrying the old reverb ids and none of the
  new ones is migrated before `replaceState`: `lead_hall_send = jlimit(1.43 × reverb_mix +
  lead_send)`, same for the harmony side, `reverb_duck_depth = duck_depth`, and the two switches
  derived from the old enable flags / non-zero sends. 1.43 is the measured crossfade → send
  calibration (the old Mix also attenuated the dry voice; a send does not). A state written by
  this build is never touched.
- **A/B slots**: the slot writer stores the new amounts and switches; the reader accepts a
  new-style slot as is and migrates a legacy one with the same formula. The migration also
  fixed a latent bug: the legacy branch derived the switches and then overwrote them with the
  defaults of the new attributes.
- `PresetMorpher`: the seven ids moved into `MorphState`, `captureState`, `applyInterpolatedState`
  and `getMorphParameterIds()`; `MorphCoverageTest` still proves every declared parameter is
  either morphed or explicitly excluded.

#### UI (`Source/PluginEditor.cpp` / `.h`, `Source/ui/OVTLanguages.h`)

- The Reverb frame is now **Lead | Harmony | Ducking**. Folded it shows the Lead column only
  (switch + **Room** + **Hall**); opened it adds the Harmony column and the Ducking knob. Each
  amount has its own caption naming the room it feeds; the knob **is** the send.
- Both switches start off, so the folded frame reads as one column of two greyed amounts until
  the singer switches the lead reverb on — then the ready-to-sing pair is already there.
- Activity dots: the Lead dot rides `lead_hall_send`, the Harmony dot rides
  `harmony_hall_send` (the amount that places the voice) and is the only place the harmony path
  becomes visible; the Room knobs and the ducker carry none.
- **Tooltips rewritten in plain words in all six locales** (EN / FR / DE / ES / JA / ZH) for the
  two switches, the four amounts and the ducker: each names its room, says what it does to the
  voice, gives the decay time, and states that a send adds instead of replacing the voice. The
  ducker tooltip says it follows the corrected lead, not the noise gate. The chevron tooltip
  now names what actually folds away (the Harmony column and Ducking).
- MIDI Learn: the four old entries replaced by the five new ones (`Lead Room`, `Lead Hall`,
  `Harmony Room`, `Harmony Hall`, `Reverb Ducking`).
- Frame width model unchanged where it mattered: folded = one measured column, open = three,
  every column floored at the 74 px cell, so no header clips in any shipped locale and the
  folded row still fits the minimum-width client.

#### Factory presets

`Studio Stack` sends only the harmony stack into the rooms (room 10 % + hall 35 %) and keeps
the lead dry; `Breathy Vocals` switches the lead on to room 15 % + hall 40 % with ducking 30 %;
`Lead Vocal` switches both voices on (lead 15 / 45 with ducking 35 %, harmony 10 / 30). Every
preset that promises nothing added leaves both switches off explicitly.

#### Docs

`docs/default-parameters.md`, `docs/architecture/dsp-pipeline.md`, `docs/architecture/ui.md`,
`docs/architecture.md`, `docs/reference/dsp-api.md`, `docs/reference/presets.md` and
`docs/preset-morphing-technical-strategy.md` updated to the bus model; this entry is the
record of the previous behaviour.

### Verification

- **Full suite green**: `OpenVoxTunerTests` — **209 test groups, 0 failures** (Release, ninja).
  New `test/dsp/ReverbDuckerTest.cpp` (corners, dB calibration, attack/release asymmetry,
  stereo link, mono key, empty block, re-prepare); `ReverbEffectTest` rewritten around the two
  shipped tunings; `NoiseGateEnableTest` no longer asserts a gate-linked duck (the gate's stale
  gain is still documented, because the harmony path still guards on it); `PluginPresetTest`
  and `MorphCoverageTest` updated for the new ids.
- **Measured RT60 through the plugin's own send/return path**: ROOM **1.179 s**, HALL
  **2.427 s** — matching the design targets and the legacy Main sound.
- **Measured ducker**: full depth → **−12.0 dB** at or above a −14 dBFS key, half depth →
  −6.0 dB, one release constant recovers 63 % of the envelope; a quiet key ducks
  proportionally less.
- **Measured levels**: a unity send on the HALL adds ~+1.9 dB of room behind an untouched dry
  voice at the 0.7 return trim (the old 30 % Mix crossfade left the dry at 70 %, so the
  reverb-to-voice balance is the same and the voice is intact); the ROOM at full wet measures
  a 0.51 wet-to-voice RMS ratio with the dry term gone (difference 0.66 > 0.58, i.e. a
  crossfade, not an additive sum).
- **Plugin build**: `ninja OpenVoxTuner` up to date, no new warnings from the changed files.
