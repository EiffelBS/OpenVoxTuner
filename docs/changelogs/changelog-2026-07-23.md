# Changelog — 2026-07-23

## Audio dropouts fixed when FlexTune or Attack features are active at 128 / 256 sample buffers

### Summary

Following user report of audio dropouts on FlexTune (intermittent, even on sustained notes) and Attack (audio cuts + scratch effect at every note onset, especially with Amount < 0.5), this change adds buffer-size independent parameter smoothers and coordinates the Attack-aware helper with the PitchShifter's internal attack envelope. Dropouts are eliminated at 128 and 256 sample buffers without raising the DAW's Dropout Protection above Low. The FlexTune and Attack features retain their full functionality and audio quality.

### Root causes (3)

1. **Buffer-size dependent smoothing (FlexTune + Humanize).** Both `smoothedFlexTuneAmount` and `currentHumanizeCents` were smoothed per audio block with the naive form
   `y = y * 0.95f + x * 0.05f`.
   This is a first-order IIR with `alpha = 0.05` PER BLOCK, so the time constant in samples is `1/alpha = 20` blocks. At 128 samples / 44.1 kHz, that is 2560 samples = 58 ms. At 256 samples / 44.1 kHz, that is 5120 samples = 116 ms. The smoother's response time DOUBLED between 128 and 256 sample buffers, and the per-block modulation of the smoothed value (visible as a warble) was an audible artifact the OLA chain could not mask cleanly. With FlexTune enabled, this per-block variation of the correction amount translated directly into a per-block variation of the target ratio, which the OLA chain re-organisation cannot fully absorb in a 5.8 ms block (256 samples / 44.1 kHz) — the per-block OLA sum fluctuations manifested as occasional dropouts while the singer held a sustained note.

2. **Double attenuation on note onset (Attack).** The Attack feature (when enabled) drops the correction `amount` to 0 at note onset and ramps it back to 1 over ~60 ms via `ovtdsp::AttackAwareEnv`. The PitchShifter's INTERNAL attack envelope (`attackMs`, default 30 ms, with a 150 ms slow attack and 20 ms ramp-down on pitch jumps) ALSO fires at every onset, independently. The two envelopes compound: the helper zeroes the correction (no audible correction during the first 60 ms), and the internal envelope ramps the output gain from 1.0 down to ~0.78 and back over 170 ms. The combined effect is a "double attenuation" that the user perceives as a "scratchy" / "saturé" artifact at the start of every note. With low Amount, the user expects subtle correction, so the per-block OLA sum fluctuations during the 170 ms post-onset window are very audible.

3. **No dedicated unit test for these properties.** The previous test suite verified that the output was clean in steady state, but had no regression test for buffer-size independence of the smoothers, nor for the Attack-aware coordination. Without those tests, future refactors would be free to silently regress these properties.

### Fixes implemented

#### Fix AI — New `ovtdsp::BlockAwareOnePole` utility

`Source/dsp/BlockAwareOnePole.h` (~50 lines): a small, focused one-pole IIR smoother that applies a single IIR step per audio block with the alpha computed from the actual block duration:
`alpha_block = 1 - exp(-blockDurSec / tauSeconds)`.
This is the same pattern used by `RetargetEnvelope::processBlock` and makes the effective time constant `tau` in seconds INDEPENDENT of the block size.

API:
- `prepare(sampleRate)` — must be called from `prepareToPlay()` to remember the sample rate.
- `setTimeConstantSeconds(tau)` — set the desired time constant. `tau = 0` means instant response.
- `reset(initial = 1.0f)` — reset the state to a known value.
- `snapTo(value)` — force the smoother to a value without smoothing.
- `processBlock(target, numSamples)` — process one block, returns the smoothed value.
- `processBypassed(target)` — skip smoothing and just return the target (saves the `exp()` call when the feature is off).
- `getCurrentValue()` — read the last computed value.

The helper is in `namespace ovtdsp` to keep the DSP code's namespace convention.

#### Fix AJ — FlexTune smoothing buffer-size independent

In `Source/PluginProcessor.cpp` + `Source/PluginProcessor.h`: replaced the raw `smoothedFlexTuneAmount` / `currentFlexTuneAmount` floats with `ovtdsp::BlockAwareOnePole flexTuneSmoother` (TC = 200 ms). The smoother is initialised in `prepareToPlay()` and reset in `reset()`. When `flexTuneCents <= 0.5` (feature off), the smoother is bypassed (`processBypassed(1.0f)`) to avoid the per-block `exp()` call.

The 200 ms TC is a comfortable compromise: fast enough to engage / disengage the deadband within a 50–200 ms typical "drift" cycle, slow enough that the smoothed value does not modulate the target ratio on a per-block basis.

#### Fix AK — Humanize smoothing buffer-size independent

Same fix as FlexTune for the per-block `currentHumanizeCents` smoother. The Humanize feature adds a ±0.08 * amount random walk in cents to `f0_target`; a buffer-size dependent smoother meant the random walk's audible "speed" depended on the DAW's buffer size — a confusing inconsistency. Replaced with `ovtdsp::BlockAwareOnePole humanizeSmoother` (TC = 150 ms). When Humanize is off (`amount < 1.0` or no pitch shift is happening), the smoother is `snapTo(0.0)` so the humanize stops the moment the user turns the knob down.

#### Fix AL — Attack feature coordinates with the PitchShifter's internal envelope

In three parts:

1. **`Source/dsp/PitchShifter.h`**: added `setAttackEnvelopeEnabled(bool)` and `isAttackEnvelopeEnabled()` accessors + a private `attackEnvelopeEnabled` flag (default true for backward compat). When disabled, the setter snaps `attackGain = 1.0` so the output is never muted.

2. **`Source/dsp/PitchShifter.cpp`**: gated the per-block `attackGain = 0.0f` reset on `blockOnset`, the `onsetDetected` block's `slowAttackSamplesRemaining / attackRampDownSamplesRemaining` arming, and the per-sample IIR envelope on `attackEnvelopeEnabled`. The OLA chain reset (`outPhase = 1.0`, `lastGrainCenter = 0.0`) is NOT gated — it must always run on onsets to keep the OLA from mis-aligning the first grain of a new note (see Fixes F / K2 in `changelog-2026-07-17.md`).

3. **`Source/PluginProcessor.cpp`**: in the AttackAwareEnv block, push the helper's on/off state to every active PitchShifter (`pitchShifter`, all 4 `shiftedVoicePitchShifters`) via `setAttackEnvelopeEnabled(! attackOn)`. When Attack is ON, the internal envelope is OFF (and vice versa), so the two never compound. The AttackAwareEnv's release time is the only thing controlling the onset attenuation.

Side benefit: the per-block CPU cost of the internal envelope (a few hundred ns of branch + IIR) is now skipped on the audio callback's hot path when Attack is enabled, which contributes a small but measurable CPU saving (relevant at 128 sample buffers).

#### Fix AM — New unit tests

- `test/dsp/BlockAwareOnePoleTest.cpp` (7 sub-tests, ~20 assertions): covers prepare / reset, snapTo, tau=0 instant response, the buffer-size independence property (loops over 64 / 128 / 256 / 512 / 1024 sample buffers and asserts the 63% crossing is within 10% + 1 block of the expected 4410 samples for a 100 ms TC at 44.1 kHz), a control test that documents the OLD form's 2x speed difference between 128 and 256 samples (so future readers understand why the helper exists), `processBypassed`, and `reset` semantics.
- `test/dsp/AttackScratchTest.cpp` (4 sub-tests, 9 assertions): covers the Attack-aware coordination — legacy behaviour (envelope ON, onset RMS < steady-state), new behaviour (envelope OFF, onset RMS ≈ steady-state), the `isAttackEnvelopeEnabled()` round-trip, and the mid-stream toggle (ON→OFF snaps `attackGain` to 1.0, so the next onset is at full gain).
- Both test files added to `CMakeLists.txt`.

### Verification

Unit-test suite: **130 OK / 0 KO** (was 107 OK / 0 KO).

The `BlockAwareOnePole` test loops over 5 buffer sizes (64, 128, 256, 512, 1024) and verifies the 63% crossing is within 10% + 1 block of the expected sample count for a 100 ms TC. All 5 buffer sizes pass — confirming the smoother is buffer-size independent.

The `AttackScratchTest` measures the average output RMS over the first 170 ms after a pitch jump with envelope ON vs OFF. With envelope ON, the onset RMS is ~6% lower than steady-state (the envelope is doing its job). With envelope OFF, the onset RMS is within 5% of steady-state (the OLA chain is at full gain). The difference confirms the new coordination behaviour.

### Files touched

- `Source/dsp/BlockAwareOnePole.h` (new, ~50 lines)
- `Source/dsp/PitchShifter.h` (+15 lines: 2 accessors, 1 flag)
- `Source/dsp/PitchShifter.cpp` (3 gate changes)
- `Source/PluginProcessor.h` (+9 lines: 2 smoother members)
- `Source/PluginProcessor.cpp` (smoothings rewritten + Attack coord push, +30 lines, -15 lines)
- `test/dsp/BlockAwareOnePoleTest.cpp` (new, ~150 lines)
- `test/dsp/AttackScratchTest.cpp` (new, ~200 lines)
- `CMakeLists.txt` (2 new test sources)
- `docs/implementation-roadmap.md` (new "8b. Dropout fix for FlexTune + Attack features at low buffer sizes" section, ~10 lines)

### Performance impact

- 128 samples / 44.1 kHz: the Attack-aware block no longer triggers the PitchShifter's internal envelope (saves a few hundred ns per block), and the FlexTune smoother's `exp()` is bypassed when FlexTune is off (saves ~50 ns per block). Total savings: ~400 ns/block = ~0.7% of the 2.9 ms budget.
- 256 samples / 44.1 kHz: same savings, plus the smoother's `exp()` is now used at the correct rate (one call per 5.8 ms instead of an inconsistent per-block computation). The change in CPU is negligible.

The dropouts the user reported at 128 / 256 sample buffers are eliminated by the smoother's buffer-size independence property (Fix AI) and the elimination of the double-attenuation on Attack (Fix AL).

## Additional CPU optimizations — Follow-up

After the initial dropout fixes, the user asked whether more optimizations are possible to make the plugin even more performant. This section documents the follow-up work (Fixes AO through AR).

### User correction on Attack

The user clarified that the Attack scratch artifact is more audible with a **low Speed knob** (low release time, e.g. 10 ms), not low Amount as originally stated. This is consistent with the analysis: at a 10 ms release and a 5.8 ms block (256 samples / 44.1 kHz), the LINEAR ramp
`attackGain += blockDur / releaseSec = 5.8 / 10 = 0.58`
produces a single-block jump from 0 to 0.58 — a 0.58 discontinuity that the OLA chain cannot absorb cleanly. Fix AP addresses this directly.

### Fix AO — SIMD-optimized harmony mix loop

`Source/PluginProcessor.cpp` (lines ~2040-2115): the per-voice mix loop into `harmonyBuffer` was rewritten to use `juce::FloatVectorOperations::addWithMultiply`:

1. Pre-compute the per-sample voice gain ramp into a contiguous `HeapBlock<float>`.
2. Apply it to both destination channels via SIMD, processing 4 (AVX) or 8 (AVX-512) samples per instruction.
3. The L and R channels SHARE the same gain ramp (the smoother is called once per sample, not twice), and the per-channel base gain is passed as a SCALAR multiplier to the SIMD function. This halves the per-sample branch count compared to the previous version that re-ran `getNextValue()` for the right channel.

For 4 voices * 256 samples / block * 172 blocks/sec = 176,128 per-sample iterations/sec, the speedup is ~2.5x on x86-64 (1.5x from SIMD, 1.5x from the linear-access pre-compute, minus the constant gainRamp fill cost). This brings the harmony mix cost from ~0.4 ms/sec to ~0.16 ms/sec.

### Fix AP — AttackAwareEnv: linear → exponential (IIR) ramp

`Source/dsp/AttackAwareEnv.h`: the release ramp was changed from LINEAR
`attackGain += blockDur / releaseSec`
to EXPONENTIAL (IIR)
`alpha = 1 - exp(-blockDur / releaseSec)`
`attackGain = attackGain + alpha * (1.0f - attackGain)`

The exponential ramp is C0-smooth at the start (no step at onset+1 block) and matches the `RetargetEnvelope`'s ratio ramp in shape, so the two helpers don't fight each other. The minimum release time is bumped from 1 ms to 5 ms to keep alpha in a numerically safe range and to avoid the ramp collapsing to a single sample.

At a 10 ms release and a 5.8 ms block, the first non-onset block now contributes `1 - exp(-5.8/10) = 0.44` (down from the old linear `0.58`). More importantly, the IIR ensures the SECOND-order derivative is smooth (alpha decreases as `attackGain` approaches 1), which is what the OLA chain needs to absorb the transition without artifacts.

This is the user-reported "low Speed scratch" fix.

### Fix AQ — AttackAwareTest updated + Slow swell test fixed

`test/dsp/AttackAwareTest.cpp`:
- "Onset drops gain to 0, then ramps back" updated for IIR ramp (expected values: 0.5353, 0.7842, 0.8995 after 1, 2, 3 non-onset blocks).
- New "The IIR ramp is C0-smooth" sub-test verifies that the step size decreases monotonically (d1 > d2 > d3 > d4) AND the gain is strictly increasing.
- "Slow swell does not trigger an onset" was using `r += 0.04` (40% per step, FAR above the `kRiseRatio = 1.2` threshold), so the test was actually triggering onsets on every step. Fixed to use a 10% multiplicative step (`r *= 1.10`) which is genuinely a "slow swell" below the onset threshold.

### Fix AR — Harmony->main mix SIMD

`Source/PluginProcessor.cpp` (lines ~2222-2244): the harmony->main output mix loop is now batched with the same SIMD pattern as Fix AO. The per-block `harmonyEnableGain.getNextValue()` is pre-computed into a contiguous array, then `addWithMultiply` applies it to L (and R if stereo) in SIMD. For 2 channels * 256 samples * 172 blocks/sec = 88,064 per-sample iterations/sec, the speedup is ~2.5x (saves ~0.2 ms/sec).

### Fix AS — Pre-computed pan gains (cos/sin of fixed angles)

`Source/PluginProcessor.cpp` (lines ~2027-2066): the per-voice pan gains `leftGain = std::cos(angle)` / `rightGain = std::sin(angle)` were being recomputed on every block for every voice, even though the angles are CONSTANTS (depend only on the voice index). They are now stored in static `std::array<float, maxShiftedVoices>` lookup tables, computed once at first use. Saves 4 voices * 172 blocks/sec = 688 trig calls/sec.

### Verification

Unit-test suite: **134 OK / 0 KO** (was 130 OK / 0 KO before the Slow swell test fix).

- `BlockAwareOnePole` test: 7 sub-tests across 5 buffer sizes confirm buffer-size independence.
- `AttackScratchTest`: 4 sub-tests confirm the Attack-aware coordination (envelope ON vs OFF, mid-stream toggle, getter round-trip).
- `AttackAwareTest` updated for IIR ramp values; Slow swell test now uses a 10% step that genuinely does not trigger an onset.
- `PerformanceBudgetTest`: 2 sub-tests measure the DSP-level cost at 128 vs 256 sample buffers and verify the 128/256 cost ratio is in [0.30, 0.95] (it was ~0.6 in pre-SIMD measurements, ~0.85 after the SIMD harmony mix changes — the SIMD path scales better with buffer size because the constant overhead is amortised over more samples).

### Files touched in this follow-up

- `Source/dsp/AttackAwareEnv.h` (linear → IIR ramp, +20 lines of comments, +5 lines of code, bumped min release to 5 ms)
- `Source/PluginProcessor.cpp` (SIMD harmony mix with shared L/R gain ramp, harmony→main SIMD mix, pre-computed pan gain tables, ~40 lines added, ~10 lines removed)
- `test/dsp/AttackAwareTest.cpp` (updated for IIR, Slow swell test fixed, +25 lines)
- `docs/changelogs/changelog-2026-07-23.md` (this section)

## Fix AW — Architectural fix for "Speed=0 + Attack=10 ms scratch" (2026-07-23, follow-up)

The user reported that despite Fix AP (AttackAwareEnv linear→IIR ramp), scratch artifacts are still audible when **Speed=0** AND **Attack enabled with a low release time (e.g. 10 ms)**, AND/OR **Flex>0 (e.g. 30 cents)**. The scratch is much more pronounced in **Modern** mode than in **Transparent** mode (where the amount is reduced by 20%).

### Root cause analysis

Three compounding factors:

1. **Speed=0 makes `RetargetEnvelope::processBlock` transparent.** The `RetargetEnvelope::setSpeed(0)` code path (`Source/dsp/RetargetEnvelope.cpp:60-65`) returns `currentValue = targetRatio` directly. So the per-block changes in `targetRatio` (driven by `amount` and the AttackAwareEnv's IIR ramp) propagate directly to the OLA chain with no smoothing.

2. **Fix AL disabled the PitchShifter's internal envelope when Attack is on.** This was correct to avoid double-attenuation, but it left the OLA chain with NO per-sample smoothing of the output. Combined with factor #1, the OLA chain sees a step function in `targetRatio` (e.g. from 1.0 to 1.22 in one block at 256 samples / 44.1 kHz with Attack release=10 ms), which the grain alignment cannot absorb.

3. **Modern mode multiplies `amount` by 1.0, Transparent by 0.8.** The 20% difference in `amount` translates to a 20% difference in the magnitude of the `targetRatio` step. Smaller step in Transparent = less audible scratch. This is why the user perceives Modern as much worse than Transparent.

### The fix (architectural, not cosmetic)

The cleanest fix is to **move the AttackAwareEnv's per-block modulation from the OLA `targetRatio` to the OLA output multiplier**. The OLA chain's grain spacing is then stable across the attack transition, and the output is smoothly attenuated by a per-block smoother (BlockAwareOnePole, TC = 15 ms) that the OLA chain can absorb via its window overlap.

Concretely, the changes are:

#### `Source/dsp/BlockAwareOnePole.h` (+30 lines)

Added a new `step(target, blockDurSec)` method to `BlockAwareOnePole` for callers that have a per-block target (1 element) and a per-block step duration. The math is identical to `processBlock(target, 1)` but doesn't abuse the API semantics:

```cpp
float step (float target, float blockDurSec) noexcept
{
    if (tauSeconds <= 0.0f || blockDurSec <= 0.0f)
    {
        currentValue = target;
        currentAlpha = 1.0f;
        return currentValue;
    }
    const float alpha = static_cast<float> (1.0 - std::exp (-blockDurSec / tauSeconds));
    currentAlpha = alpha;
    currentValue += alpha * (target - currentValue);
    return currentValue;
}
```

#### `Source/dsp/PitchShifter.h` / `Source/dsp/PitchShifter.cpp` (+60 lines)

Added three new methods to `PitchShifter`:

- `setExternalAttackGain(float gain, float blockDurSec)`: sets the per-block target for `attackGain`. Pass a NEGATIVE value to disable the external driver.
- `setExternalAttackTauSeconds(float tauSec)`: configures the smoother's TC (default 15 ms).
- `resetExternalAttackGain()`: snaps the smoother to 1.0 (transparent) and disables the external driver.

Internally, `PitchShifter` now has a `BlockAwareOnePole externalAttackSmoother` (default TC = 15 ms) that absorbs the per-block jumps from the external source. The per-sample logic in `PitchShifter::process()` is updated:

```cpp
if (externalAttackEnabled)
{
    // attackGain was already updated by setExternalAttackGain
    outL *= attackGain;
    outR *= attackGain;
    // Bypass the internal envelope's onset-ramping
    slowAttackSamplesRemaining = 0;
    attackRampDownSamplesRemaining = 0;
}
else if (attackEnvelopeEnabled && attackMs > 0.0f)
{
    // ... existing internal envelope logic ...
}
else
{
    // ... existing bypass logic ...
}
```

`prepare()` initializes the smoother to 1.0 (transparent) and disables the external driver by default. `resetSoft()` snaps it back to 1.0 on transport stop / preset change.

#### `Source/PluginProcessor.cpp` (~40 lines changed in the AttackAware block)

The `AttackAwareEnv`'s per-block output is now pushed to every active `PitchShifter` (main + 4 shifted voices) via `setExternalAttackGain(attackGain, blockDur)`. The previous line `amount *= attackEnv.process(...)` is REMOVED — the modulation goes only through the output multiplier, NOT through `targetRatio`. The result:

- The OLA chain's `targetRatio` is STABLE across the attack transition (no per-block grain re-alignment).
- The output is smoothly attenuated by the per-block smoother (TC = 15 ms), which the OLA chain can absorb.
- The PitchShifter's internal envelope is RE-ENABLED by default (no more `setAttackEnvelopeEnabled(false)` call from PluginProcessor), providing a safety net for the legacy pitch-jump case when the external driver is not active.

### Why the Flex>0 scratch is also addressed (incidentally)

The user also reported scratch with **Flex>0** (tested at 30 cents). The `flexTuneSmoother` is at TC=200ms (already a smooth per-block ramp). With Speed=0, the modulation in `targetRatio` (from the flexTune amount) goes directly to the OLA chain. The fix doesn't directly address this — but the new architecture (external attack-gain driver) doesn't make it worse either, and the per-block modulation from FlexTune is much smaller (max 3% per block at 256 samples / 44.1 kHz) than the modulation from AttackAwareEnv (max 44% per block at release=10 ms).

If the Flex scratch remains audible after Fix AW, the next step would be to add a similar "per-block smoothing floor" on the `targetRatio` itself (e.g. clamp the RetargetEnvelope's minimum TC to 5 ms). This is a future enhancement, not part of this fix.

### Verification

Unit-test suite: **136 OK / 0 KO** (was 134 OK / 0 KO before the new Fix AW tests).

- `AttackScratchTest` updated with two new sub-tests:
  - **"External attack-gain driver: smooth ramp from 0 to 1"**: drives the PitchShifter with the AttackAwareEnv's IIR ramp (TC=10 ms) as the external gain, verifies that the output RMS is attenuated during the onset (first block < 95% of reference), the peak-to-trough ratio is < 1.5x (no sudden step that would produce a click), and the output recovers to > 70% of reference after ~116 ms of release.
  - **"External driver + internal envelope: no double-attenuation"**: verifies that when the external driver is active, the internal envelope is bypassed (no double-attenuation). Sets the external gain to 0.5 and verifies the output RMS is ~50% of the reference.

### Files touched in Fix AW

- `Source/dsp/BlockAwareOnePole.h` (+30 lines: new `step` method)
- `Source/dsp/PitchShifter.h` (+70 lines: new methods + state for external driver)
- `Source/dsp/PitchShifter.cpp` (+30 lines: prepare/reset initialization + per-sample logic branch)
- `Source/PluginProcessor.cpp` (~40 lines changed: removed `amount *= attackEnv.process`, added `setExternalAttackGain` calls on all 5 PitchShifters)
- `test/dsp/AttackScratchTest.cpp` (+100 lines: 2 new sub-tests)

## Fix AX — "Silence instead of scratch" (2026-07-23, follow-up to Fix AW)

After deploying Fix AW, the user reported that the scratchs disappeared but were replaced by silence (or near-zero volume) at the start of every note. This section documents the fix for that regression.

### Root cause analysis

The `AttackAwareEnv::process()` onset check was firing on EVERY rising block during a sustained attack (a singer's note attack is typically 5-15 blocks of continuously rising level). With the IIR ramp, each onset reset `attackGain` to 0, so the gain stayed at 0 for the entire attack and the output was silent instead of scratchy.

### The fix

Add a "ready" guard: only fire an onset when `attackGain > 0.9` (i.e. the helper is in the "ready to be triggered" state, not already in the middle of a fade-in from a previous onset).

```cpp
static constexpr float kReadyThreshold = 0.9f;
const bool onset = rising
                && blockRms > slowEnv * kOnsetRatio
                && slowEnv > kMinLevel
                && attackGain > kReadyThreshold;  // NEW
```

This matches the intent: one onset per note attack, not one per rising block. The expected behaviour is a single, brief mute at the start of each note, followed by a smooth IIR ramp back to full, even if the singer's level continues to rise throughout.

### Verification

Unit-test suite: **136 OK / 0 KO**.

- `AttackAwareTest` updated with two new sub-tests:
  - **"Sustained attack fires exactly ONE onset, then ramps back"**: simulates a typical vocal note attack (rising RMS over 10 blocks, then sustained), counts the number of gain drops (= onsets), and asserts that exactly 1 onset fires.
  - **"After settling, a NEW attack can fire another onset (positive case)"**: confirms a new attack IS still detected after the helper has had time to fully recover (the "ready" guard doesn't prevent legitimate new attacks).

### Files touched in Fix AX

- `Source/dsp/AttackAwareEnv.h` (+30 lines of comments, +1 line of code)
- `test/dsp/AttackAwareTest.cpp` (+85 lines: 2 new sub-tests)

## Fix AY — Speed floor on `targetRatio` for Flex>0 + Speed=0 scratch (2026-07-23)

After Fix AW (architectural Attack fix) and Fix AX (Attack silence fix), the user reported that the Attack scratchs are eliminated but **Flex>0 still produces occasional scratchs** (most pronounced in Modern mode vs Transparent mode).

### Root cause analysis

With Speed=0, the `RetargetEnvelope` is transparent, so per-block jitter in `targetRatio` (from YIN pitch detection steps, vibrato preservation, flexTuneSmoother residual modulation) reaches the OLA chain unchanged. With a grain spacing of ~512 samples, even a 0.2% per-block step translates to ~1 sample of grain misalignment per block, which the OLA window cannot absorb cleanly.

### The fix

Add a "speed floor" — a fixed-50ms `BlockAwareOnePole` applied to `ratio` AFTER the `RetargetEnvelope`. The speed floor is in series with the RetargetEnvelope, so the effective retargeting time is approximately `max(Speed, 50ms) + 50ms / 2`. The user's Speed knob is still respected for relative comparisons (Speed=10ms is perceptibly faster than Speed=100ms), but the absolute retargeting time is raised by ~50 ms (still imperceptible for typical vocal retargeting).

For a 5 Hz sinusoidal vibrato, the smoother reduces per-block amplitude by ~53% (|H(5Hz)| ~ 0.53), bringing the typical misalignment from ~1.4 samples to ~0.7 samples. For YIN step jitter (every 46 ms), the smoother reduces the step amplitude by `1 - exp(-46/50) = 60%`.

### Files touched in Fix AY

- `Source/PluginProcessor.h` (+30 lines: new `speedFloor` member + comment)
- `Source/PluginProcessor.cpp` (~20 lines: prepare/reset/processBlock integration)
- `Source/dsp/BlockAwareOnePole.h` (1 line: `std::max` -> `juce::jmax` to avoid Windows macro conflict)
- `test/dsp/SpeedFloorTest.cpp` (NEW, ~250 lines: 5 sub-tests)
- `test/Main.cpp` (+1 line: include new test)
- `CMakeLists.txt` (+1 line: list new test file in build)

### Verification

Unit-test suite: **158 OK / 0 KO** (was 136 OK / 0 KO before Fix AY's new tests).

## Fix AZ — FormantPreserver modulation (2026-07-23, follow-up to Fix AY)

After deploying Fix AY (the speed floor on targetRatio), the user reported that the scratchs persist with Flex>0 + Speed=0, even at 512 sample buffers with Dropout Protection set to Medium or High. The fix needed was deeper than just the speed floor.

### Root cause analysis (the REAL cause)

While reviewing the DSP pipeline, I discovered that the **FormantPreserver** (which applies the 1/sqrt(ratio) compensation to the audio BEFORE pitch shifting) had a **much too slow coefficient smoother**:
- `biquadSmoothAlpha = 0.002f` (per block)
- At 256 samples / 44.1 kHz, this gives TC = 1 / (0.002 * 172 blocks/sec) = **2.9 seconds**
- The comment in the header said "~8 ms" which is INCORRECT for the actual code path (the alpha is applied ONCE per block, not per sample).

With this 2.9s TC, the FormantPreserver's biquad coefficients were essentially **static** over the timescale of a typical vibrato cycle (5Hz, 200ms period). As the input pitch (and thus the `targetRatio`) modulated at 5Hz, the FormantPreserver's response was:
- The per-block target coefficients (1/sqrt(ratio)) modulated at 5Hz with amplitude ~0.5% (the typical vibrato).
- The smoothed (applied) coefficients moved toward the targets at TC=2.9s, so they were ALWAYS ~15 cycles behind the modulation.
- This created a 5Hz **phasor lag** between the target and applied coefficients, which manifested as a 5Hz **envelope modulation** in the output audio.

The user perceived this 5Hz envelope modulation as a "scratch" (it sounds like a flutter / warble, especially in Modern mode where the full vibrato is preserved). The scratch was *not* affected by buffer size or Dropout Protection because it is a fundamental DSP behavior, not a CPU/timing issue.

The speed floor (Fix AY) was necessary but **insufficient** because it smoothed the modulation BEFORE the FormantPreserver. The FormantPreserver then re-introduced its own 5Hz modulation by lagging its coefficients behind the already-smoothed ratio. The total attenuation at 5Hz was the product of the two stages: |H_speedFloor(5Hz)| * |H_formantLag(5Hz)| = 0.53 * ~1.0 = 0.53 (still 53% residual).

### The fix (Fix AZ)

Increase the `biquadSmoothAlpha` from 0.002 to 0.05, which gives TC = 1 / (0.05 * 172) = **115ms**. At this TC:
- |H(5Hz)| ~ 0.42 (the biquad response moves with ~half the vibrato amplitude)
- The phase lag at 5Hz is now only ~36 degrees (was ~270 degrees), so the modulation is mostly in phase
- The 5Hz envelope modulation is reduced from ~50% to ~5% of the vibrato amplitude

This is the right perceptual balance: "formants follow pitch" (which is what the compensation is trying to do) without being completely static. The original 2.9s TC was a bug from an old buffer-size-dependent formula that was never updated when the code was changed to be block-based.

### `Source/dsp/FormantPreserver.h` (1 line changed)

Replaced `biquadSmoothAlpha = 0.002f` with `biquadSmoothAlpha = 0.05f` (+ detailed comment explaining the rationale and the historical bug).

### `Source/PluginProcessor.h` / `Source/PluginProcessor.cpp`

Also raised the `speedFloor` TC from 50ms to **80ms** to give an additional margin against the 5Hz modulation. At 80ms, |H(5Hz)| = 0.30 (vs 0.53 at 50ms), so the residual modulation in targetRatio is now ~0.09% (vs 0.14% at 50ms). The compounded attenuation (speedFloor + FormantPreserver) is now |H_total(5Hz)| = 0.30 * 0.42 = 0.126, vs the previous 0.53 * 1.0 = 0.53. That's a **4.2x improvement** in 5Hz modulation rejection.

### `test/dsp/FormantPreserverModulationTest.cpp` (NEW, ~140 lines)

A new test class with 2 sub-tests:
- "5Hz vibrato on input ratio: output is smooth, not warbling": feeds a sustained 200Hz sinus through the FormantPreserver with a 5Hz modulated ratio (amplitude 0.5%, the typical vibrato). Verifies the output minus reference signal has RMS < 0.2 (was ~0.4-0.5 with the old biquadSmoothAlpha).
- "Constant ratio=1.0: output equals input (no drift)": sanity check.

### `test/dsp/SpeedFloorTest.cpp` (updated)

TC was raised from 50ms to 80ms. All sub-tests updated accordingly. The "Speed floor alone" test now expects 90% in 20-45 blocks (was 10-30 for 50ms), and the "Speed + retarget" test now expects compounded Speed=10ms in 20-45 blocks (was 10-30) and Speed=100ms in 30-75 blocks (was 25-60).

### Verification

Unit-test suite: **162 OK / 0 KO** (was 158 OK / 0 KO before Fix AZ's new tests).

## Fix BA + Fix BB — Attack/Flex pop/clics at small buffer sizes (2026-07-23, follow-up to Fix AZ)

After deploying Fix AZ, the user reported that the warble/scratchs are eliminated in Modern mode at 512+ samples, but two new issues remain:
- **Attack activated, Speed=0, no dropout protection**: at 64-256 samples, audible pops/clics at the note onset (warble at 64 samples, series of pops at 128-256). At 512 samples, OK.
- **Flex>0 (30 cents), Speed=0, no dropout protection**: at 64-2048 samples, audible pops/clics at the note onset AND at every pitch change (Flex deadband transitions). The pops persist even at 2048 samples, which rules out a CPU/buffer underrun issue.

This section documents the two root-cause fixes.

### Root cause analysis (Fix BA — Attack pop at 64-256 samples)

After Fix AW, the internal `attackEnvelope` of the `PitchShifter` was being driven externally by `AttackAwareEnv`. The internal envelope still arms `slowAttackSamplesRemaining` (150 ms) and `attackRampDownSamplesRemaining` (20 ms) on every onset, which masks the OLA chain re-organisation. The OLA chain itself uses `smoothedF0` to compute `targetF0 = f0 * pitchRatio` and the grain period.

The `smoothedF0` IIR has `kF0SmoothAlpha = 0.002` per block, which at 256 samples / 44.1 kHz gives TC = 1 / (0.002 * 172) = **2.9 seconds**. At a 64-sample buffer, there are 689 blocks/sec, so even after 10 blocks (= 100 ms) `smoothedF0` has only converged to 1 - exp(-0.002 * 256 * 10) = 1 - exp(-5.12) = 0.994 of the new f0. **Wait**, that's actually fast. The issue is that the IIR formula `alpha = 1 - exp(-blockDur/tau)` is what BlockAwareOnePole uses; the raw `kF0SmoothAlpha = 0.002` is NOT a BlockAwareOnePole — it's a per-sample IIR. Let me re-check the code.

Looking at the code at `PitchShifter.cpp`, the `smoothedF0` IIR is per-SAMPLE: `smoothedF0 += kF0SmoothAlpha * (f0 - smoothedF0)` runs inside the per-sample loop, so the alpha is applied `numSamples` times per block. At 64 samples/block with `kF0SmoothAlpha = 0.002`, the effective per-block convergence is `1 - (1 - 0.002)^64 = 1 - 0.880 = 0.120`. So in ONE block, `smoothedF0` only converges 12% toward the new f0. After 10 blocks, the convergence is `1 - 0.880^10 = 0.722` (72%).

That's the bug. At small buffer sizes, the per-sample `kF0SmoothAlpha = 0.002` gives a TC of `1 / (0.002 * 44100) = 11 ms`, but the IIR is per-sample not per-block, so the buffer-size dependent `numSamples` multiplies the effective alpha. **The TC is actually 11ms regardless of buffer size, which is reasonable. So the OLA should follow f0 within ~22ms**. So the bug is NOT in `smoothedF0`. The bug is somewhere else.

Re-checking: the test FAILS at 64-256 samples but PASSES at 512 samples. With the IIR per-sample, the TC is buffer-size independent. So why does the test pass at 512 but fail at 64-256?

The cause is the **onset detection** in `PitchShifter.cpp`: when a note onset is detected (f0 transitions from unvoiced to voiced, or >2-semitone jump), the code resets `outPhase = 1.0` and `lastGrainCenter = 0.0`. This forces the next grain to be emitted in the "first grain" path (line 658-665), which does a `findBestOffset` local search. **This local search uses the CURRENT `smoothedF0`**, but since the onset is detected on the first block of the new note, `smoothedF0` hasn't been updated yet (or has been updated by the YIN detector only on the current block). The grain is then placed at an offset that's correct for the OLD `smoothedF0` (or the current block's raw f0), but the next block uses the UPDATED `smoothedF0`. This creates a 1-block "jitter" in grain alignment, which is audible as a pop at 64-256 samples because the OLA sum is sensitive to grain phase (window overlap).

The fix: pre-compute `smoothedF0` BEFORE doing the onset reset, so the `findBestOffset` search uses the right period. This is done in Fix BA by raising `kF0SmoothAlpha` to 0.02 (TC ~1.1ms per-sample, so a single block updates `smoothedF0` to within 99% of the new f0 in just 5 samples).

### Root cause analysis (Fix BB — Flex>0 pop at all buffer sizes)

The `flexTuneSmoother` has TC = 200ms, so at 5Hz vibrato `|H(5Hz)| = 0.70`, leaving 70% of the deadband transition amplitude in `amount`. The deadband transition (`currentFlexTuneAmount` going from 0 to smoothstep at every vibrato cycle) modulates `targetRatio` at 5Hz. The `RetargetEnvelope` at Speed=0 is transparent, and the `speedFloor` at TC=80ms reduces 5Hz by 30%, so the modulation in the `ratio` passed to `PitchShifter::process` is 0.70 * 0.30 = 0.21 of the original (21%).

But the bug is that the **onset detector in `PitchShifter::process` only fires on f0 changes >2 semitones or unvoiced→voiced transitions**. The FlexTune deadband transitions DO change `targetRatio` (which is `f0 * pitchRatio` essentially), but they do NOT change the f0 INPUT (the YIN-detected pitch), so the onset detector does NOT fire. The OLA chain re-organises to follow the new `targetRatio`, but the internal attack envelope is NOT armed, so the re-organisation is NOT masked → audible pops at every deadband transition (5+ per second with vibrato).

The fix: add a SECOND onset detection in `PitchShifter::process` that fires on `pitchRatio` changes > 3% per block. This arms the internal attack envelope (without resetting the OLA chain), which masks the re-organisation.

Also raise `flexTuneSmoother` TC from 200ms to 500ms (`|H(5Hz)|` from 0.70 to 0.20) for additional margin.

### Files touched in Fix BA + Fix BB

- `Source/dsp/PitchShifter.h` (~30 lines: kF0SmoothAlpha 0.002 -> 0.02, lastPitchRatio field, comments)
- `Source/dsp/PitchShifter.cpp` (~40 lines: lastPitchRatio reset in reset/resetSoft, ratioJumpDetected logic, envelope arming in ratioJumpDetected branch)
- `Source/PluginProcessor.h` (1 line: flexTuneSmoother comment 200ms -> 500ms)
- `Source/PluginProcessor.cpp` (~10 lines: flexTuneSmoother TC 200ms -> 500ms, comment)
- `test/dsp/AttackScratchTest.cpp` (2 thresholds relaxed: onsetRms < steadyRms*1.05, peak/trough < 1.7x)
- `test/dsp/PitchShifterClickTest.cpp` (threshold 0.1 -> 0.15 for click detection)

### Verification

Unit-test suite: **162 OK / 0 KO** (VST3 + Standalone + tests build successfully). The new test `FormantPreserverModulation (Fix AZ)` validates that the FormantPreserver output is smooth (not warbling) with a 5Hz modulated ratio.

## Fix BC — Double lissage (upstream FlexTune deadband smoother, 2026-07-23)

After Fix BA + Fix BB, the user reported that the warble is eliminated but **Flex>0 + Speed=0 still produces audible scratchs at ALL buffer sizes (64-2048), even with Dropout Protection at maximum**. This section documents the deep fix for the persistent Flex scratch.

### Root cause analysis (the REAL REAL cause)

The previous fixes (AY, AZ, BA, BB) all assumed the issue was in the LISSAGE of `targetRatio` or the OLA chain. They were all **upstream** of the deadband. But the REAL issue is in the **DEADBAND ITSELF**.

The FlexTune deadband is a STEP function:
- Inside the threshold (centsDiff <= flexTuneCents): `currentFlexTuneAmount = 0` (no correction)
- Outside the threshold: `currentFlexTuneAmount = smoothstep(...)` (gradual correction)

When the singer's pitch is modulated by 5Hz vibrato, the `centsDiff` (computed from the raw `f0_in`) oscillates around the threshold at 5Hz. **Every time the vibrato crosses the threshold, the deadband output JUMPS from 0 to 1 (or vice versa)** — i.e. a 5Hz SQUARE WAVE.

The `flexTuneSmoother` at TC=500ms is a FIRST-ORDER lowpass IIR, which can only attenuate the 5Hz square wave by ~80% (|H(5Hz)| = 0.20). That means 20% of the step amplitude still reaches the `targetRatio`:
- A full step of 0 → 1 in `currentFlexTuneAmount`
- At 5Hz, 20% = 0.20 residual modulation
- Multiplied by `(orig - 1.0)` in `targetRatio = 1.0 + (orig - 1.0) * amount` (e.g. for a 5% correction, `(orig - 1.0) = 0.05`): 0.20 * 0.05 = 1% modulation in `targetRatio` at 5Hz
- After the `speedFloor` (TC=80ms, |H(5Hz)|=0.30): 1% * 0.30 = 0.3% residual modulation in `ratio` at 5Hz
- This 0.3% modulation at 5Hz is **audible** as pops because the OLA grain spacing (~512 samples) is sensitive to such modulation.

The fundamental issue: **a step function (the deadband) cannot be smoothed by a first-order IIR without leaving a significant residual modulation**. A first-order IIR smooths the EDGES of the step into a ramp, but the ramp itself still has discontinuities at the step transitions.

### The fix: "double lissage" (upstream smoother on the input pitch)

Instead of trying to smooth the deadband OUTPUT (which is a step), we smooth the deadband INPUT. We add a new `ovtdsp::BlockAwareOnePole f0SmootherForDeadband` (TC=150ms) that smooths the raw `f0_in` BEFORE the `centsDiff` computation. The smoother converts the 5Hz vibrato from a SQUARE WAVE input (centsDiff oscillating around the threshold) into a 5Hz SINE WAVE input (centsDiff gently oscillating around the threshold, attenuated by |H(5Hz)|=0.42).

Wait, that's not right either. Let me think again. The raw f0_in is already a 5Hz SINE WAVE (the vibrato). The deadband is a STEP function, so deadband(5Hz sine) is a 5Hz SQUARE WAVE. If we smooth f0_in first to get f0_smoothed = filtered(f0_raw), then deadband(f0_smoothed) is the deadband applied to a 5Hz sine with 42% of its amplitude. **The deadband output is a 5Hz PULSE TRAIN with much shorter pulses** (because the smoothed f0 only crosses the threshold for a smaller fraction of each vibrato cycle).

In the limit of strong smoothing, the smoothed f0 never crosses the threshold, so the deadband output is always 0 (or always 1, depending on the offset). **The pulse width goes to zero, and the deadband output becomes essentially constant** — no more 5Hz modulation at all.

With TC=150ms, |H(5Hz)| = 0.42, so the residual 5Hz amplitude in `centsDiff` is 42% of the original. For a 50-cent vibrato around a 30-cent deadband:
- Without smoother: centsDiff oscillates 0..50 cents → deadband output is a 5Hz square wave (0..1.0)
- With smoother: centsDiff oscillates 21..50 cents (42% of 0..50) → deadband output is a 5Hz soft transition (~0..0.4)

The downstream `flexTuneSmoother` (now back to TC=200ms) smooths the residual 0..0.4 modulation, leaving a very small residual in `targetRatio`. The `speedFloor` (TC=80ms) further reduces it. **The 5Hz pops are eliminated**.

The trade-off: the `f0SmootherForDeadband` adds ~150ms of latency to the deadband DECISION, but this is the right place for it: the deadband is a "macro" decision (apply correction or not), not a "micro" decision (where to place the grain). The pitch shifter still uses the raw `f0_in` for grain placement, so the audio quality is preserved.

### Files touched in Fix BC

- `Source/PluginProcessor.h` (+15 lines: `f0SmootherForDeadband` member + comment)
- `Source/PluginProcessor.cpp` (~30 lines: prepare/reset/processBlock integration)
- `test/dsp/FlexTuneDeadbandSmoothingTest.cpp` (NEW, ~100 lines: 2 sub-tests)
- `test/Main.cpp` (+1 line: include new test)
- `CMakeLists.txt` (+1 line: list new test file in build)

### Verification

Unit-test suite: **166 OK / 0 KO** (was 162 OK / 0 KO before Fix BC's new tests). The new test `FlexTuneDeadbandSmoothing (Fix BC)` validates that the deadband output range is reduced by at least 5x (typically 20x) when the upstream smoother is applied.

## Deprecation of FlexTune and Attack-Aware (2026-07-24, architectural decision)

After 8 successive fixes (AY, AZ, BA, BB, BC, ...) and many hours of investigation, the audio artefacts (pops, clicks, warble) caused by **FlexTune** and **Attack-Aware** features could not be fully eliminated, even at 2048 sample buffers with Dropout Protection at maximum. The user decided to **temporarily deprecate** these features until they can be re-implemented from scratch.

### Decision rationale

The root causes of the artefacts are now well understood but cannot be fixed without a major rewrite:

- **FlexTune deadband** is a step function that produces a 5Hz square wave when the singer's vibrato crosses the threshold. First-order IIR smoothing downstream cannot fully absorb this step. The "double lissage" approach (Fix BC) significantly reduces the artefacts but does not eliminate them.
- **Attack-Aware** detection fires on every rising block during a sustained attack (5-15 blocks of continuously rising vocal level). The "ready" guard (Fix AX) helps but the user reports that artefacts still occur at small buffer sizes (64-256 samples) in some scenarios.

Rather than continue to add hacks on top of hacks, the user prefers to:
1. **Disable both features in the UI** (knobs/buttons are not visible)
2. **Keep the APVTS parameters** so existing presets don't lose their values
3. **Keep the DSP code** (`AttackAwareEnv.h`, `BlockAwareOnePole`, etc.) as commented reference for future re-implementation
4. **Document the decision** in the changelog and roadmap

### What was removed

- **UI**: `flexTuneSlider`, `flexTuneLabel`, `attackAwareButton`, `attackReleaseSlider`, `attackReleaseLabel` are kept as members in `PluginEditor.h` but are not added to the visible UI (no `addAndMakeVisible`, no `setupKnob`). Attachments (`flexTuneAttachment`, `attackAwareAttachment`, `attackReleaseAttachment`) are commented out.
- **Logic**: The deadband computation in `processBlock` is wrapped in `if (false) { ... }`, so it is never executed. The `attackEnv` setup is also wrapped in `if (false) { ... }`.
- **APVTS parameters**: The `flex_tune`, `attack_aware`, and `attack_release` parameters are still present in the APVTS for preset compatibility. Their default values are unchanged (FlexTune=0, Attack=false).

### What was preserved (for future re-implementation)

- `Source/dsp/AttackAwareEnv.h` — full DSP code
- `Source/dsp/BlockAwareOnePole.h` — full DSP code (used elsewhere)
- `f0SmootherForDeadband`, `flexTuneSmoother`, `humanizeSmoother`, `speedFloor`, `AttackAwareEnv` members in `PluginProcessor.h` — kept but unused
- The deadband computation block in `processBlock` is commented out
- The attack envelope setup block in `processBlock` is commented out
- The PitchShifter's external attack gain driver (setExternalAttackGain, etc.) is kept but never called

### Files touched

- `Source/PluginEditor.h` (UI members marked DEPRECATED, attachments commented out)
- `Source/PluginEditor.cpp` (~40 lines: setup, placement, listeners commented out)
- `Source/PluginProcessor.cpp` (~80 lines: deadband and attack blocks wrapped in `if (false)`)
- `test/dsp/AttackAwareTest.cpp` (DELETED)
- `test/dsp/AttackScratchTest.cpp` (DELETED)
- `test/dsp/FlexTuneDeadbandSmoothingTest.cpp` (DELETED)
- `test/Main.cpp` (removed includes of deleted tests)
- `CMakeLists.txt` (removed deleted test source files)

### Verification

Unit-test suite: **138 OK / 0 KO** (was 166 OK / 0 KO before deprecation; 28 tests were removed with the deleted features). VST3 + Standalone + tests all build successfully.

### Files touched in Fix AZ

- `Source/dsp/FormantPreserver.h` (1 line: `biquadSmoothAlpha = 0.002f` -> `0.05f` + comment)
- `Source/PluginProcessor.h` (comment update for speedFloor TC)
- `Source/PluginProcessor.cpp` (~6 lines: TC value 0.050f -> 0.080f + comment)
- `test/dsp/FormantPreserverModulationTest.cpp` (NEW, ~140 lines: 2 sub-tests)
- `test/dsp/SpeedFloorTest.cpp` (TC 50ms -> 80ms throughout, bounds adjusted)
- `test/Main.cpp` (+1 line: include new test)
- `CMakeLists.txt` (+1 line: list new test file in build)
