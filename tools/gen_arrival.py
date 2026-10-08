#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""EDDA OS: THE ARRIVAL, the first thirteen songs of EDDA as music the FM-1 holds (firmware/src/arrival.c).

  tools/gen_arrival.py OUT.h [JSON]

Each song is four tracks (the EDDA template: T1 the log drum or the bass, T2 the keys, T3 the lead or the pad, T4 the
drums), up to four sections (A..D: each its own steps for every track, LEN / DIV, its automation) and an arrangement
(up to 16 rows of a section and its repeats: the SONG page's chain). The sounds are the firmware's own engines and
presets with parameter overrides (THE ARRIVAL's kit is DRUM KIT EDDA, the log drum SAMPLE SET LOG: tools/edda_sounds.py).

Written here as music (note names, drum rows, chords), encoded compactly (arrival.c decodes):
  bars      16 steps; an event per step: b0 = step | TIE << 4 | EXT << 5 | DRUM << 6; a NOTE adds b1 = vel5 | n << 5
            (velocity (vel5 << 2) | 3), a DRUM bar's hit and accent bytes, n notes, then (EXT) flags and chance;
            0xFF ends a bar. Bars are shared: a pattern is a list of bar references (bar id, transposition), and
            a bar that is another's transposed is stored once.
  patterns  [n] then n x (bar lo, bar hi, transpose): up to four bars (64 steps)
  params    [n] then n x (id, value + 64): a track's sound over its preset, the song's globals
  motion    [n] then n x (track << 6 | step, param id, value + 64): a section's automation (core.h motion_event_t)
"""
import json
import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
CORE = (SRC / "firmware" / "src" / "core.h").read_text()


def _enum(first):
    """the ids of the core.h enum that starts with `first` (P_LEVEL, G_BPM)"""
    body = CORE[CORE.index(first):]
    body = body[:body.index("};")]
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    names = [w.strip() for w in body.replace("\n", " ").split(",") if w.strip()]
    return {n: i for i, n in enumerate(names) if re.fullmatch(r"[PG]_\w+", n)}


P = _enum("P_LEVEL")
G = _enum("G_BPM")
assert P["P_E0"] == 91 and P["P_LN0"] == 83 and G["G_RTYPE"] == 24, "core.h moved: check the ids"

ENG = {"ANALOG": 0, "PHASE": 2, "LOFI": 3, "SAMPLE": 4, "VOICE": 5, "TRIO": 6, "WHEEL": 7, "GRAIN": 8, "PHYS": 9,
       "DRUM": 10, "NOISE": 11, "FM6": 12}
# the factory presets used (engines.c and the eng_*.c tables; tests/edda_test.c checks the names)
PRESET = {("ANALOG", "SAW LEAD"): 0, ("ANALOG", "SOFT PAD"): 1, ("ANALOG", "SQR BASS"): 2, ("ANALOG", "PWM STR"): 3,
          ("ANALOG", "SINE KEY"): 5, ("ANALOG", "SUB BASS"): 7, ("ANALOG", "PLUCK"): 8, ("ANALOG", "BRASS"): 9,
          ("ANALOG", "STRINGS"): 11, ("PHASE", "BRASS"): 0, ("PHASE", "STRING"): 2, ("PHASE", "BELL"): 4,
          ("SAMPLE", "PIANO"): 0, ("SAMPLE", "FLUTE"): 2, ("SAMPLE", "SAX"): 3, ("VOICE", "CHOIR AAH"): 0,
          ("VOICE", "VOX LEAD"): 1, ("WHEEL", "GOSPEL"): 2, ("WHEEL", "SOFT FLUTE"): 3, ("WHEEL", "JAZZ PERC"): 1,
          ("GRAIN", "CLOUD PAD"): 0, ("GRAIN", "SHIMMER"): 3, ("PHYS", "MARIMBA"): 1, ("PHYS", "PLUCK"): 2,
          ("PHYS", "KALIMBA"): 4, ("PHYS", "DRONE STRING"): 7, ("PHYS", "HARP"): 8, ("DRUM", "DRUM KIT"): 0,
          ("FM6", "TINE EP"): 0, ("FM6", "BELL"): 1, ("FM6", "FM BASS"): 2, ("FM6", "BRASS"): 3, ("FM6", "PAD"): 4,
          ("FM6", "MARIMBA"): 5, ("FM6", "ORGAN"): 6, ("FM6", "PLUCK"): 7, ("FM6", "OGENE"): 8, ("FM6", "OJA"): 9,
          ("FM6", "HILIFE"): 10, ("FM6", "TALK DRUM"): 11, ("FM6", "LOG DRUM"): 12,
          ("FM6", "EDDA EP"): 13}
SET_PIANO, SET_FLUTE, SET_SAX, SET_EKIT, SET_LOG = 0, 2, 3, 9, 10    # SAMPLE SET values (eng_sample.c)
KIT_EDDA = 13                                                         # DRUM KIT EDDA (eng_drum.c DK_EDDA)
DIV = {"1/4": 0, "1/8": 1, "1/16": 2, "1/32": 3, "8T": 4, "16T": 5}

NOTE = {"C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3, "E": 4, "F": 5, "F#": 6, "Gb": 6, "G": 7, "G#": 8, "Ab": 8,
        "A": 9, "A#": 10, "Bb": 10, "B": 11}


def nn(s):
    """'A1' -> MIDI (C4 = 60), or an int as it is"""
    if isinstance(s, int):
        return s
    m = re.fullmatch(r"([A-G][#b]?)(-?\d)", s)
    if not m:
        raise ValueError(f"note {s!r}")
    return (int(m.group(2)) + 1) * 12 + NOTE[m.group(1)]


def notes(x):
    if isinstance(x, (list, tuple)):
        return [nn(v) for v in x]
    return [nn(v) for v in str(x).split()] if isinstance(x, str) else [nn(x)]


# ------------------------------------------------------------------- steps --- #
# a step as the firmware holds it (core.h step_t): time 0 NOTE, 1 TIE, 2 REST
def empty(n):
    return [None] * n


def step(time=0, nts=(), vel=100, hit=0, acc=0, flags=0, prob=0):
    return dict(time=time, notes=list(nts), vel=vel, hit=hit, acc=acc, flags=flags, prob=prob)


SF_ACCENT, SF_SLIDE, SF_FILL = 1, 2, 32


def mel(length, events, vel=100):
    """a melodic pattern of `length` steps: events (step, notes, steps held[, opts]); opts: 'A' accent, 'S' slide
    into the next note, 'F' fill-only, 'vNN' velocity, 'rN' ratchet (N hits), 'pNN' chance %"""
    st = empty(length)
    for ev in events:
        s, nts, ln = ev[0], ev[1], ev[2]
        opts = ev[3] if len(ev) > 3 else ""
        v, fl, pr = vel, 0, 0
        for o in re.findall(r"[ASF]|v\d+|r\d|p\d+", opts):
            if o == "A":
                fl |= SF_ACCENT
            elif o == "S":
                fl |= SF_SLIDE
            elif o == "F":
                fl |= SF_FILL
            elif o[0] == "v":
                v = int(o[1:])
            elif o[0] == "r":
                fl |= (int(o[1:]) - 1) << 3
            elif o[0] == "p":
                pr = int(o[1:])
        if not 0 <= s < length:
            raise ValueError(f"step {s} outside {length}")
        if st[s] is not None and st[s]["time"] == 0:
            raise ValueError(f"two notes at step {s}")
        st[s] = step(0, notes(nts), v, flags=fl, prob=pr)
        for k in range(1, ln):
            if s + k < length and st[s + k] is None:
                st[s + k] = step(1)
    return st


# drum rows: the lanes (hit bits, eng_drum.c DRUM_LANE_NOTE) and the pads named by a step's notes (the EDDA kit)
LANE = {"K": 0, "S": 1, "C": 2, "H": 3, "O": 4, "T": 5, "R": 6, "B": 7}
PAD = {"L": 41, "E": 43, "Q": 47, "U": 48, "W": 49, "G": 50, "Z": 51, "A": 53}
LEVEL = {"o": 46, "x": 92, "X": 127}


def lvl(c):
    if c in LEVEL:
        return LEVEL[c]
    if c.isdigit() and c != "0":
        return 20 + (int(c) - 1) * 107 // 8                # 1 .. 9 -> 20 .. 127
    raise ValueError(f"drum level {c!r}")


def drum(length, rows, fills=(), ratch=None):
    """a drum pattern: rows {lane: string of `length` chars} ('.' rest, 'o' ghost, 'x' hit, 'X' accent, '1'..'9' a
    level); the kick is always accented (hard and forward: the EDDA rule); lanes A..Z of PAD are notes. A step's
    hits share one velocity (the loudest plain hit there) and their own accents. fills: steps that play only while
    FILL is on. ratch: {step: hits}"""
    st = empty(length)
    for s in range(length):
        hit = acc = 0
        plain, pads = [], []
        for k, row in rows.items():
            row = row.replace(" ", "").replace("|", "")
            if len(row) != length:
                raise ValueError(f"row {k}: {len(row)} steps, not {length}")
            c = row[s]
            if c == ".":
                continue
            if k in LANE:
                hit |= 1 << LANE[k]
                if k == "K" or c == "X":
                    acc |= 1 << LANE[k]
                else:
                    plain.append(lvl(c))
            else:
                pads.append(PAD[k])
                plain.append(lvl(c))
        if hit or pads:
            fl = SF_FILL if s in fills else 0
            if ratch and s in ratch:
                fl |= (ratch[s] - 1) << 3
            st[s] = step(0, pads[:4], max(plain) if plain else 100, hit, acc, fl)
    return st


def cat(*pats):
    out = []
    for p in pats:
        out += p
    return out


def rep(p, n):
    return p * n


# ------------------------------------------------------------------ encoder --- #
class Bank:
    def __init__(self):
        self.data = bytearray([0])                  # offset 0: "none"
        self.bars, self.bar_ids = [], {}            # bar bytes; key -> (id, base transposition)
        self.stats = dict(bars=0, refs=0, transposed=0)

    def put(self, b):
        off = len(self.data)
        self.data += bytes(b)
        if len(self.data) > 0xFFFF:
            raise SystemExit("ARV_DATA past 64 KiB")
        return off

    @staticmethod
    def bar_bytes(steps, drum_bar, tr=0):
        b = bytearray()
        for i, s in enumerate(steps):
            if s is None:
                continue
            if s["time"] == 1:
                b.append(i | 0x10)
                continue
            ext = 1 if (s["flags"] or s["prob"]) else 0
            nts = [n - tr for n in s["notes"]]
            if not 0 <= len(nts) <= 4 or any(not 0 <= n <= 127 for n in nts):
                raise ValueError(f"notes {nts}")
            v5 = max(0, min(31, (s["vel"] - 3 + 2) // 4))
            b += bytes([i | ext << 5 | (0x40 if drum_bar else 0), v5 | len(nts) << 5])
            if drum_bar:
                b += bytes([s["hit"], s["acc"] & s["hit"]])
            b += bytes(nts)
            if ext:
                b += bytes([s["flags"], s["prob"]])
        b.append(0xFF)
        return bytes(b)

    def bar(self, steps, drum_bar):
        """a bar's id and its transposition: a melodic bar that is a stored one transposed is that one"""
        tr = 0
        if not drum_bar:
            ns = [n for s in steps if s and s["time"] == 0 for n in s["notes"]]
            if ns:
                tr = min(ns) - 36                    # (stored relative to its lowest note on C2)
        key = self.bar_bytes(steps, drum_bar, tr)
        if key not in self.bar_ids:
            self.bar_ids[key] = len(self.bars)
            self.bars.append(key)
        else:
            self.stats["transposed"] += tr != 0
        return self.bar_ids[key], tr

    def pattern(self, steps, drum_pat):
        if not steps or all(s is None for s in steps):
            return 0, len(steps)
        n = (len(steps) + 15) // 16
        refs = []
        for k in range(n):
            bar = steps[k * 16:k * 16 + 16]
            bar += [None] * (16 - len(bar))
            bid, tr = self.bar(bar, drum_pat)
            refs += [bid & 0xFF, bid >> 8, tr & 0xFF]
            self.stats["refs"] += 1
        return self.put([n] + refs), len(steps)

    def params(self, pairs):
        if not pairs:
            return 0
        out = [len(pairs)]
        for pid, v in pairs:
            if not -64 <= v <= 127:
                raise ValueError(f"param {pid} = {v}")
            out += [pid, v + 64]
        return self.put(out)

    def motion(self, events):
        if not events:
            return 0
        if len(events) > 64:
            raise ValueError("more than 64 motion events in a section")
        out = [len(events)]
        for trk, s, pid, v in sorted(events):
            out += [trk << 6 | s, pid, v + 64]
        return self.put(out)


def sound(engine, preset, role="", **over):
    """a track's sound: the engine's factory preset and overrides by parameter name (P_ED_FX= or ED_FX=); role:
    what it plays (the mix check's targets: log, kit, keys, lead, pad, bass)"""
    pairs = []
    for k, v in over.items():
        pid = P[k if k.startswith("P_") else "P_" + k]
        pairs.append((pid, int(v)))
    return dict(engine=engine, preset=preset, over=pairs, role=role)


def C(s):
    return f'"{s}"'


def encode(songs, out_h, out_json=None):
    bank = Bank()
    rows = []
    meta = []
    for si, sg in enumerate(songs):
        snd = []
        for k, lv in enumerate(sg.get("mix") or []):     # the mix: each track's LEVEL (over its sound's)
            if lv is not None:
                t = sg["tracks"][k]
                t["over"] = [(pid, v) for pid, v in t["over"] if pid != P["P_LEVEL"]] + [(P["P_LEVEL"], int(lv))]
        for k, t in enumerate(sg["tracks"]):
            e = ENG[t["engine"]]
            pr = PRESET[(t["engine"], t["preset"])]
            snd.append((e, pr, bank.params(t["over"])))
        secs = []
        for name, sec in sg["sections"]:
            pats, lens, divs = [], [], []
            tps = [tp if isinstance(tp, tuple) else (tp, "1/16") for tp in sec["tracks"]]
            seclen = sec.get("length") or max(len(st) for st, _ in tps) or 16
            if seclen > 64 or any(len(st) > 64 for st, _ in tps):
                raise ValueError(f"{sg['name']} {name}: a pattern longer than 64 steps")
            for k in range(4):
                steps, div = tps[k]
                if k == 0 or not steps:                 # T1 sets the section's length (the chain counts its loops)
                    steps = list(steps) + [None] * (seclen - len(steps))
                off, ln = bank.pattern(steps, sg["tracks"][k]["engine"] == "DRUM")
                pats.append(off)
                lens.append(max(1, min(64, ln)))
                divs.append(DIV[div])
            mot = bank.motion([(k, s, P[pid if pid.startswith("P_") else "P_" + pid], v)
                               for k, s, pid, v in sec.get("motion", [])])
            secs.append((name, pats, lens, divs, mot))
        if len(secs) > 4:
            raise ValueError(f"{sg['name']}: more than four sections")
        arr = sg["arrangement"]
        if not 1 <= len(arr) <= 16:
            raise ValueError(f"{sg['name']}: {len(arr)} arrangement rows")
        secidx = {name: i for i, (name, *_r) in enumerate(secs)}
        chain = [(secidx[n], r) for n, r in arr]
        if any(not 1 <= r <= 16 for _, r in chain):
            raise ValueError(f"{sg['name']}: a repeat outside 1..16")
        glob = bank.params([(G["G_" + k], v) for k, v in sg.get("globals", {}).items()])
        rows.append((sg, snd, secs, chain, glob))
        bars, t, timeline = 0, 0.0, []
        step_s = 60.0 / sg["bpm"] / 4
        for n, rr in arr:
            ln = secs[secidx[n]][2][0]
            dt = rr * ln * step_s
            timeline.append(dict(section=n, repeat=rr, start=round(t, 4), end=round(t + dt, 4)))
            t += dt
            bars += rr * ln / 16
        meta.append(dict(name=sg["name"], bpm=sg["bpm"], cam=sg["cam"], bars=bars, seconds=round(t, 2),
                         timeline=timeline, roles=[tr.get("role", "") for tr in sg["tracks"]],
                         levels=[dict(tr["over"]).get(P["P_LEVEL"], 104) for tr in sg["tracks"]]))
    L = ["/* generated by tools/gen_arrival.py: THE ARRIVAL (EDDA OS, firmware/src/arrival.c) */", "#pragma once",
         f"#define ARV_NSONGS {len(rows)}", f"#define ARV_NBARS {len(bank.bars)}"]
    offs, blob = [], bytearray()
    base = len(bank.data)
    for b in bank.bars:
        offs.append(base + len(blob))
        blob += b
    data = bank.data + blob
    if len(data) > 0xFFFF:
        raise SystemExit("ARV_DATA past 64 KiB")
    L.append(f"static const uint8_t ARV_DATA[{len(data)}] = {{")
    for i in range(0, len(data), 32):
        L.append("    " + ",".join(map(str, data[i:i + 32])) + ",")
    L.append("};")
    L.append(f"static const uint16_t ARV_BAR[{len(offs)}] = {{")
    for i in range(0, len(offs), 16):
        L.append("    " + ", ".join(map(str, offs[i:i + 16])) + ",")
    L.append("};")
    L.append("static const arv_song_t ARV_SONGS[ARV_NSONGS] = {")
    for sg, snd, secs, chain, glob in rows:
        cam = sg["cam"]
        camv = (int(cam[:-1]) - 1) * 2 + (cam[-1] == "B") + 1
        short = sg.get("short") or sg["name"]
        if len(sg["name"]) > 19 or len(short) > 12 or len(sg["chant"]) > 10 or len(sg["line"]) > 23:
            raise ValueError(f"{sg['name']}: a name too long (title 19, short 12, chant 10, line 23)")
        L.append(f"    {{{C(sg['name'])}, {C(short)}, {C(sg['chant'])}, {C(sg['line'])}, {sg['bpm']}, {camv}, {len(secs)}, {len(chain)}, "
                 f"{1 if sg.get('half') else 0}, {glob},")
        L.append("     {" + ", ".join(f"{{{e}, {p}, {o}}}" for e, p, o in snd) + "},")
        sl = []
        for name, pats, lens, divs, mot in secs:
            sl.append(f"{{{{{', '.join(map(str, pats))}}}, {{{', '.join(map(str, lens))}}}, "
                      f"{{{', '.join(map(str, divs))}}}, {mot}, {C(name)}}}")
        L.append("     {" + ", ".join(sl) + "},")
        L.append("     {" + ", ".join(f"{{{a}, {r}}}" for a, r in chain) + "}},")
    L.append("};")
    Path(out_h).write_text("\n".join(L) + "\n")
    total = len(data) + len(offs) * 2
    print(f"arrival: {len(rows)} songs, {len(bank.bars)} bars ({bank.stats['refs']} references, "
          f"{bank.stats['transposed']} transposed), {total} B of data")
    if out_json:
        Path(out_json).write_text(json.dumps(meta, indent=1))


def main(argv):
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from arrival_songs import SONGS                 # tools/arrival_songs.py: the music
    encode(SONGS, argv[0], argv[1] if len(argv) > 1 else None)


if __name__ == "__main__":
    main(sys.argv[1:])
