# analyze_ab_pitch.py
# OpenVoxTuner - pitch verification for the Hard Tune A/B renders
# Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
#
# Measures the f0 track (YIN, FFT difference function) of every WAV in a render
# directory and prints, per note window, the median pitch in cents and the
# vibrato depth (standard deviation of cents). Used to prove that the A/B
# material is actually out of tune and that the correction moves the takes.
#
# Usage: python tools/analyze_ab_pitch.py build/ab_hard_tune
# Requires numpy (the bundled DSH Python has it).
#
import sys, os, math
import numpy as np

def read_wav(path):
    with open(path, 'rb') as f:
        data = f.read()
    assert data[:4] == b'RIFF'
    pos, fmt, data_off, data_len = 12, None, None, None
    while pos + 8 <= len(data):
        cid = data[pos:pos+4]; sz = int.from_bytes(data[pos+4:pos+8], 'little')
        if cid == b'fmt ':
            ch = int.from_bytes(data[pos+10:pos+12], 'little')
            sr = int.from_bytes(data[pos+12:pos+16], 'little')
            bits = int.from_bytes(data[pos+22:pos+24], 'little')
            fmt = (ch, sr, bits)
        elif cid == b'data':
            data_off, data_len = pos+8, sz; break
        pos += 8 + sz + (sz & 1)
    ch, sr, bits = fmt
    raw = data[data_off:data_off+data_len]
    if bits == 24:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        v = np.where(v & 0x800000, v - 0x1000000, v).astype(np.float64) / 8388608.0
    elif bits == 16:
        v = np.frombuffer(raw, '<i2').astype(np.float64) / 32768.0
    else:
        raise SystemExit("bits %d" % bits)
    if ch > 1:
        v = v.reshape(-1, ch).mean(axis=1)
    return v, sr

def yin_frame(x, sr, W=2048, fmin=60.0, fmax=1000.0):
    """YIN pitch for one (already windowed-free) frame using FFT difference."""
    x = x - np.mean(x)
    E = float(np.dot(x, x))
    if E < 1e-6:
        return 0.0
    W2 = 2 * W
    spec = np.fft.rfft(x, W2)
    r = np.fft.irfft(np.abs(spec) ** 2, W2)[:W]        # r[tau] = sum_{j} x[j]x[j+tau]
    cs = np.concatenate(([0.0], np.cumsum(x * x)))     # cs[k] = sum_{j<k} x[j]^2
    taus = np.arange(1, W)
    d = np.empty(W)
    d[0] = 0.0
    # d(tau) = sum_{j=0}^{W-1-tau} (x[j]-x[j+tau])^2
    head = cs[W - taus]          # sum over j < W-tau of x[j]^2
    tail = E - cs[taus]          # sum over j >= tau of x[j]^2
    d[1:] = head + tail - 2.0 * r[1:W]
    d = np.maximum(d, 0.0)
    d[0] = 1e-12
    # cumulative mean normalised difference
    cum = np.cumsum(d[1:])
    dp = np.ones(W)
    dp[1:] = d[1:] * taus / np.maximum(cum, 1e-12)
    tau_min = max(2, int(sr / fmax))
    tau_max = min(W - 2, int(sr / fmin))
    for tau in range(tau_min, tau_max):
        if dp[tau] < 0.15:
            while tau + 1 < tau_max and dp[tau+1] < dp[tau]:
                tau += 1
            y0, y1, y2 = dp[tau-1], dp[tau], dp[tau+1]
            den = 2.0 * (2.0*y1 - y0 - y2)
            better = tau + ((y2 - y0) / den if abs(den) > 1e-12 else 0.0)
            return sr / better
    return 0.0

def f0_track(x, sr, frame=2048, hop=256):
    ts, fs = [], []
    for start in range(0, len(x)-frame, hop):
        f = yin_frame(x[start:start+frame], sr)
        ts.append((start + frame/2)/sr)
        fs.append(f)
    return np.array(ts), np.array(fs)

def main():
    folder = sys.argv[1]
    if 'take' in folder:
        windows = [(0.30, 0.55), (0.70, 0.95), (1.10, 1.35), (1.50, 1.80)]
    else:
        # synthetic phrase: 5 notes, 1.15 s each + 0.10 gap, first at 0.2
        windows = [(0.45 + k*1.25, 1.20 + k*1.25) for k in range(5)]
    print("folder:", folder)
    print("%-11s %7s | " % ("file", "rms") + "  ".join("note%d: med/vib" % (i+1) for i in range(len(windows))))
    for name in sorted(os.listdir(folder)):
        if not name.endswith('.wav'):
            continue
        x, sr = read_wav(os.path.join(folder, name))
        t, f = f0_track(x, sr)
        cents = np.where(f > 0, 1200.0*np.log2(np.maximum(f, 1e-9)/440.0), np.nan)
        rms = 20*math.log10(max(float(np.sqrt(np.mean(x**2))), 1e-12))
        cells = []
        for (a, b) in windows:
            m = (t >= a) & (t <= b) & np.isfinite(cents)
            if np.sum(m) < 4:
                cells.append("   n/a   "); continue
            c = cents[m]
            med = np.median(c); vib = np.std(c - med)
            cells.append("%+7.1f/%4.1f" % (med, vib))
        print("%-11s %7.1f | %s" % (name, rms, "  ".join(cells)))

main()
