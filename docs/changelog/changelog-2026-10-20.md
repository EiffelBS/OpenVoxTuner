# Changelog — 2026-10-20

## Feature — Knob captions run vertically, in the frame spines' typography

Asked while testing the reverb frame: *"les labels Room et Hall sur la gauche de leurs knobs,
verticaux et SmallCaps, un peu comme le bandeau titre des frames qu'on a ajouté récemment dans
EBS ?"*

- **New `Source/ui/OVTVerticalCaption.h`**: a `juce::Label` subclass that paints its text rotated
  bottom-to-top with the design system's own spine recipe — `ebs::bandCaps` (UPPERCASE, accents
  dropped, safe in every locale), `ebs::kBandLetterSpacing` tracking, `ebs::kBandTextAlpha` at
  50 %, glyphs shrinking from `kBandFontSize` towards `kBandMinFontSize` instead of clipping. It
  is NOT an `ebs::FrameTitleBand`: a spine names a whole frame and therefore spends width on a
  tinted strip and an inner trim, while a caption labels one dial — five tinted strips inside the
  Reverb frame would read as five sub-frames. Because it derives from `juce::Label`, it rides the
  existing i18n refresh (`translatableLabels`) with no new keys.
- **Reverb frame**: the four amount captions (Lead/Harmony × Room/Hall) and the Ducking caption now
  sit vertically at their dial's left. The 14 px a horizontal caption used above each dial go back
  to the dial: the amount cells measure 65 px, so the four dials reach the shared `effKnobD` = 54 px
  instead of the 49 px a top caption allowed, matching the Effects block. The Ducking dial is
  centred in its tall column instead of sitting under a strip of text.
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
