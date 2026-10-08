#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""EDDA OS: a synthetic 16-pad drum kit for the host tests (tests/edda_test.c), as 16 WAV files and the kit slot
built from them (tools/fm1_sample_upload.py kitbuild -> OUT.hdr / OUT.bin).

  tests/edda_kit.py OUT_PREFIX

The sounds are made here (no library needed): a swept kick, a snare of tone and noise, a clap of three bursts,
two hats, toms, a rim click, a bell, congas, a 3 s crash and a 2.5 s ride. Together they are longer than a
slot holds at 22050 Hz, so the builder must drop the two cymbals to 11025 Hz (the test checks it does).
The crash is pad 13 (note 49), the ride pad 15 (note 51)."""
import math
import random
import struct
import subprocess
import sys
import wave
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "tools"))
import sampleio as sio  # noqa: E402

SR = 44100
LENGTHS = [0.35, 0.22, 0.2, 0.06, 0.4, 0.3, 0.04, 0.5, 0.4, 0.25, 0.3, 0.2, 3.0, 0.15, 2.5, 0.8]


def env(n, i, tau):
    return math.exp(-i / (tau * SR))


def sound(k, rnd):
    """pad k -> float samples at SR"""
    n = int(LENGTHS[k] * SR)
    out = []
    ph = 0.0
    for i in range(n):
        t = i / SR
        if k == 0:                                      # kick: 150 -> 45 Hz sweep
            f = 45 + 105 * math.exp(-t * 18)
            ph += 2 * math.pi * f / SR
            v = math.sin(ph) * env(n, i, 0.12)
        elif k == 1:                                    # snare: 180 Hz + noise
            v = 0.5 * math.sin(2 * math.pi * 180 * t) * env(n, i, 0.05) + 0.6 * rnd.uniform(-1, 1) * env(n, i, 0.07)
        elif k == 2:                                    # clap: three bursts
            v = rnd.uniform(-1, 1) * (env(n, i, 0.012) + 0.8 * env(n, max(0, i - 450), 0.012) * (i > 450) +
                                      0.6 * env(n, max(0, i - 900), 0.05) * (i > 900)) * 0.6
        elif k == 3:                                    # hat closed: bright noise
            v = (rnd.uniform(-1, 1) - (out[-1] if out else 0) * 0.5) * env(n, i, 0.015)
        elif k == 4:                                    # hat open
            v = (rnd.uniform(-1, 1) - (out[-1] if out else 0) * 0.5) * env(n, i, 0.12)
        elif k in (5, 8, 9, 10):                        # toms: sweeps at 110 / 80 / 160 / 130 Hz
            f0 = {5: 110, 8: 80, 9: 160, 10: 130}[k]
            f = f0 * (1 + 0.6 * math.exp(-t * 25))
            ph += 2 * math.pi * f / SR
            v = math.sin(ph) * env(n, i, 0.09)
        elif k == 6:                                    # rim: a click
            v = math.sin(2 * math.pi * 1000 * t) * env(n, i, 0.006) + 0.3 * rnd.uniform(-1, 1) * env(n, i, 0.004)
        elif k == 7:                                    # bell: two inharmonic partials
            v = (math.sin(2 * math.pi * 820 * t) + 0.6 * math.sin(2 * math.pi * 1217 * t)) * 0.5 * env(n, i, 0.15)
        elif k in (11, 13):                             # congas: damped tones
            f0 = 200 if k == 11 else 300
            v = math.sin(2 * math.pi * f0 * t) * env(n, i, 0.06)
        elif k == 12:                                   # crash: long noise
            v = (rnd.uniform(-1, 1) - (out[-1] if out else 0) * 0.3) * env(n, i, 0.9)
        elif k == 14:                                   # ride: noise + a ping
            v = 0.5 * (rnd.uniform(-1, 1) - (out[-1] if out else 0) * 0.3) * env(n, i, 0.8) + \
                0.4 * math.sin(2 * math.pi * 3000 * t) * env(n, i, 0.4)
        else:                                           # ride bell
            v = (math.sin(2 * math.pi * 1800 * t) + 0.5 * math.sin(2 * math.pi * 2700 * t)) * 0.5 * env(n, i, 0.25)
        out.append(max(-1.0, min(1.0, v)))
    return out


def write_wav(path, x):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, v)) * 30000)) for v in x))


def main(prefix):
    out = Path(prefix)
    out.parent.mkdir(parents=True, exist_ok=True)
    rnd = random.Random(0x45444441)
    files = []
    for k in range(16):
        p = out.parent / f"{out.name}_pad{k + 1:02d}.wav"
        write_wav(p, sound(k, rnd))
        files.append(str(p))
    subprocess.run([sys.executable, str(HERE.parent / "tools" / "fm1_sample_upload.py"), "kitbuild", "EDDAKIT",
                    str(out), *files], check=True)
    hdr = out.with_suffix(".hdr").read_bytes()
    nz = hdr[6]
    rates = [struct.unpack_from("<I", hdr, 32 + z * 28 + 16)[0] for z in range(nz)]
    print(f"kit: {nz} pads, rates {sorted(set(rates))}, {out.with_suffix('.bin').stat().st_size} B")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
