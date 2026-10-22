# Changelog — 2026-10-22

## Feature — Delay divisions: a duration-ordered ladder with dotted and triplet values

### Context

The delay's division knob offered five note values whose labels ended with a dot: `1/8.`, `1/4.`. The
dot is the standard note-value prolongation point (a dotted eighth is 1.5 x an eighth), but on a knob
popup a trailing dot is easy to read as a typo or as the DAW "triplet" mark, which is `T`. The knob
now spells the note families out, and every plain value has both of its siblings: a dotted entry and
a triplet entry. The mapping was still being settled on the development branch before the merge, so
the table was rebuilt as one duration ladder instead of freezing the original order with the new
values appended.

### What shipped

- **Nine divisions, ordered by repetition length** (shortest first), the order the knob turns in:
  `1/8T`, `1/8`, `1/4T`, `1/8D`, `1/4` (default), `1/2T`, `1/4D`, `1/2`, `1/2D`.
- **Fractions in beats**: `{1/3, 0.5, 2/3, 0.75, 1.0, 4/3, 1.5, 2.0, 3.0}` — at 120 BPM the copy
  lengths are 8 000 / 12 000 / 16 000 / 18 000 / 24 000 / 32 000 / 36 000 / 48 000 / 72 000 samples.
- **Notation**: `D` = dotted (1.5 x the plain value), `T` = triplet (2/3 of the plain value). The
  dotted values keep their exact fractions (1/8D = 0.75 beat, 1/4D = 1.5 beats); the triplets are
  real divisions. A triplet eighth is NOT 0.75 of a beat, so the old dotted entries were not
  relabelled as triplets.
- **Long end added**: `1/2T` = 4/3 of a beat and `1/2D` = 3 beats (the classic long ambient echo).

### Design facts

- **The short end stops at `1/8T` on purpose.** The stage treats anything under 40 ms as a comb
  filter, not an echo, and clamps there. `1/8T` is the shortest value that stays above that floor at
  every legal tempo (50 ms at the 400 BPM ceiling), so every label on the knob is served exactly.
  Going below it (`1/16T`, `1/32`, `1/32T`) would be clamped at common tempos — `1/32T` is 41.7 ms at
  120 BPM and clamps above ~125 BPM — so the knob would lie, and the musical result is comb
  coloration rather than a repeat. The `1/16` family is left out for the same reason plus role
  overlap: `1/16` is 125 ms at 120 BPM, a slapback rather than a rhythmic echo, and the thickening
  role is already covered by the Doubler; the delay's identity is tempo-synced repeats. If a genre
  need appears later, add `1/16D` (0.375 beat, still above the floor up to the 400 BPM ceiling);
  `1/16` itself only holds up to 375 BPM and would need that edge accepted.
- **Capacity extended to the new longest division**: `maxCopySeconds()` goes from 6 s (bpm 20 x 2
  beats) to 9 s (bpm 20 x 3 beats = `1/2D`), so the longest request is still prepared once in
  `prepare()` and never clamped at the top. The ring is rounded up to a power of two, so at the common
  44.1 / 48 / 96 / 192 kHz rates 9 s fits the same sample count 6 s used (524 288 samples at 48 kHz);
  only an unusual rate such as 32 kHz would step up one block count.
- **State compatibility is deliberately not preserved.** The plugin state, the preset files and the
  A/B slot XML store the division as a choice INDEX, and the re-spacing moves every index (the default
  `1/4` goes from index 2 to index 4). A session, preset or slot saved by a pre-release build re-maps
  its division on load, and a host automation lane on `delay_division` (normalised) re-maps too. That
  is accepted because the mapping was corrected before the dev branch merged; no migration was added,
  since freezing the ordering now is cheaper than supporting two index maps forever.
- Four places know the count and move together: `DelayEffect::kDivisionCount`/`kDivisionFractions`,
  the APVTS `AudioParameterChoice` (nine labels, default index 4), the editor knob (detented
  `0..8 step 1`, popup labels in the same order) and `PresetMorpher`'s normalised index scaling
  (index / 8, default 4).
- Tooltips in all six locales list the ladder in the same short-to-long order and spell out
  `D` = dotted, `T` = triplet.

### Testing

- `DelayEffectTest`'s division-mapping group walks **all nine indices**: each target copy length is
  checked against `round((60/bpm) * fraction * sr)` and against an impulse echo arriving in the
  opposite channel (±3 samples); at 120 BPM the ladder is the nine counts above. The clamping group's
  worst case is now bpm 20 x `1/2D` = 9 s, exactly the prepared maximum, with the capacity assertion
  updated.
- Full `OpenVoxTunerTests` run: **`Result: 222 OK, 0 FAILED`**.
- Docs: `docs/reference/dsp-api.md` and the roadmap's delay entry updated to the new table, the 9 s
  worst case and the comb-floor rationale for the short end.
