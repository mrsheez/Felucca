#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""EDDA OS 2.0: a song's sound pack, cut from its record.

A pack is the FM-1's four user sample slots (firmware/src/eng_sample.c: USR1..3 80 KiB, USR4 72 KiB, IMA ADPCM, up to
16 zones a slot) filled for one song of THE ARRIVAL with the record's own sounds: its drums' hits, its bass and chords,
its voice in phrases. The song (tools/arrival_songs.py) plays them: the kit and the phrases through the DRUM engine's
user kits (a zone is a pad on its own note; a phrase starts on a step and runs on by itself: arrival.c's cues), the
bass and the chords through SAMPLE. The slots are named <NAME><1..4>, which is how the firmware knows the pack is
there (arrival.c arv_pack_ok); the installer page loads them over USB (the editor protocol's SMP_ commands).

  tools/arrival_pack.py RECIPE.py OUTDIR [SOURCES]   the recipe's pack(P, SOURCES) adds the zones (Pack.add)
  tools/arrival_pack.py --stand-in MANIFEST.json OUTDIR
                                                     the same layout filled with tones: what the tests play

OUTDIR gets usr1.bin .. usr4.bin (the slots as the flash holds them: the 480-byte header at 0, the ADPCM at 512),
manifest.json (every zone: its slot, note, length, rate; no audio) and zones/*.wav (each zone as the FM-1 will decode
it). A record's audio is its artist's: packs are built here and handed over, never committed.

What the encoding does beyond the editor's (tools/sampleio.py, web/editor.html):
  rates      a zone's own rate, a multiple of 44100 / 256 Hz: the engines hold a zone's rate as Q16 and drop its low
             8 bits (eng_drum.c kit_render, eng_sample.c), so any other rate plays a little flat and late; a phrase cut
             for a tempo plays exactly in time
  tempo      a record at 113.77 BPM played by a song at 114: every tempo-bound zone is resampled by the ratio (0.2 %,
             3.5 cents), so a phrase of four bars ends where the song's four bars end
  de-droop   the engines interpolate linearly: sinc^2, -7.8 dB at a zone's Nyquist. Each zone is equalised for it
             first (up to +6 dB at the top), so what comes out is flat to the top of its band
  ADPCM      tools/adpcm_xq.c: a beam search over the decoder's states with the error weighted by the sound's own
             tilt, about 1.5 dB of SNR over the nearest-code encoder and the noise under the voice, not above it
Needs numpy, soxr and soundfile (a studio tool: the firmware build and the tests do not run it, except --stand-in,
which needs none of the three).
"""
import json
import math
import struct
import subprocess
import sys
import zlib
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
UNIT = 44100.0 / 256.0                       # the rates a zone can play at exactly: k * UNIT (k: the rate's Q16 >> 8)
SLOT_SIZE = [0x14000, 0x14000, 0x14000, 72 * 1024]
SLOT_DATA = 512
SLOT_ZONES = 16
ZONE_MAX = 2 * 0x14000                       # eng_sample.c smp_user_scan: a zone's samples
MAGIC = 0x504D5346
KIT_PEAK = 19000                             # a kit's pad: x 1.25 into a knee at 24000 (eng_drum.c kit_render)
SMP_PEAK = 30000


def xq_path():
    """tools/adpcm_xq.c built with the host's cc (build/host/adpcm_xq)"""
    out = SRC / "build" / "host" / "adpcm_xq"
    src = SRC / "tools" / "adpcm_xq.c"
    if not out.exists() or out.stat().st_mtime < src.stat().st_mtime:
        out.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["cc", "-O2", "-o", str(out), str(src), "-lm"], check=True)
    return out


def ima_decode(codes, n):
    """the firmware's decoder (eng_sample.c smp_decode) -> (samples, the (predictor, index) before each sample)"""
    from sampleio import IMA_STEP, IMA_IDX
    pred = idx = 0
    out, st = [], []
    for i in range(n):
        st.append((pred, idx))
        c = (codes[i >> 1] >> ((i & 1) * 4)) & 15
        step = IMA_STEP[idx]
        vd = step >> 3
        if c & 4:
            vd += step
        if c & 2:
            vd += step >> 1
        if c & 1:
            vd += step >> 2
        pred = max(-32768, min(32767, pred - vd if c & 8 else pred + vd))
        idx = max(0, min(88, idx + IMA_IDX[c & 7]))
        out.append(pred)
    return out, st


def slot_image(name, zones, data):
    """a slot as the flash holds it. zones: dicts off n ls le k root lo hi pred idx looped"""
    zb = b"".join(struct.pack("<5I2h4B", z["off"], z["n"], z["ls"], z["le"], z["k"] << 8, z["root"] * 16, z["pred"],
                              z["idx"], z["lo"], z["hi"], 1 if z["looped"] else 0) for z in zones)
    hdr = struct.pack("<IHBB8sII8x", MAGIC, 1, len(zones), 0, name.encode("ascii").ljust(8, b"\0")[:8], len(data),
                      zlib.crc32(data)) + zb
    return hdr.ljust(SLOT_DATA, b"\0") + bytes(data)


def place(zones):
    """each zone into a slot: the pinned ones stay; the others by a search (largest first, every slot it fits tried,
    back when one is left over): a pack is nearly as large as the slots, and a slot takes 16 zones, so the first
    order that comes to mind seldom fits"""
    cap = [SLOT_SIZE[k] - SLOT_DATA for k in range(4)]
    room = list(cap)
    left = [SLOT_ZONES] * 4
    for z in zones:
        if z["slot"] is not None:
            room[z["slot"]] -= z["bytes"]
            left[z["slot"]] -= 1
    free = sorted((z for z in zones if z["slot"] is None), key=lambda z: -z["bytes"])
    if min(room) < 0 or min(left) < 0:
        raise SystemExit(f"pack: the pinned zones alone are too much: room {room}, zones left {left}")
    out = [None] * len(free)
    nodes = [0]

    def go(i):
        if i == len(free):
            return True
        nodes[0] += 1
        if nodes[0] > 2000000:
            return False
        b, seen = free[i]["bytes"], set()
        for k in sorted(range(4), key=lambda k: room[k]):   # (the tightest slot that takes it first)
            if room[k] < b or not left[k] or (room[k], left[k]) in seen:
                continue
            seen.add((room[k], left[k]))
            room[k] -= b
            left[k] -= 1
            out[i] = k
            if go(i + 1):
                return True
            room[k] += b
            left[k] += 1
        return False

    import sys
    sys.setrecursionlimit(10000)
    if not go(0):
        tot = sum(z["bytes"] for z in zones)
        raise SystemExit(f"pack: the zones fit no arrangement of the slots ({tot} B in all, the slots hold {sum(cap)} B "
                         f"and {SLOT_ZONES} zones each; {len(zones)} zones)")
    for z, k in zip(free, out):
        z["slot"] = k
    return [cap[k] - room[k] for k in range(4)]


def write_pack(name, zones, outdir, extra=None):
    """zones: dicts name slot codes n k root lo hi ls le pred idx looped (+ what the manifest keeps)"""
    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    if len(name) != 7:
        raise SystemExit("pack: a name of 7 characters (the slots are <NAME><1..4>)")
    for z in zones:
        z["bytes"] = (z["n"] + 1) // 2
        if not 0 < z["n"] <= ZONE_MAX or not 1 <= z["k"] <= 1024 or not z["ls"] <= z["le"] < z["n"]:
            raise SystemExit(f"pack: zone {z['name']}: {z['n']} samples, rate {z['k']}, loop {z['ls']}..{z['le']}")
    used = place(zones)
    for z in zones:                                  # the notes left to pick: the first free one in the zone's slot
        if z.get("pick"):
            taken = {n for o in zones if o["slot"] == z["slot"] and not o.get("pick") for n in range(o["lo"], o["hi"] + 1)}
            free = [n for n in z["pick"] if n not in taken]
            if not free:
                raise SystemExit(f"pack: no note left for {z['name']} in slot {z['slot'] + 1}")
            z["root"] = z["lo"] = z["hi"] = free[0]
            del z["pick"]
    man = dict(name=name, unit_hz=UNIT, slots=[], zones=[])
    if extra:
        man.update(extra)
    for k in range(4):
        zs = [z for z in zones if z["slot"] == k]
        data = bytearray()
        for z in zs:
            z["off"] = len(data)
            data += z["codes"]
        notes = sorted((z["lo"], z["hi"], z["name"]) for z in zs)
        for a, b in zip(notes, notes[1:]):
            if b[0] <= a[1]:
                raise SystemExit(f"pack: slot {k + 1}: {a[2]} and {b[2]} share a note")
        img = slot_image(f"{name}{k + 1}", zs, data) if zs else b""
        (outdir / f"usr{k + 1}.bin").write_bytes(img)
        man["slots"].append(dict(slot=k + 1, zones=len(zs), bytes=len(data), room=SLOT_SIZE[k] - SLOT_DATA - len(data),
                                 crc=zlib.crc32(bytes(data)) if zs else 0))
        for i, z in enumerate(zs):
            man["zones"].append({key: z[key] for key in z if key not in ("codes", "bytes", "off", "pcm", "pick")} |
                                dict(index=i, rate_hz=round(z["k"] * UNIT, 3), seconds=round(z["n"] / (z["k"] * UNIT), 4)))
    (outdir / "manifest.json").write_text(json.dumps(man, indent=1))
    return man, used


class Pack:
    """the zones of a pack as a recipe adds them"""

    def __init__(self, name, tempo=1.0, gamma=0.7, beam=16):
        self.name, self.tempo, self.gamma, self.beam = name, tempo, gamma, beam
        self.zones = []
        self.notes = []                       # (lines for the build's report)

    def add(self, name, x, fs, k, note, *, group, slot=None, sync=True, emph=True, fade=(0.0, 0.0), loop=None,
            lo=None, hi=None, kit=True, info=None):
        """a zone: x (mono, floats, at fs, in the record's time) played at k * UNIT Hz on `note` (lo..hi: the keys
        that play it, SAMPLE transposes from note; a list of notes: the first one free in the slot the zone lands
        in). group: the zones that share one gain (their loudest peak goes to full level: a voice's phrases keep
        their dynamics). sync: tempo-bound (resampled by the pack's tempo). fade: seconds in and out. loop:
        (start, end) seconds in x: the zone loops there (SAMPLE, LOOP ON)"""
        import numpy as np
        import soxr
        rate = k * UNIT
        scale = self.tempo if sync else 1.0
        y = soxr.resample(np.asarray(x, np.float64), fs, rate * scale, quality="VHQ")
        if emph:                                    # the linear interpolator's droop, undone (+6 dB at most)
            n = len(y)
            f = np.fft.rfftfreq(n, 1.0) if n > 1 else np.zeros(1)   # cycles per sample: f / rate
            g = np.minimum(1.0 / np.maximum(np.sinc(f) ** 2, 1e-3), 2.0)
            y = np.fft.irfft(np.fft.rfft(y) * g, n)
        a, b = int(fade[0] * rate), int(fade[1] * rate)
        if a > 1:
            y[:a] *= np.linspace(0.0, 1.0, a)
        if b > 1:
            y[-b:] *= np.linspace(1.0, 0.0, b)
        pick = list(note) if isinstance(note, (list, tuple)) else None
        if pick:
            note = -1
        z = dict(name=name, pcm=y, k=k, root=note, lo=note if lo is None else lo, hi=note if hi is None else hi,
                 group=group, slot=slot, kit=kit, looped=loop is not None, ls=0, le=len(y) - 1, sync=sync)
        if pick:
            z["pick"] = pick
        if loop is not None:
            z["ls"] = int(round(loop[0] * rate * scale))
            z["le"] = min(len(y) - 1, int(round(loop[1] * rate * scale)) - 1)
        if info:
            z.update(info)
        self.zones.append(z)
        return z

    def build(self, outdir, extra=None):
        import numpy as np
        import soundfile as sf
        outdir = Path(outdir)
        (outdir / "zones").mkdir(parents=True, exist_ok=True)
        xq = xq_path()
        gains = {}
        for z in self.zones:                        # each group's gain: its loudest peak to the engine's full level
            pk = float(np.abs(z["pcm"]).max()) + 1e-12
            gains[z["group"]] = min(gains.get(z["group"], 1e9), (KIT_PEAK if z["kit"] else SMP_PEAK) / pk)
        for z in self.zones:
            y = z.pop("pcm") * gains[z["group"]]
            n = len(y)
            x16 = np.clip(np.round(y), -32768, 32767).astype("<i2")
            fr = 256                                # the error's weighting: the sound's own tilt, frame by frame
            c = np.zeros(n, "<f4")
            for i in range(0, n, fr):
                s = y[max(0, i - fr // 2):i + fr + fr // 2]
                r0 = float(np.dot(s, s))
                r1 = float(np.dot(s[:-1], s[1:])) if len(s) > 1 else 0.0
                c[i:i + fr] = self.gamma * max(0.0, min(0.95, r1 / r0 if r0 > 0 else 0.0))
            dec = outdir / "zones" / "dec.raw"
            r = subprocess.run([str(xq), str(self.beam), str(dec)], input=struct.pack("<i", n) + x16.tobytes() + c.tobytes(),
                               capture_output=True, check=True)
            z["codes"] = r.stdout
            z["n"] = n
            z["snr_db"] = float(r.stderr.decode().strip())
            d = np.frombuffer(dec.read_bytes(), "<i2")
            z["peak"] = int(np.abs(d).max())
            z["pred"], z["idx"] = 0, 0
            if z["looped"]:                         # the decoder's state where the loop starts
                _o, st = ima_decode(z["codes"], z["ls"] + 1)
                z["pred"], z["idx"] = st[z["ls"]]
            sf.write(outdir / "zones" / f"{z['name']}.wav", d, int(round(z["k"] * UNIT)), subtype="PCM_16")
            dec.unlink()
        man, used = write_pack(self.name, self.zones, outdir, extra)
        tot = sum(used)
        cap = sum(s - SLOT_DATA for s in SLOT_SIZE)
        print(f"pack {self.name}: {len(self.zones)} zones, {tot} B of {cap} ({100.0 * tot / cap:.1f} %), "
              f"{sum(z['n'] / (z['k'] * UNIT) for z in self.zones):.1f} s of sound")
        by = {}
        for z in self.zones:
            g = by.setdefault(z["group"], [0, 0.0, 0.0, 0])
            g[0] += (z["n"] + 1) // 2
            g[1] += z["n"] / (z["k"] * UNIT)
            g[2] += z["snr_db"] * z["n"]
            g[3] += z["n"]
        for g, (b, s, q, n) in by.items():
            print(f"  {g:8s} {b:7d} B {100.0 * b / cap:5.1f} %  {s:6.2f} s  SNR {q / n:5.1f} dB  gain {20 * math.log10(gains[g]):+.1f} dB")
        for k, sl in enumerate(man["slots"]):
            print(f"  USR{k + 1}: {sl['zones']:2d} zones, {sl['bytes']:6d} B, {sl['room']:6d} free")
        return man


def stand_in(manifest, outdir):
    """the layout of a real pack (its manifest) filled with tones: each zone a decaying tone of its own pitch, as long
    as the real one. For the tests: the song's logic (the pack found, the cues, the kits) without a record's audio"""
    sys.path.insert(0, str(SRC / "tools"))
    from sampleio import ima_encode
    man = json.loads(Path(manifest).read_text())
    zones = []
    for i, m in enumerate(man["zones"]):
        n, rate = m["n"], m["k"] * UNIT
        f = 110.0 * 2.0 ** ((i % 24) / 12.0)
        tau = max(0.05, n / rate / 3.0)
        s = [int(9000.0 * math.sin(2 * math.pi * f * t / rate) * (math.exp(-t / rate / tau) if not m["looped"] else 1.0)
                 * min(1.0, t / 32.0)) for t in range(n)]
        codes, st = ima_encode(s, m["ls"])
        z = {k: m[k] for k in ("name", "k", "root", "lo", "hi", "ls", "le", "looped", "group", "kit", "sync", "n")}
        z.update(slot=m["slot"], codes=bytes(codes), pred=st[0] if m["looped"] else 0, idx=st[1] if m["looped"] else 0)
        zones.append(z)
    write_pack(man["name"], zones, outdir, {k: v for k, v in man.items() if k not in ("slots", "zones", "name", "unit_hz")})
    print(f"stand-in pack {man['name']}: {len(zones)} zones of tones in the real pack's layout")


def main(argv):
    sys.path.insert(0, str(SRC / "tools"))
    if argv and argv[0] == "--stand-in":
        stand_in(argv[1], argv[2])
        return
    if len(argv) < 2:
        raise SystemExit(__doc__)
    import importlib.util
    spec = importlib.util.spec_from_file_location("recipe", argv[0])
    rec = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(rec)
    rec.pack(argv[1], argv[2] if len(argv) > 2 else ".")


if __name__ == "__main__":
    main(sys.argv[1:])
