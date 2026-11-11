# SPDX-License-Identifier: GPL-3.0-only
"""OYA COME's sound pack: the recipe (tools/arrival_pack.py RECIPE OUTDIR SOURCES).

SOURCES holds the record split into stems, 44.1 kHz (the split is not part of this: any separator's stems, named
  mix.wav       the record itself
  vox.wav       the voice (lead, backing, its reverb)
  pad.wav       what is neither voice, drums nor bass (the chords and the line on top of them)
  drumbass.wav  the drums and the bass together
  d_kick.wav d_snare.wav d_toms.wav d_hh.wav    the drums' pieces (the bass's rest is in the kick's)
). Everything else is read off the record here: where its beats are, which bars are which, what is cut from where.

The record: 113.7732 BPM, the first downbeat at 0.05145 s, 64 bars of 4/4 in B minor: | G | G | Bm | A |, every
change an eighth early. Bars are counted from 0, steps are 16ths.
"""
import json
import sys
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import butter, fftconvolve, sosfiltfilt

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from arrival_pack import Pack, UNIT                         # noqa: E402

REC_P, REC_BAR0 = 0.527365, 0.05145       # the record's beat (seconds) and its first downbeat
SONG_BPM = 114                            # the song's tempo on the FM-1 (whole BPM)
S16 = REC_P / 4.0
NAME = "OYACOME"

# the slots: USR4 (72 KiB) the kit, USR1 the bass and the chords; the voice in what is left of all four
SLOT_KIT, SLOT_SYN = 3, 0
# the notes. The kit's pads: the DRUM engine's own notes (eng_drum.c DRUM_LANE_NOTE and the GM map), so the drums'
# patterns play KIT EDDA when the pack is not there. The bass: SAMPLE's keys B0..B2 around its G1. The chords: C3 D3
# E3 (as the record ends: F3 G3 A3). The voice's phrases: from C4 up in every slot, off the hats' lane (69 70 73 74)
N_KICK, N_KICK_B = 36, 35
N_SHK = (42, 44)
# the bar of percussion, cut: name: (bar, from step, to step); their pads, and the low-passed ones'
TILES = {"hi": (37, 2, 5), "lo": (37, 5, 9), "long": (37, 9, 13), "end_down": (33, 13, 18), "end_one": (34, 13, 18)}
N_TILE = {"hi": 47, "lo": 45, "long": 43, "end_down": 41, "end_one": 51}
N_DULL = {"hi": 60, "lo": 62, "long": 64}
N_BASS_ROOT = 31
N_LOG_ROOT = 95                           # the log drum's B2 on the bass's track: B6, an octave either way
PAD_NOTES = {"G": 48, "Bm": 50, "A": 52, "G_end": 53, "Bm_end": 55, "A_end": 57}
VOX_NOTES = [60, 61, 62, 63, 64, 65, 66, 67, 68, 71, 72, 75, 76, 77, 78, 79]


class Src:
    def __init__(self, d):
        self.d, self.c = Path(d), {}

    def __call__(self, name):
        if name not in self.c:
            x, sr = sf.read(self.d / f"{name}.wav", dtype="float32", always_2d=True)
            assert sr == 44100, sr
            self.c[name] = x.mean(1).astype(np.float64)
        return self.c[name]

    def mix(self, names):
        return sum(self(n) for n in names)


SR = 44100


def at(bar, step=0.0):
    """the record's time of a bar's step"""
    return REC_BAR0 + (bar * 16 + step) * S16


def cut(x, t0, t1):
    return x[int(round(t0 * SR)):int(round(t1 * SR))].copy()


def band(x, lo, hi):
    if lo and hi:
        sos = butter(4, [lo, hi], "bandpass", fs=SR, output="sos")
    elif lo:
        sos = butter(4, lo, "highpass", fs=SR, output="sos")
    else:
        sos = butter(4, hi, "lowpass", fs=SR, output="sos")
    return sosfiltfilt(sos, x)


def average_hits(x, places, length, pre=0.002, search=0.008, keep=0.75):
    """one hit from many: the hits at `places` ((bar, step)) aligned to the first by cross-correlation and averaged
    (what is the hit adds up, what bleeds in from the rest does not); the ones that are not the same sound (a
    correlation under `keep` with the first) are left out. -> (the hit, how many made it)"""
    L, lag = int(length * SR), int(search * SR)
    segs = []
    for b, s in places:
        i = int(round((at(b, s) - pre) * SR))
        segs.append(x[i - lag:i + L + lag])
    ref = segs[0][lag:lag + L]
    got = [ref]
    for s in segs[1:]:
        xc = fftconvolve(s, ref[::-1], "valid")
        j = int(np.argmax(xc))
        a = s[j:j + L]
        c = float(np.dot(a, ref) / (np.linalg.norm(a) * np.linalg.norm(ref) + 1e-12))
        if c >= keep:
            got.append(a)
    return np.mean(got, axis=0), len(got)


def tail_trim(x, floor_db=-50.0, hold=0.01):
    """a hit up to where it has rung out (the last sample above the floor re its peak, and a little more)"""
    thr = np.abs(x).max() * 10 ** (floor_db / 20.0)
    idx = np.nonzero(np.abs(x) > thr)[0]
    end = min(len(x), (idx[-1] if len(idx) else 0) + int(hold * SR))
    return x[:end]


def drums(P, S, report):
    """The kit is taken as it sounds, not taken apart. The kick: bar 37's (no bass under it), the downbeat's (it has
    a click the others have not) and the second beat's (the thump every other beat has, and all the beats before
    the first verse). The rest is hands on congas with a shaker, never twice the same: the pack holds a bar of it
    as it is (bars 33 and 34, no bass), cut where its strokes start: a 32nd's grace before each conga (steps 2, 5,
    9: cut on the 16th before), and the bar's end from step 13 through the next bar's downbeat, which is struck
    differently on every other bar (two cuts: into an even bar, into an odd one). Played in order the cuts are the
    bar again. Before the first verse the record plays the congas behind a low-pass: the same three cuts through
    one (their own zones, at a lower rate), under the shaker alone (two strokes of it as they are)."""
    kick = S("d_kick")
    perc = S.mix(["d_snare", "d_toms", "d_hh", "d_ride", "d_crash"])
    P.add("kick", tail_trim(cut(kick, at(37, 0) - 0.002, at(37, 0) + 0.30)), SR, 128, N_KICK, group="kick",
          slot=SLOT_KIT, sync=False, fade=(0, 0.02))
    P.add("kick_b", tail_trim(cut(kick, at(37, 4) - 0.002, at(37, 4) + 0.30)), SR, 32, N_KICK_B, group="kick",
          slot=SLOT_KIT, sync=False, fade=(0, 0.02))
    pre, xf = 0.003, 0.004                 # a cut starts just before its step (the strokes land a hair early)
    dull = sosfiltfilt(butter(1, 990, "lowpass", fs=SR, output="sos"), perc)   # (twice through: -3 dB at 700 Hz)
    for name, (bar, a, b) in TILES.items():
        x = cut(perc, at(bar, a) - pre, at(bar, b) - pre + xf)
        P.add(name, x, SR, 128, N_TILE[name], group="perc", slot=SLOT_KIT, fade=(0.0005, xf))
        if name in N_DULL:                 # before the first verse: the same strokes behind the record's low-pass
            x = cut(dull, at(bar, a) - pre, at(bar, b) - pre + xf)   # (6 dB an octave from 700 Hz, as measured)
            P.add(name + "_dull", x, SR, 40, N_DULL[name], group="perc", slot=SLOT_KIT, fade=(0.001, xf))
    hh = S("d_hh")
    for i, (b, s) in enumerate(((53, 14), (53, 7))):
        x = cut(hh, at(b, s) - 0.002, at(b, s) + 0.07)       # (at the full rate: the kit's air is the shaker's)
        P.add(f"shaker_{i + 1}", band(x, 1500, 0), SR, 256, N_SHK[i], group="shaker", slot=SLOT_KIT, sync=False,
              fade=(0.001, 0.03))


def harmonics(seg, f0, count):
    """the amplitudes of f0's harmonics in seg (a windowed fit)"""
    n = np.arange(len(seg))
    w = np.hanning(len(seg))
    return np.array([abs(2.0 * np.sum(seg * w * np.exp(-2j * np.pi * h * f0 * n / SR)) / w.sum())
                     for h in range(1, count + 1)])


def bass(P, S, report):
    """the bass is one held tone, so the pack holds its wave and SAMPLE plays the notes: the harmonics of the G where
    nothing else sounds (after the kick, before the conga: steps 1.6..3 and 13.6..16 of the hooks' G bars), the
    median over the windows, as two cycles at 11025 Hz (225 samples a cycle: G1 to a hundredth of a cent)"""
    x = S("drumbass")
    wins = [cut(x, at(b, 13.6), at(b, 15.9)) for b in (24, 28, 40, 44, 48, 52, 56)]
    wins += [cut(x, at(b, 1.6), at(b, 3.0)) for b in (24, 25, 28, 40, 41, 44, 45, 48, 49, 52, 53, 56, 57)]
    H = 24
    mags = []
    for w in wins:
        fs = np.arange(48.0, 50.0, 0.02)
        f0 = fs[int(np.argmax([np.sum(harmonics(w, f, 6) ** 2) for f in fs]))]
        mags.append(harmonics(w, f0, H))
    m = np.median(np.array(mags), axis=0)
    m[m < m[0] * 10 ** (-60 / 20.0)] = 0.0              # (under -60 dB: the stem's noise, not the bass)
    report("bass: harmonics (dB re the fundamental) " + " ".join(f"{20 * np.log10(v / m[0] + 1e-9):.0f}" for v in m[:8]))
    per = 225 * 4                                       # at 44100: 900 samples a cycle (49 Hz)
    t = np.arange(4 * per) / per
    # (phases: the fundamental's sine, each harmonic a quarter turn on: a low crest, a soft start at the zero crossing)
    y = sum(m[h - 1] * np.sin(2 * np.pi * h * t + (h - 1) * np.pi / 2) for h in range(1, H + 1))
    P.add("bass", y, SR, 64, N_BASS_ROOT, group="bass", slot=SLOT_SYN, sync=False, emph=False, kit=False,
          lo=N_BASS_ROOT - 8, hi=N_BASS_ROOT + 16, loop=(2 * per / SR, 4 * per / SR))


def log_drum(P, S, report):
    """the log drum that answers the bass in the hooks: its loudest stroke (bar 50, step 13: a B2), played by the
    bass's track (SAMPLE: from note N_LOG_ROOT, any pitch) where the bass leaves it room"""
    x = S.mix(["d_snare", "d_toms"])
    y = cut(band(x, 0, 2500), at(50, 13) - 0.006, at(50, 16))
    P.add("log", y, SR, 32, N_LOG_ROOT, group="log", slot=SLOT_SYN, sync=False, kit=False, fade=(0.001, 0.04),
          lo=N_LOG_ROOT - 12, hi=N_LOG_ROOT + 12)


def seamless(x, t0, length, xfade=0.03):
    """the stretch of x from t0, `length` long, made to loop: its end cross-faded into what came before its start"""
    i, n, f = int(round(t0 * SR)), int(round(length * SR)), int(xfade * SR)
    y = x[i:i + n].copy()
    w = np.sin(0.5 * np.pi * np.arange(f) / f) ** 2    # (equal gain: the two are the same sound a beat apart)
    y[n - f:] = y[n - f:] * (1.0 - w) + x[i - f:i] * w
    return y


def pad(P, S, report):
    """the chords and the line on top of them: a beat of each, looped (SAMPLE, LOOP ON); the top note is struck again
    every eighth, so a beat holds two of them and the loop keeps that pulse. Two sets: as they are before the first
    verse (bars 8, 10 and 11: bright, the chords in front; the A's beat is bar 11's first: later in that bar a low
    drum rolls under it), and as they end the record (bars 60..63: the line in front)"""
    x = S("pad")
    pre = 0.02
    for tag, src in (("", {"G": (8, 8), "Bm": (10, 8), "A": (11, 0)}), ("_end", {"G": (60, 8), "Bm": (62, 8), "A": (63, 8)})):
        for name, (bar, step) in src.items():
            y = seamless(x, at(bar, step), REC_P)
            z = np.concatenate([y, y[:int(pre * SR)]])  # (the loop is all of it; a little more, so the decoder's state
            P.add(f"pad_{name}{tag}", z, SR, 48, PAD_NOTES[name + tag], group="pad", slot=SLOT_SYN, kit=False,   # at the
                  loop=(pre, pre + REC_P))                                                    # loop's start is settled)


# The voice. It sings 57 of the 64 bars and the slots hold about a minute at the rates below, so every bar that is
# sung again (the hook's four bars, the chant, the line that answers them) is in the pack once and played where it
# comes round; the verses and the bridge are there two bars at a time. A zone starts on a step, mostly a bar's first: the
# record's own kick ducks the voice there (its bus pumps), so a cut on the beat falls where the voice is lowest and
# the song's kick lands on it. Rates: about 10 kHz for what repeats, 8269 Hz for the verses (the voice has little
# between 4 and 8 kHz: its air is above 8 kHz, more than the slots can hold at any rate that leaves room for it).
#   name: (bar, step, steps long, rate k, steps of tail)
VOX = {
    "A": (24, 0, 16, 58, 0), "B": (25, 0, 16, 58, 0), "C": (26, 0, 16, 58, 0), "D": (27, 0, 16, 58, 0),
    "B2": (29, 0, 16, 58, 0), "D2": (31, 0, 16, 58, 0),
    "E": (6, 0, 16, 56, 0), "F": (7, 0, 16, 56, 1), "T": (9, 0, 16, 58, 1),
    "L12": (12, 6, 10, 58, 0),
    "V16": (16, 8, 24, 48, 0), "V18": (18, 0, 32, 48, 0), "V20": (20, 0, 32, 48, 0), "V22": (22, 0, 32, 48, 0),
    "V32": (32, 0, 32, 48, 0), "V34": (34, 0, 32, 48, 0), "V36": (36, 0, 32, 48, 0), "V38": (38, 0, 16, 44, 0),
    "R40": (40, 0, 16, 44, 0), "R45": (45, 0, 16, 48, 0),
}
# The break (bars 46..47) is the whole record, not the voice alone: a gated chord, a fall, a gap, then the last
# hook. It is one zone cut from the mix, played where the drums and the bass stop
BREAK = ("BRK", 46, 0, 32, 44)
# where the song plays them: (bar, step, zone), the record's own order
VOX_PLAY = (
    [(0, 0, "A"), (1, 0, "B"), (2, 0, "C"), (3, 0, "D"), (4, 0, "A"), (5, 0, "B2"), (6, 0, "E"), (7, 0, "F"),
     (9, 0, "T"), (10, 0, "E"), (11, 0, "F"), (12, 6, "L12"), (13, 0, "T"), (14, 0, "E"), (15, 0, "F"),
     (16, 8, "V16"), (18, 0, "V18"), (20, 0, "V20"), (22, 0, "V22")] +
    [(24, 0, "A"), (25, 0, "B"), (26, 0, "C"), (27, 0, "D"), (28, 0, "A"), (29, 0, "B2"), (30, 0, "C"), (31, 0, "D2")] +
    [(32, 0, "V32"), (34, 0, "V34"), (36, 0, "V36"), (38, 0, "V38"), (39, 0, "T")] +
    [(40, 0, "R40"), (41, 0, "T"), (42, 0, "C"), (43, 0, "T"), (44, 0, "R40"), (45, 0, "R45"), (46, 0, "BRK"),
     (48, 0, "A"), (49, 0, "B"), (50, 0, "C"), (51, 0, "D"), (52, 0, "A"), (53, 0, "T"), (54, 0, "C"), (55, 0, "D2"),
     (61, 0, "T")])
XF = 0.008                                # a cut's cross-fade: the zone before fades out over it, this one in


def voice(P, S, report):
    x = S("vox")
    for name, (bar, step, steps, k, tail) in VOX.items():
        t0, t1 = at(bar, step), at(bar, step + steps)
        end = t1 + (tail * S16 if tail else XF)
        P.add("vox_" + name, cut(x, t0, end), SR, k, VOX_NOTES, group="vox", fade=(XF, tail * S16 if tail else XF),
              info=dict(steps=steps))
    name, bar, step, steps, k = BREAK
    P.add("vox_" + name, cut(S("mix"), at(bar, step), at(bar, step + steps)), SR, k, VOX_NOTES, group="vox",
          fade=(XF, XF), info=dict(steps=steps))


def pack(outdir, src):
    S = Src(src)
    notes = []
    P = Pack(NAME, tempo=(60.0 / SONG_BPM) / REC_P)
    drums(P, S, notes.append)
    bass(P, S, notes.append)
    log_drum(P, S, notes.append)
    pad(P, S, notes.append)
    voice(P, S, notes.append)
    man = P.build(outdir, dict(song="OYA COME", record_bpm=round(60.0 / REC_P, 4), song_bpm=SONG_BPM,
                               play=[list(v) for v in VOX_PLAY]))
    for n in notes:
        print("  " + n)
    return man
