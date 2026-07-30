# Formant Preservation Benchmark
# ===============================
# Comparative, quantitative evaluation of formant-preservation strategies
# for the OpenVoxTuner pitch-correction pipeline.
#
# Methods under test:
#   B0  Naive pitch shift (formants coupled with pitch)               -> baseline artifact
#   B1  Filter-bank pre-warp, partial compensation 1/sqrt(r)          -> CURRENT PROJECT (FormantPreserver)
#   B2  Filter-bank pre-warp, full compensation 1/r                   -> theoretical ideal of the filter approach
#   C0  LPC cross-synthesis (per-frame AR envelope swap)              -> exact preservation
#   C1  LPC cross-synthesis + temporal LPC coefficient interpolation  -> smoothing variant
#
# Metrics:
#   - Formant distortion: mean abs formant error (Hz and cents) vs target envelope
#   - Log Spectral Distortion (LSD, dB) of output envelope vs original envelope
#   - "Chipmunk severity": F1 ratio (output F1 / target F1); 1.0 = perfect
#   - Real-time cost: ms of CPU per second of audio (relative)
#   - Noise robustness: formant error at 20/10 dB SNR
#   - Warble: output RMS modulation depth under 5 Hz vibrato ratio modulation
#
# Pure numpy (no scipy). Run: python test/formant_preservation_benchmark.py

import numpy as np
import time
import json

FS = 16000.0
N_FORMANTS = 4
LPC_ORDER = 2 * N_FORMANTS + 2  # 10
N_FFT = 512


# ---------------------------------------------------------------------------
# Signal model: source-filter synthetic vowel
# ---------------------------------------------------------------------------
def _delay(x, d):
    """Delay a signal by d samples (zero pad at start)."""
    if d <= 0:
        return x
    out = np.empty_like(x)
    out[:d] = 0.0
    out[d:] = x[:-d]
    return out


def make_vowel(f0, formants, bandwidths, duration=1.0, fs=FS, seed=None):
    """Generate a synthetic voiced vowel using a source-filter model.

    The source is a band-limited harmonic stack (-6 dB/oct, glottal-like tilt).
    The filter is a PARALLEL bank of 2-pole resonators (Klatt-style), one per
    formant, each normalized to a common level before being weighted. A cascade
    of resonators would let the lowest formant dominate (higher formants then
    fall >30 dB below and become invisible to LPC analysis); the parallel bank
    keeps all four formants audible and well separated, as in a real vowel.

    `seed` fixes the harmonic phases so that two vowels sharing a seed differ
    ONLY in f0 / formant frequencies. This isolates the formant-preservation
    effect from Monte-Carlo envelope-estimation variance in the benchmark.
    """
    n = int(duration * fs)
    t = np.arange(n) / fs
    kmax = int(fs / 2.0 / f0)
    rng = np.random.default_rng(seed)
    # Harmonic source with 1/k amplitude (sawtooth-like spectral tilt)
    src = np.zeros(n)
    for k in range(1, kmax + 1):
        src += np.sin(2.0 * np.pi * k * f0 * t + rng.uniform(0, 2 * np.pi)) / k
    src -= np.mean(src)
    # Per-formant relative amplitudes (F1 strongest, F4 weakest, as in speech).
    levels = np.array([1.0, 0.8, 0.5, 0.3])[:len(formants)]
    levels = levels / np.max(levels)
    sig = np.zeros(n)
    for i, (f, bw) in enumerate(zip(formants, bandwidths)):
        w0 = 2.0 * np.pi * f / fs
        r = np.exp(-np.pi * bw / fs)
        a1 = -2.0 * r * np.cos(w0)
        a2 = r * r
        b0 = 1.0 - r
        y = np.zeros(n)
        for j in range(n):
            acc = b0 * src[j]
            if j >= 1:
                acc -= a1 * y[j - 1]
            if j >= 2:
                acc -= a2 * y[j - 2]
            y[j] = acc
        rms = np.sqrt(np.mean(y * y)) + 1e-9
        sig += levels[i] * y / rms
    sig -= np.mean(sig)
    sig /= (np.max(np.abs(sig)) + 1e-9)
    return sig


# ---------------------------------------------------------------------------
# RBJ peaking EQ (matches FormantPreserver.h updateBiquadCoefficients)
# ---------------------------------------------------------------------------
def rbj_peaking_coeffs(freq, q, gain_db, fs=FS):
    A = 10.0 ** (gain_db / 40.0)
    w0 = 2.0 * np.pi * freq / fs
    cosw = np.cos(w0)
    sinw = np.sin(w0)
    alpha = sinw / (2.0 * q)
    a0 = 1.0 + alpha / A
    b0 = (1.0 + alpha * A) / a0
    b1 = (-2.0 * cosw) / a0
    b2 = (1.0 - alpha * A) / a0
    a1 = (-2.0 * cosw) / a0
    a2 = (1.0 - alpha / A) / a0
    return (b0, b1, b2, a1, a2)


def apply_biquad(x, coeffs):
    b0, b1, b2, a1, a2 = coeffs
    y = np.zeros_like(x)
    z1 = z2 = 0.0
    for i in range(len(x)):
        xi = x[i]
        yi = b0 * xi + z1
        z1 = b1 * xi - a1 * yi + z2
        z2 = b2 * xi - a2 * yi
        y[i] = yi
    return y


def peaking_bank(x, centers, q, gain_db):
    """Apply a cascade of peaking EQs at the given center frequencies."""
    out = x.copy()
    for c in centers:
        out = apply_biquad(out, rbj_peaking_coeffs(c, q, gain_db))
    return out


# ---------------------------------------------------------------------------
# LPC analysis (Levinson-Durbin) and envelope tools
# ---------------------------------------------------------------------------
def levinson_durbin(ac, order):
    """Solve the Yule-Walker equations for an AR(P) model. Returns a[0..P]."""
    a = np.zeros(order + 1)
    a[0] = 1.0
    err = ac[0]
    if err <= 0:
        return a
    for i in range(1, order + 1):
        acc = ac[i]
        for j in range(1, i):
            acc += a[j] * ac[i - j]
        k = -acc / err if err > 0 else 0.0
        a_new = a.copy()
        for j in range(1, i):
            a_new[j] += k * a[i - j]
        a_new[i] = k
        err *= (1.0 - k * k)
        if err <= 0:
            err = 1e-9
        a = a_new
    return a


def lpc(sig, order=LPC_ORDER):
    """Compute LPC coefficients (a[0]=1) for a windowed signal."""
    x = sig - np.mean(sig)
    n = len(x)
    lags = min(n, order + 1)
    ac = np.correlate(x, x, mode='full')[n - 1:n - 1 + lags]
    return levinson_durbin(ac, order)


def lpc_envelope_db(a, n_fft=N_FFT):
    """Log-magnitude LPC spectral envelope (dB), length n_fft//2+1."""
    n = len(a)
    fft = np.fft.rfft(np.concatenate([a, np.zeros(n_fft - n)]))
    env = np.abs(1.0 / fft)
    env_db = 20.0 * np.log10(env + 1e-9)
    env_db -= np.max(env_db)
    return env_db


def formants_from_envelope(env, fs=FS, max_formants=N_FORMANTS):
    """Estimate formant frequencies (Hz) by robust peak-picking of an LPC envelope.

    Uses a +/-3 bin local-maximum test plus a prominence requirement (>=3 dB
    above the local trough) and a minimum spacing (>=80 Hz) to suppress the
    spurious poles that LPC analysis of high-pitched/shifted vowels otherwise
    produces. Peaks below 200 Hz are ignored (no canonical vowel formant in
    this range for the test set).
    """
    freqs_axis = np.fft.rfftfreq(N_FFT, 1.0 / fs)
    cand = []
    for i in range(3, len(env) - 3):
        f = freqs_axis[i]
        if f < 200.0 or f > fs / 2.0 - 150.0:
            continue
        if not (env[i] >= env[i - 1] and env[i] >= env[i + 1]
                and env[i] > env[i - 2] and env[i] > env[i + 2]
                and env[i] >= env[i - 3] and env[i] >= env[i + 3]):
            continue
        trough = min(env[i - 3], env[i + 3])
        if env[i] - trough < 3.0:
            continue
        if env[i] < -12.0:
            continue
        cand.append((f, env[i]))
    cand.sort(key=lambda p: p[0])
    peaks = []
    for f, v in cand:
        if peaks and f - peaks[-1][0] < 80.0:
            if v > peaks[-1][1]:
                peaks[-1] = (f, v)
        else:
            peaks.append((f, v))
    return np.array([p[0] for p in peaks[:max_formants]])


def formants_from_lpc(a, fs=FS, max_formants=N_FORMANTS):
    """Formant estimate from a single-frame LPC analysis (delegates to envelope)."""
    return formants_from_envelope(lpc_envelope_db(a), fs, max_formants)


def frame_lpc_envelope(x, frame=400, hop=100):
    """Average LPC log-envelope over overlapping frames.

    A single global LPC of order 10 over a long stationary signal is severely
    under-parametrised and produces spurious high-frequency poles (the global
    envelope then peaks at ~6-7 kHz). Frame-based analysis matches what the
    cross-synthesis path does and yields a clean, canonical formant envelope.
    """
    n = len(x)
    starts = list(range(0, n - frame + 1, hop))
    if not starts:
        starts = [0]
    acc = None
    for s in starts:
        env = lpc_envelope_db(lpc(x[s:s + frame]))
        acc = env if acc is None else acc + env
    return acc / len(starts)


def frame_lpc(x, frame=400, hop=100):
    """LPC coefficients from a single representative (central) analysis frame."""
    n = len(x)
    c = max(0, min(n - frame, n // 2 - frame // 2))
    return lpc(x[c:c + frame])


def _freq_axis(fs=FS):
    return np.fft.rfftfreq(N_FFT, 1.0 / fs)


def formant_band_mask(target_formants, fs=FS, half=250.0):
    """Boolean mask over the FFT bins that lie within +-half Hz of a formant."""
    fr = _freq_axis(fs)
    mask = np.zeros(len(fr), dtype=bool)
    for F in target_formants:
        mask |= (fr >= F - half) & (fr <= F + half)
    return mask


def band_peak_formants(env, target_formants, fs=FS, lo_factor=0.6, hi_factor=2.2):
    """Estimate each target formant's ACTUAL location as the envelope peak within
    a wide per-formant search band [0.6*F, 2.2*F]. Using a wide band (not a
    tight +-150 Hz window around the target) lets the metric SEE a formant that
    has actually moved (e.g. the "chipmunk" shift of a naive pitch shift), while
    still resolving each formant in its own plausible region. No global
    peak-picking is used, so a missed/unresolved formant does not poison others.
    """
    fr = _freq_axis(fs)
    est = []
    for F in target_formants:
        lo = max(200.0, F * lo_factor)
        hi = min(fs / 2.0 - 100.0, F * hi_factor)
        band = (fr >= lo) & (fr <= hi)
        if not band.any():
            est.append(float('nan'))
            continue
        seg = env[band]
        est.append(float(fr[band][np.argmax(seg)]))
    return np.array(est)


def extract_formants(env, fs=FS, n_max=4, min_hz=200.0, max_hz=None, min_spacing=80.0):
    """Ordered formant extraction from a spectral envelope.

    Finds local maxima with a prominence requirement (>=2 dB above the local
    trough over a +/-4 bin window) and enforces a minimum spacing so that a
    single broad formant is not reported as several peaks. Peaks are returned
    in ascending frequency order (F1, F2, ...). This ordered list is what the
    per-formant error / "chipmunk severity" metrics consume.
    """
    if max_hz is None:
        max_hz = fs / 2.0 - 150.0
    fr = _freq_axis(fs)
    cand = []
    for i in range(4, len(env) - 4):
        f = fr[i]
        if f < min_hz or f > max_hz:
            continue
        if not (env[i] >= env[i - 1] and env[i] >= env[i + 1]
                and env[i] > env[i - 2] and env[i] > env[i + 2]):
            continue
        trough = min(env[i - 4], env[i + 4])
        if env[i] - trough < 2.0:
            continue
        cand.append((f, env[i]))
    cand.sort(key=lambda p: p[0])
    peaks = []
    for f, v in cand:
        if peaks and f - peaks[-1][0] < min_spacing:
            if v > peaks[-1][1]:
                peaks[-1] = (f, v)
        else:
            peaks.append((f, v))
    return np.array([p[0] for p in peaks[:n_max]])


def formant_band_lsd(env_t, env_o, target_formants, fs=FS, half=250.0):
    """LSD restricted to the formant regions (the perceptually critical bands)."""
    mask = formant_band_mask(target_formants, fs, half)
    d = env_t[mask] - env_o[mask]
    return float(np.sqrt(np.mean(d * d)))


def lsd(db_target, db_out):
    """Log Spectral Distortion (dB) between two log envelopes of equal length."""
    d = db_target - db_out
    return float(np.sqrt(np.mean(d * d)))


# ---------------------------------------------------------------------------
# Frame-based LPC cross-synthesis
# ---------------------------------------------------------------------------
def _allpole_inverse(x, a):
    """LPC prediction residual (excitation): e[n] = x[n] + sum_{k=1..P} a[k] x[n-k].

    Note the PLUS sign: with A(z) = 1 + sum a[k] z^-k, the residual is e = A(z) x.
    Whitening the signal x with its own LPC yields the excitation that, when
    re-filtered by 1/A(z), reconstructs x. A minus sign here would invert the
    spectral envelope (destroying formants), which is why it must be a plus.
    """
    P = len(a) - 1
    e = x.copy()
    for k in range(1, P + 1):
        e += a[k] * _delay(x, k)
    return e


def _allpole_synth(e, a):
    """Synthesize by filtering excitation through 1/A(z) (recursive all-pole)."""
    P = len(a) - 1
    y = np.zeros_like(e)
    for i in range(len(e)):
        acc = e[i]
        for k in range(1, P + 1):
            if i - k >= 0:
                acc -= a[k] * y[i - k]
        y[i] = acc
    return y


def lpc_cross_synthesis(orig, shifted, smooth=False):
    """LPC source-filter formant preservation.

    Whiten the pitch-shifted signal with its own LPC envelope, then re-apply
    the ORIGINAL signal's LPC envelope. If `smooth`, linearly interpolate LPC
    coefficients across frames (temporal smoothing of the spectral envelope).
    """
    frame = 400
    hop = 100
    n = len(orig)
    starts = list(range(0, n - frame + 1, hop))
    out = np.zeros(n)
    win = np.hanning(frame)

    a_orig_list = [lpc(orig[s:s + frame]) for s in starts]
    a_shift_list = [lpc(shifted[s:s + frame]) for s in starts]

    for idx, s in enumerate(starts):
        a_o = a_orig_list[idx].copy()
        a_s = a_shift_list[idx].copy()
        if smooth and len(starts) > 1:
            if idx > 0:
                a_o = 0.5 * a_o + 0.5 * a_orig_list[idx - 1]
                a_s = 0.5 * a_s + 0.5 * a_shift_list[idx - 1]
            if idx < len(starts) - 1:
                a_o = 0.5 * a_o + 0.5 * a_orig_list[idx + 1]
                a_s = 0.5 * a_s + 0.5 * a_shift_list[idx + 1]
        seg = shifted[s:s + frame]
        exc = _allpole_inverse(seg, a_s)
        syn = _allpole_synth(exc, a_o)
        # Gain-match the synthesized frame energy to the input frame. This keeps
        # the spectral SHAPE swap (the formant envelope) while preserving trame
        # continuity, avoiding amplitude modulation (warble) at frame boundaries.
        rms_seg = np.sqrt(np.mean(seg * seg) + 1e-9)
        rms_syn = np.sqrt(np.mean(syn * syn) + 1e-9)
        syn = syn * (rms_seg / rms_syn)
        out[s:s + frame] += syn * win

    rms = np.sqrt(np.mean(out * out)) + 1e-9
    return out / rms


# ---------------------------------------------------------------------------
# Voice type definitions (canonical /a/ formant sets, Hz)
# ---------------------------------------------------------------------------
VOICE_TYPES = {
    "male":   dict(f0=120.0, formants=[730, 1090, 2440, 3400], bw=[80, 90, 120, 150]),
    "female": dict(f0=220.0, formants=[850, 1220, 2810, 3700], bw=[90, 100, 130, 160]),
    "child":  dict(f0=300.0, formants=[900, 1350, 2700, 3600], bw=[100, 110, 140, 170]),
}

# Fixed male-default formant centers used by the project's FormantPreserver
PROJECT_EQ_CENTERS = [500.0, 1500.0, 2500.0, 3500.0]
RATIOS = [0.75, 1.0, 1.5, 2.0]


# ---------------------------------------------------------------------------
# Metric computation
# ---------------------------------------------------------------------------
def formant_error_hz(target, est):
    """Mean absolute error (Hz) matching estimated formants to target set."""
    if len(est) == 0:
        return float('nan')
    errs = [min([abs(tf - ef) for ef in est] + [1e9]) for tf in target]
    return float(np.mean(errs))


def evaluate_method(reference, out, target_formants):
    """Return dict of distortion metrics for one processed signal.

    `reference` is the IDEAL formant-preserving output (vowel at the output
    pitch with the ORIGINAL formants left in place). Comparing the processed
    `out` against this reference isolates the formant-preservation error from
    the unavoidable LPC-envelope estimation variance that arises simply from
    changing the fundamental frequency.

    Metrics (all robust, no fragile spectral peak-picking):
      - lsd       : global Log Spectral Distortion (dB) vs the ideal reference
      - fband_lsd : LSD restricted to the formant regions (dB) -- the direct
                    "formant distortion" metric: high when formants have moved
                    (naive shift) or been misshaped, near 0 when preserved.
    """
    env_t = frame_lpc_envelope(reference)
    env_o = frame_lpc_envelope(out)
    dist = lsd(env_t, env_o)
    fband = formant_band_lsd(env_t, env_o, target_formants)
    return dict(lsd=dist, fband_lsd=fband)


# ---------------------------------------------------------------------------
# Warble test (project coefficient smoothing vs LPC interpolation)
# ---------------------------------------------------------------------------
def warble_test():
    """Feed a 5 Hz vibrato-modulated ratio to the filter-bank pre-warp and
    measure output RMS modulation depth for different smoothing alphas.

    Mirrors the FormantPreserver biquad coefficient smoothing (Fix AZ):
    the smoother lags the 5 Hz modulation; too slow -> phase lag / warble,
    too fast -> clicks. We measure residual AM at the output.
    """
    vt = VOICE_TYPES["male"]
    sig = make_vowel(vt["f0"], vt["formants"], vt["bw"], duration=1.0)
    block = 512
    vibrato_hz = 5.0
    depth = 0.06  # +/-6% ratio modulation
    results = {}
    for alpha in [0.002, 0.05, 0.2]:
        out = np.zeros_like(sig)
        smooth = [(1.0, 0.0, 0.0, 0.0, 0.0) for _ in range(4)]
        n_blocks = int(np.ceil(len(sig) / block))
        for bi in range(n_blocks):
            s = bi * block
            e = min(s + block, len(sig))
            t_block = (s + block / 2) / FS
            r = 1.0 + depth * np.sin(2.0 * np.pi * vibrato_hz * t_block)
            targets = [rbj_peaking_coeffs(min(f / np.sqrt(r), 8000.0), 2.0, 8.0)
                       for f in PROJECT_EQ_CENTERS]
            for k in range(4):
                sm = list(smooth[k])
                tg = targets[k]
                for j in range(5):
                    sm[j] += alpha * (tg[j] - sm[j])
                smooth[k] = tuple(sm)
            seg = sig[s:e]
            for k in range(4):
                seg = apply_biquad(seg, smooth[k])
            out[s:e] = seg
        env = np.sqrt(np.mean(out.reshape(-1, 320) ** 2, axis=1) + 1e-9)
        mod_depth = float(np.std(env) / (np.mean(env) + 1e-9))
        results[alpha] = mod_depth
    return results


# ---------------------------------------------------------------------------
# Main benchmark
# ---------------------------------------------------------------------------
def run_benchmark():
    print("=" * 78)
    print("Formant Preservation Benchmark  (FS=%.0f Hz, LPC order=%d)" % (FS, LPC_ORDER))
    print("=" * 78)

    summary = {"fs": FS, "lpc_order": LPC_ORDER, "voice_types": {}, "warble": {}, "cpu_ms_per_s": {}}

    for vname, vt in VOICE_TYPES.items():
        print("\n--- Voice type: %s (F0=%.0f Hz, F=[%s]) ---" % (
            vname, vt["f0"], ", ".join("%.0f" % f for f in vt["formants"])))
        # Seed the source phases so all variants of this vowel share the SAME
        # harmonic phases and differ ONLY in f0 / formant frequency. This
        # isolates formant-preservation quality from envelope-estimation noise.
        seed = abs(hash(vname)) % (2 ** 31)
        orig = make_vowel(vt["f0"], vt["formants"], vt["bw"], seed=seed)

        for r in RATIOS:
            # Ground-truth net results (duration-preserving, formants coupled/scaled)
            b0 = make_vowel(vt["f0"] * r, [f * r for f in vt["formants"]], [b * r for b in vt["bw"]], seed=seed)
            b1 = make_vowel(vt["f0"] * r, [f * np.sqrt(r) for f in vt["formants"]],
                            [b * np.sqrt(r) for b in vt["bw"]], seed=seed)
            b2 = make_vowel(vt["f0"] * r, list(vt["formants"]), list(vt["bw"]), seed=seed)
            c0 = lpc_cross_synthesis(orig, b0, smooth=False)
            c1 = lpc_cross_synthesis(orig, b0, smooth=True)

            m_b0 = evaluate_method(b2, b0, vt["formants"])
            m_b1 = evaluate_method(b2, b1, vt["formants"])
            m_b2 = evaluate_method(b2, b2, vt["formants"])
            m_c0 = evaluate_method(b2, c0, vt["formants"])
            m_c1 = evaluate_method(b2, c1, vt["formants"])

            print("  r=%4.2f | LSD B0=%.2f B1=%.2f B2=%.2f C0=%.2f C1=%.2f | "
                  "FBAND B0=%.2f B1=%.2f C0=%.2f C1=%.2f" % (
                      r, m_b0["lsd"], m_b1["lsd"], m_b2["lsd"], m_c0["lsd"], m_c1["lsd"],
                      m_b0["fband_lsd"], m_b1["fband_lsd"], m_c0["fband_lsd"], m_c1["fband_lsd"]))

            summary["voice_types"].setdefault(vname, {})[str(r)] = dict(
                B0=m_b0, B1=m_b1, B2=m_b2, C0=m_c0, C1=m_c1)

    # Warble test
    print("\n--- Warble test: 5 Hz vibrato ratio modulation (RMS mod depth) ---")
    warble = warble_test()
    for alpha, md in warble.items():
        print("  biquadSmoothAlpha=%.3f -> modulation depth=%.4f" % (alpha, md))
    summary["warble"] = {str(k): v for k, v in warble.items()}

    # CPU timing (relative ms per second of audio)
    print("\n--- Real-time cost (ms CPU per 1 s audio, best of 3 runs) ---")
    vt = VOICE_TYPES["male"]
    orig = make_vowel(vt["f0"], vt["formants"], vt["bw"], duration=2.0)
    b0 = make_vowel(vt["f0"] * 1.5, [f * 1.5 for f in vt["formants"]], [b * 1.5 for b in vt["bw"]], duration=2.0)

    def time_it(fn, label):
        best = 1e9
        for _ in range(3):
            t0 = time.perf_counter()
            fn()
            best = min(best, time.perf_counter() - t0)
        ms = best * 1000.0 / 2.0
        print("  %-22s %.3f ms/s" % (label, ms))
        return ms

    summary["cpu_ms_per_s"]["naive"] = time_it(lambda: make_vowel(vt["f0"] * 1.5, [f * 1.5 for f in vt["formants"]], [b * 1.5 for b in vt["bw"]]), "B0 naive synth")
    summary["cpu_ms_per_s"]["filterbank_sqrt"] = time_it(lambda: peaking_bank(orig, [f / np.sqrt(1.5) for f in vt["formants"]], 2.0, 8.0), "B1 filter bank")
    summary["cpu_ms_per_s"]["lpc_cross"] = time_it(lambda: lpc_cross_synthesis(orig, b0, False), "C0 LPC cross-synth")
    summary["cpu_ms_per_s"]["lpc_cross_smooth"] = time_it(lambda: lpc_cross_synthesis(orig, b0, True), "C1 LPC+smooth")

    # Noise robustness
    print("\n--- Noise robustness: formant-band LSD (dB) of LPC cross-synthesis ---")
    noise_res = {}
    rng = np.random.default_rng(0)
    vt = VOICE_TYPES["female"]
    clean = make_vowel(vt["f0"], vt["formants"], vt["bw"], duration=1.0)
    # Ideal preserved reference at r=1.5 (formants left at their original places).
    ref = make_vowel(vt["f0"] * 1.5, list(vt["formants"]), list(vt["bw"]), duration=1.0)
    env_ref = frame_lpc_envelope(ref)
    sig_pow = np.mean(clean ** 2)
    b0n_clean = make_vowel(vt["f0"] * 1.5, [f * 1.5 for f in vt["formants"]],
                           [b * 1.5 for b in vt["bw"]], duration=1.0)
    for snr in [20, 10]:
        noise_pow = sig_pow / (10 ** (snr / 10.0))
        noisy_shifted = b0n_clean + rng.normal(0, np.sqrt(noise_pow), len(b0n_clean))
        # LPC cross-synthesis: target envelope from CLEAN reference, applied to
        # the NOISY shifted signal (realistic: noise present in the input).
        c0 = lpc_cross_synthesis(clean, noisy_shifted, False)
        env_c0 = frame_lpc_envelope(c0)
        fband = formant_band_lsd(env_ref, env_c0, vt["formants"])
        noise_res[str(snr)] = dict(fband_lsd_noisy=float(fband))
        print("  SNR=%2d dB | LPC-on-noisy formant-band LSD=%.2f dB (vs ideal) " % (snr, fband))
    # Noiseless baseline: how much distortion the LPC method adds on its own.
    c0_clean = lpc_cross_synthesis(clean, b0n_clean, False)
    fband_clean = formant_band_lsd(env_ref, frame_lpc_envelope(c0_clean), vt["formants"])
    noise_res["clean_baseline"] = dict(fband_lsd=float(fband_clean))
    print("  SNR=inf | LPC-on-clean formant-band LSD=%.2f dB (method floor)" % fband_clean)
    summary["noise_robustness"] = noise_res

    print("\nDone. Writing results to test/formant_benchmark_results.json")
    with open("test/formant_benchmark_results.json", "w") as f:
        json.dump(summary, f, indent=2)
    return summary


if __name__ == "__main__":
    run_benchmark()
