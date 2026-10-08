#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""EDDA OS: THE ARRIVAL's mix, measured (tests/arrival_render.c writes the songs and, with STEMS, each track alone).

  python3 tests/arrival_check.py build/arrival build/gen/arrival.json [SONG ...]

Per song and section (its first pass): the loudness of the mix and of each track (K-weighted as ITU-R BS.1770: the
sub-bass counts less, the presence more; LU here are dB of that weighting against the Q15 full scale), its peak, and
how far each track sits from its role's place in the mix (ROLE_LU: the log drum and the kit carry the floor, the lead
sits over the keys, the pads under them). numpy and scipy."""
import json
import sys
import wave
from pathlib import Path

import numpy as np
from scipy import signal

# a role's loudness against the mix in the drop (LU): the EDDA balance (kick and log drum forward, the top low)
ROLE_LU = {"log": -3.5, "bass": -3.5, "kit": -3.0, "keys": -8.0, "lead": -7.5, "pad": -11.0, "bell": -10.0}
MIX_LU = -15.0                 # the drop's loudness at MASTER full (the master limiter starts at -5.2 dB, Q15; the
                               # factory power-on mix with its patterns: -12.6, the limiter on all the time)
LEVEL_DB = 0.5                 # a LEVEL step (tools/gen_tables.py LEVEL_Q12: (v - 112) / 2 dB, 112 = 0 dB)


def read(path):
    with wave.open(str(path)) as w:
        x = np.frombuffer(w.readframes(w.getnframes()), "<i2").astype(np.float64).reshape(-1, w.getnchannels())
        return w.getframerate(), x / 32768.0


def kweight(x, sr):
    """BS.1770's K filter (a high shelf of +4 dB from ~1.5 kHz, a high-pass at ~38 Hz), bilinear from the analog
    prototypes so it holds at 44.1 kHz"""
    f0, g, q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    k = np.tan(np.pi * f0 / sr)
    vh = 10 ** (g / 20)
    vb = vh ** 0.4996667741545416
    a0 = 1 + k / q + k * k
    b1 = [(vh + vb * k / q + k * k) / a0, 2 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0]
    a1 = [1, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0]
    f0, q = 38.13547087602444, 0.5003270373238773
    k = np.tan(np.pi * f0 / sr)
    a0 = 1 + k / q + k * k
    b2 = [1, -2, 1]
    a2 = [1, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0]
    return signal.lfilter(b2, a2, signal.lfilter(b1, a1, x, axis=0), axis=0)


def lufs(x, sr):
    """the loudness of a span (no gating: a section of a song is music throughout)"""
    y = kweight(x, sr)
    ms = np.mean(y ** 2, axis=0).sum()
    return -0.691 + 10 * np.log10(ms + 1e-20)


def peak_db(x):
    return 20 * np.log10(np.max(np.abs(x)) + 1e-12)


def main(argv):
    d, meta = Path(argv[0]), json.loads(Path(argv[1]).read_text())
    only = {int(a) for a in argv[2:]}
    import os
    master_db = 20 * np.log10(int(os.environ.get("MASTER_Q12", "4096")) / 4096)   # (the render's MASTER)
    worst, sug = [], {}
    for i, m in enumerate(meta):
        if only and i + 1 not in only:
            continue
        stem = f"{i + 1:02d}_{m['name'].replace(' ', '_')}"
        mix_p = d / f"{stem}.wav"
        if not mix_p.exists():
            continue
        sr, mix = read(mix_p)
        stems = [read(d / f"{stem}_T{k + 1}.wav")[1] if (d / f"{stem}_T{k + 1}.wav").exists() else None for k in range(4)]
        print(f"{i + 1:02d} {m['name']:18s} {m['bpm']} BPM {m['cam']:3s} {m['seconds']:6.1f} s  "
              f"mix {lufs(mix, sr):6.1f} LU  peak {peak_db(mix):5.1f} dB")
        seen = set()
        for row in m["timeline"]:
            if row["section"] in seen:
                continue
            seen.add(row["section"])
            a, b = int(row["start"] * sr), int(row["end"] * sr)
            seg = mix[a:b]
            if len(seg) < sr // 4:
                continue
            ml = lufs(seg, sr)
            line = f"   {row['section']:7s} mix {ml:6.1f} LU pk {peak_db(seg):5.1f}"
            for k in range(4):
                if stems[k] is None:
                    continue
                s = stems[k][a:b]
                if np.max(np.abs(s)) < 1e-4:
                    line += f" | T{k + 1}  ----"
                    continue
                sl = lufs(s, sr) - ml
                role = m["roles"][k]
                off = sl - ROLE_LU[role] if role in ROLE_LU else 0.0
                line += f" | T{k + 1} {role[:4]:4s} {sl:5.1f} ({off:+4.1f})"
                if row["section"] in ("DROP", "HOOK"):
                    worst.append((abs(off), m["name"], row["section"], k + 1, off))
                    if role in ROLE_LU:            # the LEVEL that puts it in its place at the mix target
                        want = MIX_LU + ROLE_LU[role] - (lufs(s, sr) - master_db)
                        sug.setdefault(i, {})[k] = round(m["levels"][k] + want / LEVEL_DB)
            print(line)
    for i, lv in sug.items():
        print(f"{i + 1:02d} {meta[i]['name']}: LEVEL for a drop at {MIX_LU:.0f} LU: " +
              ", ".join(f"T{k + 1} {meta[i]['levels'][k]} -> {v}" for k, v in sorted(lv.items())))
    if worst:
        worst.sort(reverse=True)
        print("furthest from their role (drop):", ", ".join(f"{n} T{k} {o:+.1f}" for _, n, s, k, o in worst[:6]))


if __name__ == "__main__":
    main(sys.argv[1:])
