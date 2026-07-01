# Changelog 2026-07-01

## Fixed: Harmony "Use Voice" shifted voices volume extremely low

> Requested by Jerome: when "Use Voice" is enabled in the Harmony section, pitch-shifted harmony voices were approximately 13.5 dB quieter than synthesized voices (Use Voice disabled). The fix compensates for real audio input level, making shifted voices consistent with synth voices while preserving the dedicated Volume knob control.

---

### Root Cause

The per-voice gain for shifted (pitch-shifted) voices used `perVoiceLevel = 1.05 / sqrt(N)`, which was designed for synthesized waveforms at full digital amplitude (~1.0). Real vocal input typically peaks around 0.2, resulting in an effective output of ~0.0525 per shifted voice vs ~0.25 per synthesized voice — a ~13.5 dB discrepancy.

### Files modified

| File | Changes |
|------|---------|
| `Source/PluginProcessor.cpp` | Increased `perVoiceLevel` base constant from `1.05f` to `4.0f` in the Use Voice shifted voice loop. Added explanatory comment documenting the rationale and input-level compensation. |

### Impact

- Shifted voices now produce output comparable to synthesized voices at typical vocal input levels
- The Volume (harmony gain) knob continues to work identically — it multiplies the final harmony buffer independent of this fix
- Pitch shifting, formant preservation, blend, panning, and all other harmony features are unaffected
- No clicks, no pops — only the gain constant changed, no signal path modifications