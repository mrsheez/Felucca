#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""EDDA OS: the sounds of THE ARRIVAL bank, made offline (numpy, scipy) into assets/samples-edda/.

  python3 tools/edda_sounds.py [--fetch]

Two kinds of source, every one written as a mono 16-bit WAV at the rate it is stored at in flash (the build,
tools/gen_samples.py, encodes them as they are: no resampling there):

  designed   synthesised here, sample by sample: the amapiano kick, the log drum (two zones), the udu's
             whoop, a glass bottle. Deterministic: the same files on every run.
  recorded   real instruments from Versilian Studios' VCSL (CC0 1.0, https://github.com/sgossner/VCSL):
             shakers, cabasa, claps, the cross-stick, a snare, congas, agogo (the ogene's two bells), the slit
             drum (ekwe), claves. --fetch downloads the takes named in RECORDED into build/vcsl/ (raw.githubusercontent,
             the first 400 KB of each); each is then cut to its hit, high-passed, faded, normalised, resampled
             (polyphase, Kaiser) to its rate.

assets/samples-edda/ATTRIBUTION.txt names every recorded source. The kit's pads and the log drum's zones are
listed in tools/gen_samples.py (EDDA_KIT, EDDA_LOG)."""
import math
import sys
import urllib.parse
import urllib.request
import wave
from pathlib import Path

import numpy as np
from scipy import signal

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "samples-edda"
RAW = ROOT / "build" / "vcsl"
VCSL = "https://raw.githubusercontent.com/sgossner/VCSL/master/"
RNG = np.random.default_rng(0x45444441)          # "EDDA": the designed sounds' noise, the same every run

# name: (VCSL path, rate, keep (s), fade (s), high-pass (Hz), semitones (a retune), pre-onset (ms)); ONSET: the
# takes whose hit is not their first sound (a shaker's beads fly before they land): cut where the envelope first
# reaches this fraction of its peak, so the stroke lands on the step
ONSET = {"shaker": 0.30}
RECORDED = {
    "shaker":    ("Idiophones/Struck Idiophones/Shaker, Large/LShaker_Hit_rr1_Mid.wav", 44100, 0.095, 0.055, 350, 0, 1.0),
    "cabasa":    ("Idiophones/Struck Idiophones/Cabasa/Cabasa1_Hit_rr1_Mid.wav", 44100, 0.140, 0.070, 250, 0, 0.2),
    "clap":      ("Idiophones/Struck Idiophones/Claps/Clap_rr1.wav", 32000, 0.200, 0.100, 120, 0, 0.3),
    "xstick":    ("Membranophones/Struck Membranophones/Snare Drum, Modern 2/Snare3M_Xstick_v2_rr1_Mid.wav", 32000, 0.150, 0.080, 80, 0, 0.2),
    "snare":     ("Membranophones/Struck Membranophones/Snare Drum, Modern 1/Snare2_HitSN_v6_rr1_Mid.wav", 32000, 0.240, 0.120, 60, 0, 0.2),
    "conga":     ("Membranophones/Struck Membranophones/Conga/Conga_HitN_v3_rr1_Sum.wav", 22050, 0.250, 0.120, 50, 0, 0.3),
    "agogo_hi":  ("Idiophones/Struck Idiophones/Agogo Bells/Agogo_High_v3_rr1_Mid.wav", 32000, 0.200, 0.110, 200, -1, 0.2),
    "agogo_lo":  ("Idiophones/Struck Idiophones/Agogo Bells/Agogo_Low_v2_rr1_Mid.wav", 32000, 0.210, 0.120, 200, 0, 0.2),
    "ekwe":      ("Idiophones/Struck Idiophones/Slit Drum/LogDrumHi_MedM_v3_rr1_Sum.wav", 22050, 0.260, 0.130, 50, 0, 0.3),
    "claves":    ("Idiophones/Struck Idiophones/Claves/Claves1_Hit_v2_rr1_Mid.wav", 32000, 0.090, 0.050, 300, 0, 0.2),
}
CREDIT = "sgossner/VCSL (Versilian Studios Community Sample Library, CC0 1.0)"


def write_wav(path, x, sr):
    x = np.clip(np.round(np.asarray(x) * 32767.0), -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(x.tobytes())


def read_wav(path):
    """any PCM / float WAV -> (rate, mono float64)"""
    d = Path(path).read_bytes()
    i, fmt, data = 12, None, None
    while i + 8 <= len(d):
        cid, n = d[i:i + 4], int.from_bytes(d[i + 4:i + 8], "little")
        if cid == b"fmt ":
            fmt = d[i + 8:i + 8 + n]
        elif cid == b"data":
            data = d[i + 8:i + 8 + n]
        i += 8 + n + (n & 1)
    tag, nch, sr = int.from_bytes(fmt[0:2], "little"), int.from_bytes(fmt[2:4], "little"), int.from_bytes(fmt[4:8], "little")
    bits = int.from_bytes(fmt[14:16], "little")
    if tag == 0xFFFE:
        tag = int.from_bytes(fmt[24:26], "little")
    nfr = len(data) // (bits // 8 * nch)
    raw = data[:nfr * bits // 8 * nch]
    if tag == 3:
        x = np.frombuffer(raw, "<f4").astype(np.float64)
    elif bits == 16:
        x = np.frombuffer(raw, "<i2") / 32768.0
    elif bits == 24:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | b[:, 1] << 8 | b[:, 2] << 16
        x = np.where(v >= 1 << 23, v - (1 << 24), v) / 8388608.0
    else:
        x = np.frombuffer(raw, "<i4") / 2147483648.0
    return sr, x.reshape(-1, nch).mean(axis=1)


def norm(x, peak_db=-0.3):
    return x / np.max(np.abs(x)) * 10 ** (peak_db / 20)


def cos_fade(x, n):
    """the last n samples out on a raised cosine"""
    n = min(n, len(x))
    if n > 0:
        x[-n:] *= 0.5 + 0.5 * np.cos(np.pi * np.arange(1, n + 1) / n)
    return x


def hp(x, sr, f, order=2):
    return signal.sosfilt(signal.butter(order, f, "highpass", fs=sr, output="sos"), x) if f else x


def lp(x, sr, f, order=2):
    return signal.sosfilt(signal.butter(order, f, "lowpass", fs=sr, output="sos"), x)


def bp(x, sr, lo, hi, order=2):
    return signal.sosfilt(signal.butter(order, [lo, hi], "bandpass", fs=sr, output="sos"), x)


def sweep_phase(f, sr):
    """the phase of an oscillator whose frequency is f[n] (Hz), from 0"""
    return 2 * np.pi * np.cumsum(f) / sr


def sat(x, drive):
    """a soft clip that keeps the level: tanh(drive x) / tanh(drive)"""
    return np.tanh(drive * x) / np.tanh(drive)


# ------------------------------------------------------------------ designed --- #
def design_kick(sr=22050):
    """the amapiano / 3-step kick: a deep round body (a sine falling 170 -> 50 Hz), a short knock and a click, hard
    and forward (saturated: harmonics that read on small speakers), tight (gone in a third of a second: the log
    drum owns the low end between the kicks)"""
    dur = 0.30
    n = int(dur * sr)
    t = np.arange(n) / sr
    f = 50.0 + 120.0 * np.exp(-t / 0.020) + 18.0 * np.exp(-t / 0.002)
    body = np.sin(sweep_phase(f, sr))
    amp = np.where(t < 0.0006, t / 0.0006, 1.0) * (0.62 * np.exp(-t / 0.055) + 0.38 * np.exp(-t / 0.15))
    amp *= np.clip((dur - t) / 0.09, 0, 1) ** 2             # (the tail closed off smoothly)
    knock = bp(RNG.standard_normal(n), sr, 1400, 4800) * np.exp(-t / 0.0025) * 0.30
    click = np.sin(2 * np.pi * 2900 * t) * np.exp(-t / 0.0009) * 0.22
    x = body * amp + knock + click
    x = sat(x * 1.0, 3.4)                                       # (dense: a low crest, hard in the mix, the limiter idle)
    x = hp(x, sr, 28, 2)
    return norm(x, -0.5)


def design_logdrum(root_hz, sr=16000, dur=0.60):
    """the amapiano log drum at root_hz: a woody, tuned sub. A sine body that drops a fifth into its pitch in the first
    10 ms, its 2nd and 3rd harmonics decaying faster (the wood fading into the round sub), an inharmonic knock mode
    (2.73 f) and a short band-passed thump of air, saturated (the forward, heavy log drum of the records), the tail
    closed off. A sample: the SAMPLE engine plays it anywhere within a few semitones of root_hz"""
    n = int(dur * sr)
    t = np.arange(n) / sr
    f = root_hz * (1.0 + 0.50 * np.exp(-t / 0.0085))        # the strike: a fifth above, falling into the note
    ph = sweep_phase(f, sr)
    env = np.where(t < 0.0012, t / 0.0012, 1.0)
    a1 = env * (0.45 * np.exp(-t / 0.045) + 0.55 * np.exp(-t / 0.20))
    a2 = env * (0.5 * np.exp(-t / 0.03) + 0.5 * np.exp(-t / 0.11))
    a3 = env * np.exp(-t / 0.045)
    a4 = env * np.exp(-t / 0.025)
    x = a1 * np.sin(ph) + 0.42 * a2 * np.sin(2 * ph + 0.3) + 0.22 * a3 * np.sin(3 * ph + 0.9) + 0.10 * a4 * np.sin(4 * ph)
    knock = env * np.exp(-t / 0.018) * np.sin(2.73 * ph) * 0.30
    thump = bp(RNG.standard_normal(n), sr, 300, 1500) * np.exp(-t / 0.006) * 0.35
    x = x + knock + thump
    x = sat(x * (0.8 + 0.6 * np.exp(-t / 0.08)), 3.0)          # (driven hardest at the strike: hollow, woody)
    x = lp(x, sr, 5200, 2)
    x = hp(x, sr, 24, 2)
    x *= np.clip((dur - t) / 0.14, 0, 1) ** 2
    return norm(x, -0.5)


def design_udu(sr=16000, dur=0.32):
    """the udu's bass tone: the clay pot's Helmholtz resonance, struck on its side hole: a low tone whooping up three
    semitones as the hand lifts, the slap of the palm, a little breath"""
    n = int(dur * sr)
    t = np.arange(n) / sr
    f0 = 98.0
    f = f0 * 2 ** ((3.0 * (1 - np.exp(-t / 0.06))) / 12)
    ph = sweep_phase(f, sr)
    env = np.where(t < 0.004, t / 0.004, 1.0) * np.exp(-t / 0.10)
    x = env * (np.sin(ph) + 0.18 * np.sin(2 * ph) + 0.05 * np.sin(3 * ph))
    slap = bp(RNG.standard_normal(n), sr, 700, 2600) * np.exp(-t / 0.007) * 0.30
    breath = bp(RNG.standard_normal(n), sr, 150, 600) * env * 0.05
    x = sat(x + slap + breath, 1.4)
    x = hp(x, sr, 40, 2)
    x *= np.clip((dur - t) / 0.1, 0, 1) ** 2
    return norm(x, -1.0)


def design_bottle(sr=24000, dur=0.16):
    """a glass bottle struck with a stick: four inharmonic modes of the glass, the higher ones gone sooner, the tick
    of the stick"""
    n = int(dur * sr)
    t = np.arange(n) / sr
    f1 = 1180.0
    modes = [(1.0, 1.0, 0.09), (1.006, 0.5, 0.09), (2.32, 0.45, 0.05), (4.25, 0.28, 0.03), (6.63, 0.16, 0.018),
             (9.1, 0.08, 0.01)]
    x = sum(a * np.sin(2 * np.pi * f1 * r * t + r) * np.exp(-t / d) for r, a, d in modes)
    tick = bp(RNG.standard_normal(n), sr, 2500, 9000) * np.exp(-t / 0.0012) * 0.5
    x = (x + tick) * np.where(t < 0.0004, t / 0.0004, 1.0)
    x = hp(x, sr, 400, 2)
    x *= np.clip((dur - t) / 0.06, 0, 1) ** 2
    return norm(x, -1.5)


DESIGNED = {
    "kick": (design_kick, 22050),
    "logdrum_lo": (lambda: design_logdrum(46.25), 16000),     # F#1: the zone for E1 .. C#2
    "logdrum_hi": (lambda: design_logdrum(92.50), 16000),     # F#2: the zone for D2 .. up
    "udu": (design_udu, 16000),
    "bottle": (design_bottle, 24000),
}


# ------------------------------------------------------------------ recorded --- #
def fetch():
    RAW.mkdir(parents=True, exist_ok=True)
    for name, (path, *_rest) in RECORDED.items():
        dest = RAW / Path(path).name
        if dest.exists():
            continue
        req = urllib.request.Request(VCSL + urllib.parse.quote(path), headers={"Range": "bytes=0-399999"})
        with urllib.request.urlopen(req) as r:
            dest.write_bytes(r.read())
        print(f"  fetched {dest.name}")


def recorded(name):
    """the take cut to its hit and stored at its rate; retuned by `semi` semitones (resampled: the agogo's high bell
    a fourth above the low one, the two pitches of the ogene)"""
    path, rate, keep, fade, hpf, semi, pre = RECORDED[name]
    sr, x = read_wav(RAW / Path(path).name)
    x = x - np.mean(x[:max(1, min(len(x), 64))])                # (any DC offset of the recording)
    pk = np.max(np.abs(x))
    if name in ONSET:                                            # (the envelope, 2 ms smoothing)
        w = max(1, int(0.002 * sr))
        e = np.convolve(np.abs(x), np.ones(w) / w, "same")
        on = int(np.argmax(e > ONSET[name] * np.max(e)))
    else:
        on = int(np.argmax(np.abs(x) > 0.03 * pk))               # the hit, a fraction of a millisecond early
    x = x[max(0, on - int(pre * sr / 1000)):]
    x = hp(x, sr, hpf, 2)
    src = sr * 2 ** (semi / 12)                                  # a retune: the take read as if recorded at src
    up, down = rate, int(round(src))
    g = math.gcd(up, down)
    x = signal.resample_poly(x, up // g, down // g, window=("kaiser", 8.0))
    x = x[:int(keep * rate)].copy()
    x = cos_fade(x, int(fade * rate))
    if pre > 0:                                                  # the first samples in on a quarter sine (no step)
        k = min(len(x), int(0.0002 * rate) + 1)
        x[:k] *= np.sin(0.5 * np.pi * np.arange(1, k + 1) / k)
    return norm(x, -0.3), rate


def main(argv):
    if "--fetch" in argv:
        fetch()
    OUT.mkdir(parents=True, exist_ok=True)
    for name, (fn, sr) in DESIGNED.items():
        x = fn()
        write_wav(OUT / f"{name}.wav", x, sr)
        print(f"  designed {name:12s} {len(x) / sr:.3f} s at {sr}")
    lines = ["THE ARRIVAL bank (EDDA OS): the sounds in this folder", "",
             "Designed (tools/edda_sounds.py, synthesised, GPL-3.0-only with the firmware):",
             "  " + ", ".join(f"{n}.wav" for n in DESIGNED), "",
             "Recorded: Versilian Studios' VCSL, CC0 1.0 (public domain dedication)",
             "  https://github.com/sgossner/VCSL  https://creativecommons.org/publicdomain/zero/1.0/",
             "  each cut to its hit, high-passed, faded, normalised and resampled by tools/edda_sounds.py", ""]
    for name in RECORDED:
        if not (RAW / Path(RECORDED[name][0]).name).exists():
            print(f"  (no {name}: run with --fetch)")
            continue
        x, sr = recorded(name)
        write_wav(OUT / f"{name}.wav", x, sr)
        lines.append(f"  {name}.wav  <-  {CREDIT}: {RECORDED[name][0]}")
        print(f"  recorded {name:12s} {len(x) / sr:.3f} s at {sr}")
    (OUT / "ATTRIBUTION.txt").write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main(sys.argv[1:])
