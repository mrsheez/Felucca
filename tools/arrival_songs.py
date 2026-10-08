# SPDX-License-Identifier: GPL-3.0-only
"""EDDA OS: THE ARRIVAL, the music (tools/gen_arrival.py encodes it for firmware/src/arrival.c).

Thirteen songs in album order. Every one keeps the EDDA rules: everything on the grid (no swing), kick and bass hard and
forward, the high percussion low, no rolls (a transition is a subtraction: the shakers go, the keys close, the log drum
runs alone, a beat of silence, the drop), an Igbo voice in every track. The tracks: T1 the log drum (I Dey Waka: the
808), T2 the keys, T3 the lead (or the pad), T4 THE ARRIVAL's kit (DRUM KIT EDDA: real shakers, cabasa, claps,
cross-stick, snare, congas, agogo as the ogene's two bells, ekwe, claves; a designed kick, udu and glass bottle).

Notation: notes as names (C4 = 60); a melodic part is (step, notes, steps held[, options]) on 16ths (mel); a drum
part is a row of steps per lane (drum: K kick, S snare, C clap, H shaker, O cabasa, T conga, R cross-stick, B ogene
high; the pads L low conga, E ekwe, Q high conga, U udu, W claves, G bottle, Z low shaker, A ogene low), '.' rest,
'o' ghost, 'x' hit, 'X' accent, '1'..'9' a level.
"""
from gen_arrival import (KIT_EDDA, SET_FLUTE, SET_LOG, SET_PIANO, SET_SAX, cat, drum, mel, nn, rep, sound)

V_POLY, V_MONO, V_LEGATO = 0, 1, 2
MIN, MAJ = 2, 1                     # P_SCALE
SL_GATE = 1                         # P_SLCR
ROOTS = {"C": 0, "C#": 1, "D": 2, "Eb": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "Bb": 10, "B": 11}


# ------------------------------------------------------------------ sounds --- #
def log_drum(root, drv=18, level=86, glide=10, rev=6, cut=127, rel=70, scale=MIN):
    """T1: the log drum (SAMPLE SET LOG), LEGATO with glide for the slides, its tail left to ring"""
    return sound("SAMPLE", "PIANO", role="log", E0=SET_LOG, E1=0, E2=0, E3=0, E4=cut, E6=drv, ATK=0, DEC=127, SUS=127,
                 REL=rel, VOICE=V_LEGATO, GLIDE=glide, LEVEL=level, DIST=0, CHOR=0, DLY=0, REV=rev, ROOT=root,
                 SCALE=scale, QUANT=1)


def kit(level=86, shaker=78, cabasa=68, clap=96, rim=92, conga=84, bell=66, kick=127, snare=92, rev=14, tone=64, decy=64):
    """T4: THE ARRIVAL's kit; the lanes' levels set the balance (the shakers, the bells low: the EDDA rule)"""
    return sound("DRUM", "DRUM KIT", role="kit", E0=KIT_EDDA, E1=64, E2=tone, E3=decy, E4=64, E5=100, E6=0, E7=0,
                 LEVEL=level, LN0=kick, LN1=snare, LN2=clap, LN3=shaker, LN4=cabasa, LN5=conga, LN6=rim, LN7=bell,
                 DIST=0, CHOR=0, DLY=0, REV=rev)


def guitar(root, scale=MIN, role="keys", damp=84, brit=72, level=104, pan=-12, rev=24, dly=12, cho=14, gate=64,
           dist=34, **kw):
    """the highlife guitar: PHYS's string (an extended Karplus-Strong pluck); DAMP down: the palm-muted stab. DIST: the
    amp (the track's soft clip: the pick's spike rounded off, the note's body up)"""
    return sound("PHYS", "PLUCK", role=role, E2=brit, E3=damp, LEVEL=level, PAN=pan, DIST=dist, CHOR=cho, DLY=dly,
                 REV=rev, ROOT=root, SCALE=scale, QUANT=1, SGATE=gate, **kw)


def piano(root, scale=MIN, role="keys", level=100, pan=0, rev=34, dly=0, cho=0, gate=96, rel=40, **kw):
    """the grand piano (SAMPLE PIANO: VCSL's Kawai)"""
    return sound("SAMPLE", "PIANO", role=role, E0=SET_PIANO, E3=1, E4=120, ATK=0, DEC=104, SUS=0, REL=rel, LEVEL=level,
                 PAN=pan, CHOR=cho, DLY=dly, REV=rev, ROOT=root, SCALE=scale, QUANT=1, SGATE=gate, **kw)


def rhodes(root, scale=MIN, role="keys", level=100, pan=6, rev=30, dly=10, cho=30, dist=30, **kw):
    """the electric piano: FM6 EDDA EP (TINE EP with a short release) through the track's drive (DIST: the amp's bark,
    the second and third harmonics up 10 dB, as a Rhodes played into its suitcase amp)"""
    return sound("FM6", "EDDA EP", role=role, LEVEL=level, PAN=pan, DIST=dist, CHOR=cho, DLY=dly, REV=rev, ROOT=root,
                 SCALE=scale, QUANT=1, **kw)


def sax(root, scale=MIN, level=96, pan=8, rev=40, dly=24, glide=4, **kw):
    """the tenor sax (SAMPLE SAX: VCSL), LEGATO"""
    return sound("SAMPLE", "SAX", role="lead", E0=SET_SAX, E4=118, ATK=6, DEC=80, SUS=110, REL=40, VOICE=V_LEGATO,
                 GLIDE=glide, LEVEL=level, PAN=pan, CHOR=10, DLY=dly, REV=rev, ROOT=root, SCALE=scale, QUANT=1, **kw)


def flute(root, scale=MIN, level=96, pan=-6, rev=46, dly=30, glide=6, **kw):
    """the flute as the oja (SAMPLE FLUTE: VSCO-2), LEGATO"""
    return sound("SAMPLE", "FLUTE", role="lead", E0=SET_FLUTE, E4=124, ATK=10, DEC=80, SUS=118, REL=46,
                 VOICE=V_LEGATO, GLIDE=glide, LEVEL=level, PAN=pan, CHOR=8, DLY=dly, REV=rev, ROOT=root, SCALE=scale,
                 QUANT=1, **kw)


# ------------------------------------------------------------------ helpers --- #
def steps(row):
    """the steps of a row ('..x. ..x.'): where it has a mark"""
    row = row.replace(" ", "").replace("|", "")
    return [i for i, c in enumerate(row) if c != "."]


def stabs(chords, row, held=1, vel=96, opts=""):
    """chords (one per bar, note names) struck on a bar's row of steps, each held `held` steps; 'X' in the row:
    accented"""
    r = row.replace(" ", "").replace("|", "")
    ev = []
    for b, ch in enumerate(chords):
        for i, c in enumerate(r):
            if c == ".":
                continue
            o = opts + ("A" if c == "X" else "") + ("v70" if c == "o" else "")
            ev.append((b * 16 + i, ch, held, o))
    return mel(16 * len(chords), ev, vel=vel)


def line(length, events, vel=100):
    """a melodic line: events (step, note, held[, options])"""
    return mel(length, events, vel=vel)


SCALES = {MIN: [0, 2, 3, 5, 7, 8, 10], MAJ: [0, 2, 4, 5, 7, 9, 11], "harm": [0, 2, 3, 5, 7, 8, 11]}


def deg(note, steps, key):
    """note moved `steps` degrees along the key's scale (key: (root pitch class, MIN / MAJ / "harm"); 7 an octave)"""
    root, mode = key
    offs = SCALES[mode]
    rel = (note - root) % 12
    if rel not in offs:
        raise ValueError(f"{note} is not in the key {key}")
    octv, k = divmod(offs.index(rel) + steps, 7)
    return note - rel + offs[k] + 12 * octv


def log_bars(roots, motif, key, vel=112, legato=True):
    """a log drum line: for each bar's root (note name), the motif (step, scale degrees above the root (7 an octave,
    -3 the fifth below), held[, options]): always in the key. legato: each note held (tied) up to the next, so the log
    drum is never a released voice between its notes (the shared budget gives up released voices first: the bass must
    not be one); its sample decays as it would anyway"""
    ev = []
    for b, r in enumerate(roots):
        base = nn(r)
        for m in motif:
            s, iv, h = m[0], m[1], m[2]
            ev.append([b * 16 + s, deg(base, iv, key), h, m[3] if len(m) > 3 else ""])
    n = 16 * len(roots)
    if legato:
        for i, e in enumerate(ev):
            nxt = ev[i + 1][0] if i + 1 < len(ev) else n
            e[2] = max(e[2], nxt - e[0])
    return mel(n, [tuple(e) for e in ev], vel=vel)


def silence_last_beat(pat):
    """a pattern's last beat (four steps) emptied: the beat of silence before the drop"""
    out = list(pat)
    for i in range(len(out) - 4, len(out)):
        out[i] = None
    return out


# common grooves (16 steps)
SH_AND = "oxXo oxXo oxXo oxXo"      # shakers: the accent on the and
SH_BEAT = "Xoxo Xoxo Xoxo Xoxo"     # .. on the beat
SH_A = "xoxX xoxX xoxX xoxX"        # .. on the a (the bounce)
K_3STEP = "x... ..x. .... x..."     # 1, the and of 2, 4: the 3-step / tresillo kick
K_AMAP = "x... .... x... ...."      # amapiano: 1 and 3, the log drum between
K_SGIJA = "x..x ..x. ..x. x..."     # sgija: syncopated
K_4 = "x... x... x... x..."         # four on the floor (Do Am Again, Turn Am Up: in the drops only)




def stab_part(chords, row, held=1, vel=96):
    return stabs(chords, row, held=held, vel=vel)


# ================================================================ 1 UP NEPA === #
# A minor, 116. The call: the light comes back, the room shouts, the first drop is a white-out. F - C - Am - G (VI III i
# VII), euphoric and defiant. Muted guitar stabs, the sax riff as the hook, congas and sticks, the log drum returning
# under the hook. Opens on "Any moment now."
def up_nepa():
    A, K = 9, (9, MIN)
    roots = ["F1", "C2", "A1", "G1"]
    log = log_bars(roots, [(0, 0, 3, "A"), (3, 0, 2), (6, 4, 3), (10, 0, 2), (12, 2, 2, "S"), (14, 4, 2)], K)
    run = silence_last_beat(log_bars(["A1", "G1"], [(0, 0, 3, "A"), (3, 0, 2), (6, 4, 2), (8, 7, 2, "S"), (10, 4, 2),
                                                    (12, 0, 2)], K, vel=118))
    ch = ["A3 C4 F4", "G3 C4 E4", "A3 C4 E4", "G3 B3 D4"]
    gtr = stabs(ch, "..x. .x.o ..x. .x.o", vel=100)
    gtr_open = stabs(ch, "x.x. .x.x ..x. .x.x", vel=104)
    sax_riff = line(64, [
        (0, "A4", 2), (2, "C5", 2), (4, "A4", 1), (5, "G4", 1), (6, "A4", 4), (10, "C5", 2), (12, "D5", 2), (14, "C5", 2),
        (16, "E5", 3, "A"), (19, "D5", 1), (20, "C5", 2), (22, "A4", 6),
        (32, "A4", 2), (34, "C5", 2), (36, "A4", 1), (37, "G4", 1), (38, "A4", 4), (42, "E5", 2), (44, "D5", 2), (46, "C5", 2),
        (48, "B4", 3), (51, "A4", 1), (52, "G4", 6), (60, "E4", 1), (61, "G4", 1), (62, "A4", 2)], vel=108)
    intro = drum(16, {"H": SH_AND, "T": "..x. ..x. .x.. ..x.", "L": "x... .... ..x. ....", "W": "x..x ..x. ..x. x..."})
    groove = drum(16, {"K": K_3STEP, "H": SH_AND, "R": ".... x..x .... x...", "T": "..x. .... .x.. ..x.",
                       "L": "x... .... ..x. ...."})
    drop = drum(16, {"K": K_3STEP, "H": SH_AND, "C": ".... x... .... x...", "R": "...x ...x .... ..x.",
                     "T": "..x. .... .x.. ..x.", "B": "..x. ..x. ..x. x.x.", "O": ".... .... .... ..x."})
    drop4 = drum(16, {"K": "x... ..x. .... ....", "H": SH_AND, "C": ".... x... .... x...", "R": "...x ...x .... ....",
                      "T": "..x. .... .x.x .x.x", "B": "..x. ..x. ..x. x.x.", "O": ".... .... .... x.x."})
    run_k = drum(32, {"K": "x... ..x. .... x... x... ..x. .... ...."})
    return dict(
        name="UP NEPA", mix=[89, 117, 81, 87], chant="IHE", line="ANY MOMENT NOW.", bpm=116, cam="8A",
        tracks=[log_drum(A, drv=20),
                guitar(A, damp=52, brit=60, level=110, pan=-14, gate=40),
                sax(A),
                kit(shaker=76, conga=80, rim=94, bell=60)],
        globals=dict(DTIME=11, DFDBK=38, DCOLOR=60, DMIX=60, RSIZE=86, RDAMP=70, RTYPE=0),
        sections=[
            ("INTRO", dict(tracks=[[], gtr, [], rep(intro, 4)], motion=[(1, 0, "ED_FX", -36)])),
            ("GROOVE", dict(tracks=[log, gtr, [], rep(groove, 4)])),
            ("DROP", dict(tracks=[log, gtr_open, sax_riff, cat(drop, drop, drop, drop4)])),
            ("RUN", dict(tracks=[run, [], [], run_k])),
        ],
        arrangement=[("INTRO", 2), ("GROOVE", 4), ("RUN", 1), ("DROP", 4), ("INTRO", 2), ("GROOVE", 2), ("RUN", 1),
                     ("DROP", 4), ("GROOVE", 2), ("DROP", 2), ("INTRO", 2)],
    )


# ============================================================== 2 WE OUTSIDE === #
# G minor, 115. The state of being: out into the night, one more. A two-chord vamp, Gm9 - Cm9 (i - iv), Rhodes stabs,
# the log drum tuned to the fifth, a string drone under everything, udu and ekwe in conversation. Opens on "One more."
def we_outside():
    G, K = 7, (7, MIN)
    log = log_bars(["D2", "D2", "C2", "D2"], [(0, 0, 3, "A"), (3, 0, 2), (6, -4, 3), (10, 0, 2), (12, 3, 2, "S"),
                                              (14, 0, 2)], K)
    log2 = log_bars(["D2", "D2", "C2", "C2"], [(0, 0, 2, "A"), (2, 0, 2), (6, -4, 2), (9, 0, 2), (11, -1, 2, "S"),
                                               (13, 0, 3)], K)
    run = silence_last_beat(log_bars(["D2", "G1"], [(0, 0, 3, "A"), (3, 0, 3), (6, 0, 2, "S"), (8, 2, 2), (11, 0, 2),
                                                    (13, -3, 2)], K, vel=118))
    ch = ["Bb3 F4 A4", "Bb3 F4 A4", "Eb4 Bb4 D5", "Eb4 Bb4 D5"]
    rh = stabs(ch, "..x. ..x. ...x ..x.", held=1, vel=108)
    rh_drop = stabs(ch, "..x. .xx. ...x ..x.", held=1, vel=114)
    drone = line(64, [(0, "G2 D3", 64)], vel=96)
    intro = drum(16, {"H": SH_BEAT, "E": "x... ..x. ..x. ....", "U": ".... .... .... x..."})
    groove = drum(16, {"K": K_SGIJA, "H": SH_BEAT, "R": ".... x... .... x...", "E": "x... ..x. ..x. ....",
                       "U": "...x .... .x.. ...."})
    drop = drum(16, {"K": K_SGIJA, "H": SH_BEAT, "C": ".... x... .... x...", "R": "..x. ...x .... ..x.",
                     "E": "x... ..x. ..x. ....", "U": "...x .... .x.. x...", "O": ".... .... .... ..x."})
    drop4 = drum(16, {"K": "x..x ..x. ..x. ....", "H": SH_BEAT, "C": ".... x... .... x...", "R": "..x. ...x .... ....",
                      "E": "x... ..x. ..x. x.x.", "U": "...x .... .x.. ....", "O": ".... .... .... x..."})
    run_k = drum(32, {"K": "x..x ..x. ..x. x... x..x ..x. .... ...."})
    return dict(
        name="WE OUTSIDE", mix=[88, 89, 68, 83], chant="ABALI", line="ONE MORE.", bpm=115, cam="6A",
        tracks=[log_drum(G, drv=22),
                rhodes(G),
                sound("ANALOG", "STRINGS", role="pad", ATK=70, REL=80, LEVEL=96, PAN=0, CHOR=40, REV=44, ROOT=G, SCALE=MIN,
                      QUANT=1, SGATE=127),
                kit(shaker=74, rim=92, conga=88)],
        globals=dict(DTIME=11, DFDBK=44, DCOLOR=56, DMIX=56, RSIZE=92, RDAMP=64, RTYPE=0),
        sections=[
            ("INTRO", dict(tracks=[[], rh, drone, rep(intro, 4)], motion=[(1, 0, "ED_FX", -30)])),
            ("GROOVE", dict(tracks=[log, rh, drone, rep(groove, 4)])),
            ("DROP", dict(tracks=[log2, rh_drop, drone, cat(drop, drop, drop, drop4)])),
            ("RUN", dict(tracks=[run, [], [], run_k])),
        ],
        arrangement=[("INTRO", 2), ("GROOVE", 4), ("RUN", 1), ("DROP", 4), ("INTRO", 2), ("GROOVE", 2), ("RUN", 1),
                     ("DROP", 4), ("GROOVE", 2), ("DROP", 2), ("INTRO", 2)],
    )


# ================================================================= 3 OYA COME === #
# D minor, 113. The welcome. Dm - Bb - F - C (i VI III VII), warm. Skeletal piano stabs, a dusty ghostly pad (grains of
# the flute), the log drum, the ogene's two bells calling everyone in. Opens on "Oya, come."
def oya_come():
    D, K = 2, (2, MIN)
    log = log_bars(["D2", "Bb1", "F1", "C2"], [(0, 0, 3, "A"), (4, 0, 2), (7, 4, 2), (10, 0, 2), (13, 7, 2, "S"),
                                               (15, 4, 1)], K)
    run = silence_last_beat(log_bars(["D2", "C2"], [(0, 0, 3, "A"), (4, 0, 2), (6, 4, 2, "S"), (9, 3, 2), (12, 0, 2)],
                                     K, vel=118))
    ch = ["D4 F4 A4", "D4 F4 Bb4", "C4 F4 A4", "C4 E4 G4"]
    pno = stabs(ch, "..x. .x.. x.x. ..x.", held=1, vel=92)
    pno_drop = stabs(ch, "x.x. .x.x ..x. .xx.", held=1, vel=98)
    pad = mel(64, [(0, "D3 A3", 16), (16, "Bb2 F3", 16), (32, "F3 C4", 16), (48, "C3 G3", 16)], vel=100)
    intro = drum(16, {"H": SH_A, "B": "x..x ..x. ..x. ....", "A": ".... .... .... x.x."})
    groove = drum(16, {"K": K_AMAP, "H": SH_A, "C": ".... x... .... x...", "R": "..x. ...x .x.. ....",
                       "B": "x..x ..x. ..x. ...."})
    drop = drum(16, {"K": "x... ..x. x... ....", "H": SH_A, "C": ".... x... .... x...", "R": "..x. ...x .x.. ..x.",
                     "B": "x..x ..x. ..x. ....", "A": ".... .... .... x.x.", "T": ".x.. .... ...x ...."})
    drop4 = drum(16, {"K": "x... ..x. x... ....", "H": SH_A, "C": ".... x... .... x...", "R": "..x. ...x .x.. ....",
                      "B": "x..x ..x. ..x. ....", "A": ".... .... .... x.x.", "T": ".x.. .... .x.x .x.x"})
    run_k = drum(32, {"K": "x... ..x. x... .... x... ..x. .... ...."})
    return dict(
        name="OYA COME", mix=[89, 76, 75, 87], chant="UZO", line="OYA, COME.", bpm=113, cam="7A",
        tracks=[log_drum(D, drv=16),
                piano(D, level=100),
                sound("GRAIN", "CLOUD PAD", role="pad", E0=SET_FLUTE, ATK=60, REL=70, LEVEL=96, PAN=0, CHOR=30, REV=50,
                      ROOT=D, SCALE=MIN, QUANT=1, SGATE=127),
                kit(shaker=76, bell=62, clap=92)],
        globals=dict(DTIME=11, DFDBK=40, DCOLOR=60, DMIX=56, RSIZE=90, RDAMP=60, RTYPE=0),
        sections=[
            ("INTRO", dict(tracks=[[], pno, pad, rep(intro, 4)], motion=[(1, 0, "ED_FX", -28)])),
            ("GROOVE", dict(tracks=[log, pno, pad, rep(groove, 4)])),
            ("DROP", dict(tracks=[log, pno_drop, pad, cat(drop, drop, drop, drop4)])),
            ("RUN", dict(tracks=[run, [], [], run_k])),
        ],
        arrangement=[("INTRO", 2), ("GROOVE", 4), ("RUN", 1), ("DROP", 4), ("INTRO", 2), ("GROOVE", 2), ("RUN", 1),
                     ("DROP", 4), ("GROOVE", 2), ("DROP", 2), ("INTRO", 2)],
    )


# ========================================================== 4 WHERE YOU SLEEP === #
# G# minor, 112. The threshold: a night somewhere new, answerable to no one. A two-chord vamp, G#m9 - Emaj7 (i - VI),
# a low organ, the oja flute riff, the log drum, a glass bottle and the ekwe in the dark. Opens on "The night still
# young."
def where_you_sleep():
    Gs, K = 8, (8, MIN)
    log = log_bars(["G#1", "G#1", "E1", "E1"], [(0, 0, 3, "A"), (3, 0, 2), (6, 7, 2, "S"), (8, 4, 2), (11, 0, 2),
                                                (14, 4, 2)], K)
    run = silence_last_beat(log_bars(["G#1", "F#1"], [(0, 0, 3, "A"), (3, 0, 2), (6, 4, 2, "S"), (8, 7, 2), (11, 4, 2),
                                                      (13, 0, 2)], K, vel=118))
    org = mel(64, [(0, "B3 D#4 F#4", 14), (16, "B3 D#4 F#4", 14), (32, "B3 E4 G#4", 14), (48, "B3 E4 G#4", 14)],
              vel=88)
    org_stab = stabs(["B3 D#4 F#4", "B3 D#4 F#4", "B3 E4 G#4", "B3 E4 G#4"], "..x. ..x. ..x. .x..", held=2, vel=92)
    riff = line(64, [
        (0, "D#5", 3, "A"), (3, "B4", 1), (4, "C#5", 2), (6, "G#4", 4), (12, "F#4", 2), (14, "G#4", 2),
        (16, "B4", 6), (24, "C#5", 2), (26, "B4", 2), (28, "G#4", 4),
        (32, "D#5", 3, "A"), (35, "B4", 1), (36, "C#5", 2), (38, "G#4", 4), (44, "F#5", 2), (46, "D#5", 2),
        (48, "E5", 4), (52, "D#5", 2), (54, "B4", 2), (56, "C#5", 4), (60, "G#4", 4)], vel=104)
    intro = drum(16, {"H": SH_AND, "G": "..x. .... ..x. ....", "E": "x... .x.. .... ..x."})
    groove = drum(16, {"K": K_3STEP, "H": SH_AND, "R": ".... x... .... x...", "G": "..x. .... ..x. ....",
                       "E": "x... .x.. .... ..x."})
    drop = drum(16, {"K": K_3STEP, "H": SH_AND, "R": ".... x..x .... x...", "C": ".... x... .... x...",
                     "G": "..x. .... ..x. ..x.", "E": "x... .x.. .... ..x.", "O": ".... .... .... x..."})
    drop4 = drum(16, {"K": "x... ..x. .... ....", "H": SH_AND, "R": ".... x..x .... ....", "C": ".... x... .... x...",
                      "G": "..x. .... ..x. ..x.", "E": "x... .x.. .x.x .x.x", "O": ".... .... .... x.x."})
    run_k = drum(32, {"K": "x... ..x. .... x... x... ..x. .... ...."})
    return dict(
        name="WHERE YOU SLEEP", mix=[90, 86, 80, 87], short="WHERE YOU", chant="NZUZO", line="THE NIGHT STILL YOUNG.", bpm=112, cam="1A",
        tracks=[log_drum(Gs, drv=18),
                sound("WHEEL", "SOFT FLUTE", role="keys", LEVEL=100, PAN=-6, CHOR=0, REV=40, ROOT=Gs, SCALE=MIN, QUANT=1),
                flute(Gs),
                kit(shaker=74, rim=90, conga=84)],
        globals=dict(DTIME=10, DFDBK=50, DCOLOR=50, DMIX=62, RSIZE=100, RDAMP=56, RTYPE=0),
        sections=[
            ("INTRO", dict(tracks=[[], org, riff, rep(intro, 4)], motion=[(1, 0, "ED_FX", -24)])),
            ("GROOVE", dict(tracks=[log, org_stab, [], rep(groove, 4)])),
            ("DROP", dict(tracks=[log, org_stab, riff, cat(drop, drop, drop, drop4)])),
            ("RUN", dict(tracks=[run, [], [], run_k])),
        ],
        arrangement=[("INTRO", 2), ("GROOVE", 4), ("RUN", 1), ("DROP", 4), ("INTRO", 2), ("GROOVE", 2), ("RUN", 1),
                     ("DROP", 4), ("GROOVE", 2), ("DROP", 2), ("INTRO", 2)],
    )


# ================================================================== 5 E GO BE === #
# C minor, 116. The mantra: it will be, it already is. Cm - Ab - Eb - Bb (i VI III VII, the major lift), piano chords,
# the highlife guitar riff as the hook, the log drum; it ends into the stop. Opens on "E go be."
def e_go_be():
    C, K = 0, (0, MIN)
    log = log_bars(["C2", "Ab1", "Eb2", "Bb1"], [(0, 0, 3, "A"), (3, 0, 2), (6, 4, 2), (9, 0, 2), (12, 6, 2, "S"),
                                                 (14, 7, 2)], K)
    run = silence_last_beat(log_bars(["C2", "Bb1"], [(0, 0, 3, "A"), (3, 0, 3), (6, 4, 2, "S"), (8, 6, 2), (11, 0, 2),
                                                     (13, 4, 2)], K, vel=118))
    ch = ["C4 Eb4 G4", "C4 Eb4 Ab4", "Bb3 Eb4 G4", "Bb3 D4 F4"]
    pno = stabs(ch, "x... ..x. ..x. ....", held=2, vel=94)
    pno_drop = stabs(ch, "x.x. ..x. ..x. .x..", held=1, vel=98)
    riff = line(64, [
        (0, "G4", 1), (1, "C5", 1), (2, "Eb5", 1), (3, "C5", 1), (4, "G4", 1), (6, "Bb4", 1), (7, "C5", 2),
        (10, "G4", 1), (11, "Bb4", 1), (12, "C5", 1), (14, "Eb5", 2),
        (16, "Ab4", 1), (17, "C5", 1), (18, "Eb5", 1), (19, "C5", 1), (20, "Ab4", 1), (22, "Bb4", 1), (23, "C5", 2),
        (26, "Ab4", 1), (27, "Bb4", 1), (28, "C5", 1), (30, "Eb5", 2),
        (32, "G4", 1), (33, "Bb4", 1), (34, "Eb5", 1), (35, "Bb4", 1), (36, "G4", 1), (38, "Bb4", 1), (39, "Eb5", 2),
        (42, "G5", 2), (44, "F5", 1), (45, "Eb5", 1), (46, "D5", 2),
        (48, "F4", 1), (49, "Bb4", 1), (50, "D5", 1), (51, "Bb4", 1), (52, "F4", 1), (54, "Ab4", 1), (55, "Bb4", 2),
        (58, "C5", 2), (60, "D5", 2), (62, "Eb5", 2)], vel=118)
    intro = drum(16, {"H": SH_BEAT, "R": ".... x... .... x...", "T": "...x .... ..x. ...."})
    groove = drum(16, {"K": K_3STEP, "H": SH_BEAT, "R": ".... x... .... x...", "T": "...x .... ..x. ....",
                       "L": "x... .... .... ..x."})
    drop = drum(16, {"K": K_3STEP, "H": SH_BEAT, "C": ".... x... .... x...", "R": "..x. ...x .... ..x.",
                     "T": "...x .... ..x. ....", "L": "x... .... .... ..x.", "B": "x.x. .x.x .x.x .x.."})
    drop4 = drum(16, {"K": "x... ..x. .... ....", "H": SH_BEAT, "C": ".... x... .... x...", "R": "..x. ...x .... ....",
                      "T": "...x .... ..x. x.x.", "L": "x... .... .... ....", "B": "x.x. .x.x .x.x .x.."})
    run_k = drum(32, {"K": "x... ..x. .... x... x... ..x. .... ...."})
    return dict(
        name="E GO BE", mix=[87, 79, 107, 87], chant="NDIDI", line="E GO BE.", bpm=116, cam="5A",
        tracks=[log_drum(C, drv=20),
                piano(C, level=98),
                guitar(C, role="lead", damp=72, brit=78, level=106, pan=10, gate=50),
                kit(shaker=76, conga=86, bell=60)],
        globals=dict(DTIME=11, DFDBK=36, DCOLOR=64, DMIX=54, RSIZE=86, RDAMP=66, RTYPE=0),
        sections=[
            ("INTRO", dict(tracks=[[], pno, riff, rep(intro, 4)], motion=[(2, 0, "ED_FX", -20)])),
            ("GROOVE", dict(tracks=[log, pno, [], rep(groove, 4)])),
            ("DROP", dict(tracks=[log, pno_drop, riff, cat(drop, drop, drop, drop4)])),
            ("RUN", dict(tracks=[run, [], [], run_k])),
        ],
        arrangement=[("INTRO", 2), ("GROOVE", 4), ("RUN", 1), ("DROP", 4), ("INTRO", 2), ("GROOVE", 2), ("RUN", 1),
                     ("DROP", 4), ("GROOVE", 2), ("RUN", 1), ("DROP", 4)],
    )


# ================================================================ 6 I DEY WAKA === #
# B-flat minor, 139 half-time: the one trap record, the cold cut, framed by silence. Bbm - Gb - Db - Ab (i VI III VII),
# the oja flute riff first (with the ogene's two bells), then the drums land: a hard distorted 808 with long glides, a
# cracking half-time clap, the shaker as the hats with triplet bursts, the udu thumping into the 808, a pad of grains.
# Opens on the flute, then "Nwayo... nwayo..."
def i_dey_waka():
    Bb = 10
    eight = line(64, [
        (0, "Bb1", 6, "A"), (6, "Bb1", 2), (8, "Bb1", 4, "S"), (12, "Db2", 4),
        (16, "Gb1", 8, "A"), (24, "Gb1", 2), (26, "Ab1", 6, "S"),
        (32, "Db2", 6, "A"), (38, "Db2", 2), (40, "Db2", 4, "S"), (44, "F2", 4),
        (48, "Ab1", 8, "A"), (56, "Ab1", 2), (58, "Bb1", 6, "S")], vel=120)
    eight_v = line(64, [(0, "Bb1", 16, "A"), (16, "Gb1", 16, "A"), (32, "Db2", 16, "A"), (48, "Ab1", 8, "AS"),
                        (56, "Bb1", 8)], vel=116)
    pad = mel(64, [(0, "Bb3 Db4 F4", 16), (16, "Bb3 Db4 Gb4", 16), (32, "Ab3 Db4 F4", 16), (48, "Ab3 C4 Eb4", 16)],
              vel=96)
    riff = line(64, [
        (0, "F5", 3, "A"), (3, "Db5", 1), (4, "Eb5", 2), (6, "Bb4", 6), (12, "Ab4", 2), (14, "Bb4", 2),
        (16, "Db5", 4), (20, "Bb4", 2), (22, "Ab4", 2), (24, "Gb4", 8),
        (32, "F5", 3, "A"), (35, "Db5", 1), (36, "Eb5", 2), (38, "Bb4", 4), (42, "Db5", 2), (44, "Eb5", 2), (46, "F5", 2),
        (48, "Ab5", 4), (52, "Gb5", 2), (54, "F5", 2), (56, "Eb5", 4), (60, "Db5", 2), (62, "Bb4", 2)], vel=100)
    bell = drum(16, {"B": "x... ..x. .... x...", "A": "..x. .... x... ..x."})
    hook = drum(16, {"K": "x... .... ..x. ...x", "C": ".... .... x... ....", "H": "x.x. x.xx x.x. x.x.",
                     "B": "x... ..x. .... x...", "A": "..x. .... x... ..x.", "U": ".... ...x .... ...."},
                ratch={5: 3, 13: 3})
    hook4 = drum(16, {"K": "x... .... ..x. ....", "C": ".... .... x... ....", "H": "x.x. x.x. x.xx x.x.",
                      "B": "x... ..x. .... x...", "A": "..x. .... x... ..x.", "U": ".... ...x .... ...x"},
                 ratch={10: 3, 11: 3, 14: 2})
    verse = drum(16, {"K": "x... .... ..x. ....", "H": "x.x. x.x. x.x. x.x.", "B": "x... ..x. .... x...",
                      "A": "..x. .... x... ..x."})
    return dict(
        name="I DEY WAKA", mix=[85, 71, 80, 85], chant="NWAYO", line="NWAYO... NWAYO...", bpm=139, cam="3A", half=True,
        tracks=[sound("ANALOG", "SUB BASS", role="bass", E0=3, E4=70, E5=10, E6=60, ATK=0, DEC=110, SUS=90, REL=60,
                      ED_PIT=10, VOICE=V_LEGATO, GLIDE=40, GLMODE=1, LEVEL=96, DIST=24, REV=0, ROOT=Bb, SCALE=MIN, QUANT=1),
                sound("GRAIN", "CLOUD PAD", role="pad", E0=SET_FLUTE, ATK=70, REL=80, LEVEL=96, CHOR=24, REV=56, ROOT=Bb,
                      SCALE=MIN, QUANT=1, SGATE=127),
                flute(Bb, level=98, rev=50, dly=36),
                kit(shaker=72, bell=70, clap=104, rev=10)],
        globals=dict(DTIME=11, DFDBK=52, DCOLOR=48, DMIX=62, RSIZE=104, RDAMP=50, RTYPE=0),
        sections=[
            ("INTRO", dict(tracks=[[], pad, riff, rep(bell, 4)])),
            ("HOOK", dict(tracks=[eight, pad, riff, cat(hook, hook, hook, hook4)])),
            ("VERSE", dict(tracks=[eight_v, [], [], rep(verse, 4)])),
            ("BRIDGE", dict(tracks=[[], pad, [], []], length=64)),
        ],
        arrangement=[("INTRO", 2), ("HOOK", 4), ("VERSE", 4), ("HOOK", 4), ("VERSE", 2), ("BRIDGE", 1), ("HOOK", 6),
                     ("VERSE", 2), ("INTRO", 2)],
    )


# ================================================================= 7 BODY KNOW === #
# G minor, 112. The body takes the room: stop thinking. Gm - Eb - F (i VI VII, the third chord held), the Pretoria
# bacardi bounce: a busy, syncopated kick, the log drum in the gaps, organ stabs, the sax riff high. Opens on "Stop
# thinking."
def body_know():
    G, K = 7, (7, MIN)
    log = log_bars(["G1", "G1", "Eb2", "F1"], [(0, 0, 2, "A"), (3, 0, 2), (5, 7, 1), (7, 4, 2), (10, 0, 2, "S"),
                                               (12, 2, 2), (14, 0, 2)], K)
    run = silence_last_beat(log_bars(["G1", "F1"], [(0, 0, 2, "A"), (3, 0, 2), (5, 7, 2, "S"), (8, 4, 2), (10, 0, 2),
                                                    (12, 2, 2)], K, vel=118))
    ch = ["Bb3 D4 G4", "Bb3 D4 G4", "Bb3 Eb4 G4", "A3 C4 F4"]
    org = stabs(ch, "x..x ..x. .x.. x...", held=1, vel=98)
    riff = line(64, [
        (0, "D5", 2, "A"), (3, "D5", 1), (4, "F5", 2), (6, "D5", 2), (8, "C5", 2), (10, "Bb4", 2), (12, "G4", 4),
        (20, "G4", 1), (21, "Bb4", 1), (22, "C5", 2), (24, "D5", 4), (28, "C5", 2), (30, "Bb4", 2),
        (32, "D5", 2, "A"), (35, "D5", 1), (36, "F5", 2), (38, "D5", 2), (40, "G5", 4), (44, "F5", 2), (46, "D5", 2),
        (48, "C5", 3), (51, "Bb4", 1), (52, "A4", 4), (56, "F4", 2), (58, "G4", 6)], vel=106)
    intro = drum(16, {"H": SH_A, "R": "..x. ...x .x.. ..x.", "T": "x... .... x.x. ...."})
    groove = drum(16, {"K": "x..x ..x. x... x.x.", "H": SH_A, "R": "..x. ...x .x.. ..x.", "T": "x... .... x.x. ....",
                       "C": ".... x... .... x..."})
    drop = drum(16, {"K": "x..x ..x. x... x.x.", "H": SH_A, "C": ".... x... .... x...", "R": "..x. ...x .x.. ..x.",
                     "T": "x... .x.. x.x. ....", "L": "...x .... ...x ....", "O": ".... ..x. .... ..x."})
    drop4 = drum(16, {"K": "x..x ..x. x... ....", "H": SH_A, "C": ".... x... .... x...", "R": "..x. ...x .x.. ....",
                      "T": "x... .x.. x.x. x.x.", "L": "...x .... ...x .x.x", "O": ".... ..x. .... ...."})
    run_k = drum(32, {"K": "x..x ..x. x... x.x. x..x ..x. .... ...."})
    return dict(
        name="BODY KNOW", mix=[87, 95, 81, 81], chant="EGWU", line="STOP THINKING.", bpm=112, cam="6A",
        tracks=[log_drum(G, drv=24),
                sound("WHEEL", "JAZZ PERC", role="keys", LEVEL=98, PAN=-8, CHOR=0, REV=26, ROOT=G, SCALE=MIN, QUANT=1,
                      SGATE=40),
                sax(G, level=94),
                kit(shaker=76, conga=90, clap=98)],
        globals=dict(DTIME=11, DFDBK=40, DCOLOR=62, DMIX=54, RSIZE=84, RDAMP=70, RTYPE=0),
        sections=[
            ("INTRO", dict(tracks=[[], org, [], rep(intro, 4)], motion=[(1, 0, "ED_FX", -30)])),
            ("GROOVE", dict(tracks=[log, org, [], rep(groove, 4)])),
            ("DROP", dict(tracks=[log, org, riff, cat(drop, drop, drop, drop4)])),
            ("RUN", dict(tracks=[run, [], [], run_k])),
        ],
        arrangement=[("INTRO", 2), ("GROOVE", 4), ("RUN", 1), ("DROP", 4), ("INTRO", 2), ("GROOVE", 2), ("RUN", 1),
                     ("DROP", 4), ("GROOVE", 2), ("DROP", 2), ("INTRO", 1)],
    )


# =============================================================== 8 DO AM AGAIN === #
# D major, 125: the first house peak, the trial done again until it is who you are. Gmaj7 - A - F#m7 - Bm (IV V iii vi),
# a filtered highlife guitar loop pumping (the SLICER on the beat: the pulse from the loop), the log drum as the
# talking bass riff, emotional grand piano, four on the floor in the drops only; three breakdown, build, drop cycles,
# the build a filter opening and a bar of silence. Opens on "Do am again."
def do_am_again():
    D, K = 2, (2, MAJ)
    roots = ["G1", "A1", "F#1", "B1"]
    riffb = log_bars(roots, [(0, 0, 2, "A"), (3, 0, 1), (4, 7, 2, "S"), (6, 4, 2), (10, 0, 2), (12, 3, 1), (13, 4, 1),
                             (14, 7, 2, "S")], K, vel=114)
    sparse = log_bars(roots, [(0, 0, 4, "A"), (10, 4, 2), (14, 0, 2)], K, vel=108)
    pno = mel(64, [(0, "B3 D4 F#4", 6), (6, "B3 D4 F#4", 4), (10, "A3 D4 F#4", 6),
                   (16, "A3 C#4 E4", 6), (22, "A3 C#4 E4", 4), (26, "A3 C#4 E4", 6),
                   (32, "F#3 A3 C#4", 6), (38, "F#3 A3 C#4", 4), (42, "A3 C#4 E4", 6),
                   (48, "F#3 B3 D4", 6), (54, "F#3 B3 D4", 4), (58, "F#3 A3 D4", 6)], vel=94)
    loop = line(16, [(0, "D5", 1), (2, "B4", 1), (3, "A4", 1), (4, "F#4", 1), (6, "A4", 1), (7, "B4", 1), (8, "D5", 1),
                     (10, "E5", 1), (11, "D5", 1), (12, "B4", 1), (14, "A4", 1), (15, "F#4", 1)], vel=120)
    brk = drum(16, {"K": "x... .... .... ....", "H": SH_AND, "B": "..x. .... ..x. ....", "A": ".... ..x. .... ..x."})
    build = drum(64, {"K": "x... .... .... .... x... .... x... .... x... x... x... x... .... .... .... ....",
                      "B": "..x. ..x. ..x. ..x. ..x. ..x. ..x. ..x. ..x. ..x. ..x. ..x. .... .... .... ...."})
    drop = drum(16, {"K": K_4, "H": SH_AND, "C": ".... x... .... x...", "O": "..x. ..x. ..x. ..x.",
                     "B": "..x. ..x. ..x. ..x.", "A": ".... ...x .... ...x", "R": ".... .... ...x ...."})
    drop4 = drum(16, {"K": K_4, "H": SH_AND, "C": ".... x... .... x...", "O": "..x. ..x. ..x. ..x.",
                      "B": "..x. ..x. ..x. ..x.", "A": ".... ...x .... ...x", "T": ".... .... .x.x .x.x"})
    # the build: the guitar and the piano open from closed over three bars, the fourth bar silent
    opening = [(2, s, "ED_FX", -50 + s * 50 // 48) for s in range(0, 48, 8)] + [(1, s, "ED_FX", -40 + s * 40 // 48)
                                                                                for s in range(0, 48, 8)]
    return dict(
        name="DO AM AGAIN", mix=[83, 80, 110, 83], chant="MEE YA OZO", line="DO AM AGAIN.", bpm=125, cam="10B",
        tracks=[log_drum(D, drv=34, cut=110, glide=14, scale=MAJ),
                piano(D, scale=MAJ, level=100, rev=40),
                guitar(D, scale=MAJ, role="lead", damp=70, brit=80, level=104, pan=-10, gate=48, SLCR=SL_GATE, SLPAT=13,
                       SLRATE=0, SLDEPTH=50),
                kit(shaker=74, cabasa=70, clap=100, bell=64)],
        globals=dict(DTIME=11, DFDBK=36, DCOLOR=64, DMIX=52, RSIZE=84, RDAMP=70, RTYPE=0),
        sections=[
            ("BREAK", dict(tracks=[sparse, pno, rep(loop, 4), rep(brk, 4)], motion=[(2, 0, "ED_FX", -30)])),
            ("BUILD", dict(tracks=[silence_last_beat(sparse)[:48] + [None] * 16, pno[:48] + [None] * 16,
                                   rep(loop, 3) + [None] * 16, build], motion=opening)),
            ("DROP", dict(tracks=[riffb, pno, rep(loop, 4), cat(drop, drop, drop, drop4)])),
            ("GROOVE", dict(tracks=[riffb, [], rep(loop, 4), rep(brk, 4)])),
        ],
        arrangement=[("BREAK", 2), ("BUILD", 1), ("DROP", 4), ("GROOVE", 2), ("BREAK", 2), ("BUILD", 1), ("DROP", 4),
                     ("GROOVE", 1), ("BREAK", 1), ("BUILD", 1), ("DROP", 4), ("GROOVE", 2)],
    )


# ========================================================== 9 YOU DEY WHINE ME === #
# E minor, 124: the flirt, the peak holding (an afro house push without the four on the floor: the congas drive it).
# Em9 - Cmaj7 - G - D (i VI III VII), Rhodes, the lead as a voice (VOICE: a sung line treated like a sample), the log
# drum. Opens on "Just ask."
def you_dey_whine_me():
    E, K = 4, (4, MIN)
    log = log_bars(["E1", "C2", "G1", "D2"], [(0, 0, 3, "A"), (4, 4, 2), (7, 0, 2), (10, 7, 2, "S"), (12, 4, 2),
                                              (14, 0, 2)], K)
    run = silence_last_beat(log_bars(["E1", "D2"], [(0, 0, 3, "A"), (4, 4, 2), (7, 0, 2, "S"), (10, 2, 2), (12, 0, 2)],
                                     K, vel=118))
    ch = ["G3 B3 F#4", "G3 B3 E4", "G3 B3 D4", "F#3 A3 D4"]
    rh = stabs(ch, "x... ..x. ..x. ....", held=2, vel=106)
    rh_drop = stabs(ch, "x..x ..x. ..x. .x..", held=1, vel=112)
    vox = line(64, [
        (0, "B4", 2, "A"), (2, "D5", 2), (4, "E5", 4), (10, "D5", 1), (11, "B4", 1), (12, "A4", 4),
        (16, "G4", 2), (18, "A4", 2), (20, "B4", 6), (28, "A4", 2), (30, "G4", 2),
        (32, "B4", 2, "A"), (34, "D5", 2), (36, "E5", 4), (42, "G5", 2), (44, "F#5", 2), (46, "E5", 2),
        (48, "D5", 4), (52, "B4", 2), (54, "A4", 2), (56, "B4", 8)], vel=102)
    intro = drum(16, {"H": SH_BEAT, "T": "..x. .x.. ..x. .xx.", "L": "x... .... x... ....", "Q": "...x .... ...x ...."})
    groove = drum(16, {"K": "x... ..x. x... ..x.", "H": SH_BEAT, "C": ".... x... .... x...", "T": "..x. .x.. ..x. .xx.",
                       "L": "x... .... x... ....", "Q": "...x .... ...x ...."})
    drop = drum(16, {"K": "x... ..x. x... ..x.", "H": SH_BEAT, "C": ".... x... .... x...", "T": "..x. .x.. ..x. .xx.",
                     "L": "x... .... x... ....", "Q": "...x .... ...x ....", "O": "..x. ..x. ..x. ..x.",
                     "R": ".... ...x .... ...."})
    drop4 = drum(16, {"K": "x... ..x. x... ....", "H": SH_BEAT, "C": ".... x... .... x...", "T": "..x. .x.. ..x. .x.x",
                      "L": "x... .... x... ..x.", "Q": "...x .... ...x .x..", "O": "..x. ..x. ..x. x.x."})
    run_k = drum(32, {"K": "x... ..x. x... ..x. x... ..x. .... ...."})
    return dict(
        name="YOU DEY WHINE ME", mix=[87, 89, 77, 84], short="YOU DEY", chant="ANYA", line="JUST ASK.", bpm=124, cam="9A",
        tracks=[log_drum(E, drv=20),
                rhodes(E, level=100),
                sound("VOICE", "VOX LEAD", role="lead", LEVEL=96, PAN=6, CHOR=16, DLY=30, REV=40, VOICE=V_LEGATO,
                      GLIDE=6, ROOT=E, SCALE=MIN, QUANT=1),
                kit(shaker=74, cabasa=68, conga=96, clap=98)],
        globals=dict(DTIME=11, DFDBK=42, DCOLOR=60, DMIX=56, RSIZE=86, RDAMP=64, RTYPE=0),
        sections=[
            ("INTRO", dict(tracks=[[], rh, [], rep(intro, 4)], motion=[(1, 0, "ED_FX", -26)])),
            ("GROOVE", dict(tracks=[log, rh, [], rep(groove, 4)])),
            ("DROP", dict(tracks=[log, rh_drop, vox, cat(drop, drop, drop, drop4)])),
            ("RUN", dict(tracks=[run, [], [], run_k])),
        ],
        arrangement=[("INTRO", 2), ("GROOVE", 4), ("RUN", 1), ("DROP", 4), ("INTRO", 2), ("GROOVE", 2), ("RUN", 1),
                     ("DROP", 4), ("GROOVE", 2), ("DROP", 4), ("INTRO", 1)],
    )


# ================================================================ 10 TURN AM UP === #
# E major, 126: the last peak, the strobe. C#m - A - E - B (vi IV I V), euphoric and aloof. The surf-picked highlife
# guitar riff opens it and returns in every drop (tremolo: each sixteenth picked twice), four on the floor in the drops
# only, emotional grand piano, the log drum answering the bass stabs; two breakdown, build, drop cycles. Opens on the
# guitar, then "Turn am up."
def turn_am_up():
    E, K = 4, (4, MAJ)
    roots = ["C#2", "A1", "E1", "B1"]
    log = log_bars(roots, [(0, 0, 2, "A"), (2, 0, 1), (6, 4, 2), (8, 7, 2, "S"), (11, 4, 1), (12, 0, 2), (14, 4, 2)], K,
                   vel=114)
    sparse = log_bars(roots, [(0, 0, 4, "A"), (10, 4, 2)], K, vel=106)
    pno = mel(64, [(0, "G#3 C#4 E4", 8), (8, "G#3 C#4 E4", 8), (16, "A3 C#4 E4", 8), (24, "A3 C#4 E4", 8),
                   (32, "G#3 B3 E4", 8), (40, "G#3 B3 E4", 8), (48, "F#3 B3 D#4", 8), (56, "F#3 B3 D#4", 8)], vel=92)
    gtr = line(64, [(b * 16 + s, n, 1, "r2") for b, notes in enumerate([
        ["C#5", "E5", "G#5", "E5", "C#5", "E5", "G#5", "B5", "G#5", "E5", "C#5", "E5", "G#5", "E5", "C#5", "B4"],
        ["A4", "C#5", "E5", "C#5", "A4", "C#5", "E5", "A5", "E5", "C#5", "A4", "C#5", "E5", "C#5", "A4", "G#4"],
        ["G#4", "B4", "E5", "B4", "G#4", "B4", "E5", "G#5", "E5", "B4", "G#4", "B4", "E5", "B4", "G#4", "B4"],
        ["F#4", "B4", "D#5", "B4", "F#4", "B4", "D#5", "F#5", "D#5", "B4", "F#4", "B4", "D#5", "F#5", "G#5", "B5"]])
        for s, n in enumerate(notes)], vel=118)
    brk = drum(16, {"K": "x... .... .... ....", "H": SH_AND, "W": "..x. .... ..x. ....", "B": ".... ..x. .... ..x."})
    build = drum(64, {"K": "x... .... .... .... x... .... x... .... x... x... x... x... .... .... .... ....",
                      "H": "oxXo oxXo oxXo oxXo oxXo oxXo oxXo oxXo .... .... .... .... .... .... .... ...."})
    drop = drum(16, {"K": K_4, "H": SH_AND, "C": ".... x... .... x...", "O": "..x. ..x. ..x. ..x.",
                     "B": "..x. ..x. ..x. ..x.", "R": "...x .... .... ..x."})
    drop4 = drum(16, {"K": K_4, "H": SH_AND, "C": ".... x... .... x...", "O": "..x. ..x. ..x. ..x.",
                      "B": "..x. ..x. ..x. ..x.", "T": ".... .... .x.x .x.x"})
    opening = [(1, s, "ED_FX", -44 + s * 44 // 48) for s in range(0, 48, 8)] + \
              [(2, s, "ED_FX", -50 + s * 50 // 48) for s in range(0, 48, 8)]
    return dict(
        name="TURN AM UP", mix=[84, 84, 103, 83], chant="ELU", line="TURN AM UP.", bpm=126, cam="12B",
        tracks=[log_drum(E, drv=30, scale=MAJ),
                piano(E, scale=MAJ, level=100, rev=40),
                guitar(E, scale=MAJ, role="lead", damp=60, brit=86, level=100, pan=8, gate=40, rev=30, dly=10),
                kit(shaker=74, cabasa=70, clap=100, bell=62)],
        globals=dict(DTIME=11, DFDBK=34, DCOLOR=66, DMIX=50, RSIZE=84, RDAMP=72, RTYPE=0),
        sections=[
            ("INTRO", dict(tracks=[sparse, pno, gtr, rep(brk, 4)])),
            ("BUILD", dict(tracks=[silence_last_beat(sparse)[:48] + [None] * 16, pno[:48] + [None] * 16,
                                   gtr[:48] + [None] * 16, build], motion=opening)),
            ("DROP", dict(tracks=[log, pno, gtr, cat(drop, drop, drop, drop4)])),
            ("BREAK", dict(tracks=[sparse, pno, [], rep(brk, 4)])),
        ],
        arrangement=[("INTRO", 2), ("BUILD", 1), ("DROP", 4), ("BREAK", 3), ("BUILD", 1), ("DROP", 4), ("BREAK", 2),
                     ("BUILD", 1), ("DROP", 6), ("INTRO", 2)],
    )


# ============================================================= 11 ENJOYMENT ONLY === #
# C minor, 114: the house rule, the night at its fullest. Cm - Fm - Ab - G (i iv VI V, the leading tone's lift), the
# Igbo ancestral highlife as emotional house: a filtered highlife guitar-and-ogene loop pumping, emotional piano, the
# log drum. Opens on "Not in this house."
def enjoyment_only():
    C, K = 0, (0, "harm")
    log = log_bars(["C2", "F1", "Ab1", "G1"], [(0, 0, 3, "A"), (3, 0, 2), (6, 4, 2), (9, 7, 2, "S"), (11, 4, 2),
                                               (14, 0, 2)], K)
    run = silence_last_beat(log_bars(["C2", "G1"], [(0, 0, 3, "A"), (3, 0, 2), (6, 7, 2, "S"), (8, 4, 2), (11, 0, 2),
                                                    (13, 2, 2)], K, vel=118))
    pno = mel(64, [(0, "G3 C4 Eb4", 4), (6, "G3 C4 Eb4", 3), (12, "G3 C4 Eb4", 3),
                   (16, "Ab3 C4 F4", 4), (22, "Ab3 C4 F4", 3), (28, "Ab3 C4 F4", 3),
                   (32, "Ab3 C4 Eb4", 4), (38, "Ab3 C4 Eb4", 3), (44, "Ab3 C4 Eb4", 3),
                   (48, "G3 B3 D4", 4), (54, "G3 B3 D4", 3), (60, "G3 B3 F4", 3)], vel=94)
    loop = line(32, [(0, "G4", 1), (1, "C5", 1), (3, "Eb5", 1), (4, "D5", 1), (6, "C5", 1), (8, "G4", 1), (10, "Bb4", 1),
                     (11, "C5", 1), (12, "Eb5", 1), (14, "C5", 1),
                     (16, "Ab4", 1), (17, "C5", 1), (19, "F5", 1), (20, "Eb5", 1), (22, "C5", 1), (24, "Ab4", 1),
                     (26, "B4", 1), (27, "D5", 1), (28, "F5", 1), (30, "D5", 1)], vel=120)
    intro = drum(16, {"H": SH_AND, "B": "x.x. .x.x .x.x .x..", "A": "...x .... ...x ...."})
    groove = drum(16, {"K": K_AMAP, "H": SH_AND, "R": ".... x... .... x...", "B": "x.x. .x.x .x.x .x..",
                       "A": "...x .... ...x ...."})
    drop = drum(16, {"K": "x... ..x. x... ....", "H": SH_AND, "C": ".... x... .... x...", "R": "...x .... ...x ....",
                     "B": "x.x. .x.x .x.x .x..", "A": "...x .... ...x ....", "T": "..x. .... ..x. .x.."})
    drop4 = drum(16, {"K": "x... ..x. x... ....", "H": SH_AND, "C": ".... x... .... x...", "R": "...x .... ...x ....",
                      "B": "x.x. .x.x .x.x .x..", "A": "...x .... ...x ....", "T": "..x. .... ..x. x.x."})
    run_k = drum(32, {"K": "x... ..x. x... .... x... ..x. .... ...."})
    return dict(
        name="ENJOYMENT ONLY", mix=[88, 80, 112, 86], short="ENJOYMENT", chant="OFUMA", line="NOT IN THIS HOUSE.", bpm=114, cam="5A",
        tracks=[log_drum(C, drv=20),
                piano(C, level=98, rev=40),
                guitar(C, role="lead", damp=66, brit=76, level=104, pan=-10, gate=48, SLCR=SL_GATE, SLPAT=13, SLRATE=0,
                       SLDEPTH=46),
                kit(shaker=74, bell=66, clap=96)],
        globals=dict(DTIME=11, DFDBK=40, DCOLOR=60, DMIX=56, RSIZE=88, RDAMP=66, RTYPE=0),
        sections=[
            ("INTRO", dict(tracks=[[], pno, rep(loop, 2), rep(intro, 4)], motion=[(2, 0, "ED_FX", -34)])),
            ("GROOVE", dict(tracks=[log, pno, rep(loop, 2), rep(groove, 4)], motion=[(2, 0, "ED_FX", -16)])),
            ("DROP", dict(tracks=[log, pno, rep(loop, 2), cat(drop, drop, drop, drop4)])),
            ("RUN", dict(tracks=[run, [], [], run_k])),
        ],
        arrangement=[("INTRO", 2), ("GROOVE", 4), ("RUN", 1), ("DROP", 4), ("INTRO", 2), ("GROOVE", 2), ("RUN", 1),
                     ("DROP", 4), ("GROOVE", 2), ("DROP", 2), ("INTRO", 2)],
    )


# =================================================================== 12 RING ME === #
# D minor, 116: after the party, the phone: ring first. The call home, the bell (an afro house lean). Dm - Am - Bb - C
# (i v VI VII), the ogene as the riff (FM6's iron bell, MONO: each strike stops the last, the call clear), a hum under
# it, the log drum. Opens on "Ring am first."
def ring_me():
    D, K = 2, (2, MIN)
    log = log_bars(["D2", "A1", "Bb1", "C2"], [(0, 0, 3, "A"), (4, 0, 2), (7, 4, 2), (10, 0, 2), (12, 7, 2, "S"),
                                               (14, 4, 2)], K)
    run = silence_last_beat(log_bars(["D2", "C2"], [(0, 0, 3, "A"), (4, 0, 2), (7, 4, 2, "S"), (9, 2, 2), (12, 0, 2)],
                                     K, vel=118))
    bell = line(32, [(0, "D6", 2, "A"), (3, "A5", 1), (4, "D6", 2), (6, "A5", 2), (10, "C6", 2), (12, "A5", 2),
                     (16, "D6", 2, "A"), (19, "A5", 1), (20, "D6", 2), (22, "F6", 2), (24, "E6", 2), (26, "D6", 2),
                     (28, "C6", 2), (30, "A5", 2)], vel=96)
    hum = mel(64, [(0, "D3 A3", 16), (16, "C3 A3", 16), (32, "D3 Bb3", 16), (48, "E3 C4", 16)], vel=96)
    intro = drum(16, {"H": SH_BEAT, "T": "..x. .... ..x. .x..", "W": "x..x ..x. ...x ..x."})
    groove = drum(16, {"K": "x... ..x. x... ..x.", "H": SH_BEAT, "C": ".... x... .... x...", "T": "..x. .... ..x. .x..",
                       "L": "x... .... .... ...."})
    drop = drum(16, {"K": "x... ..x. x... ..x.", "H": SH_BEAT, "C": ".... x... .... x...", "T": "..x. .x.. ..x. .x..",
                     "L": "x... .... x... ....", "O": "..x. ..x. ..x. ..x.", "R": "...x .... .... ...."})
    drop4 = drum(16, {"K": "x... ..x. x... ....", "H": SH_BEAT, "C": ".... x... .... x...", "T": "..x. .x.. ..x. x.x.",
                      "L": "x... .... x... ....", "O": "..x. ..x. ..x. x.x."})
    run_k = drum(32, {"K": "x... ..x. x... ..x. x... ..x. .... ...."})
    return dict(
        name="RING ME", mix=[87, 81, 62, 85], chant="OGENE", line="RING AM FIRST.", bpm=116, cam="7A",
        tracks=[log_drum(D, drv=20),
                sound("FM6", "OGENE", role="bell", LEVEL=100, PAN=10, CHOR=8, DLY=34, REV=40, VOICE=V_MONO, ROOT=D,
                      SCALE=MIN, QUANT=1),
                sound("VOICE", "CHOIR AAH", role="pad", ATK=60, REL=70, LEVEL=96, CHOR=20, REV=48, ROOT=D, SCALE=MIN,
                      QUANT=1, SGATE=127),
                kit(shaker=74, cabasa=68, conga=90, clap=98)],
        globals=dict(DTIME=10, DFDBK=46, DCOLOR=56, DMIX=60, RSIZE=92, RDAMP=60, RTYPE=0),
        sections=[
            ("INTRO", dict(tracks=[[], rep(bell, 2), hum, rep(intro, 4)])),
            ("GROOVE", dict(tracks=[log, [], hum, rep(groove, 4)])),
            ("DROP", dict(tracks=[log, rep(bell, 2), hum, cat(drop, drop, drop, drop4)])),
            ("RUN", dict(tracks=[run, [], [], run_k])),
        ],
        arrangement=[("INTRO", 2), ("GROOVE", 4), ("RUN", 1), ("DROP", 4), ("INTRO", 2), ("GROOVE", 2), ("RUN", 1),
                     ("DROP", 4), ("GROOVE", 2), ("DROP", 2), ("INTRO", 2)],
    )


# ======================================================== 13 JAPA AND COME BACK === #
# G# minor, 115: the flight home, the return. A man runs from himself as far as the world allows and comes back with
# what he found. G#m - E - B - F# (i VI III VII), Rhodes, the sax as the voice of the return, the log drum. Opens on "I
# go come back."
def japa_and_come_back():
    Gs, K = 8, (8, MIN)
    log = log_bars(["G#1", "E1", "B1", "F#1"], [(0, 0, 3, "A"), (3, 0, 2), (6, 4, 2), (9, 0, 2), (12, 7, 2, "S"),
                                                (14, 4, 2)], K)
    run = silence_last_beat(log_bars(["G#1", "F#1"], [(0, 0, 3, "A"), (3, 0, 2), (6, 7, 2, "S"), (8, 4, 2), (11, 0, 2),
                                                      (13, 4, 2)], K, vel=118))
    ch = ["B3 D#4 G#4", "B3 E4 G#4", "B3 D#4 F#4", "A#3 C#4 F#4"]
    rh = stabs(ch, "x... ..x. ..x. ....", held=2, vel=106)
    rh_drop = stabs(ch, "x..x ..x. ..x. .x..", held=1, vel=112)
    riff = line(64, [
        (0, "D#5", 4, "A"), (4, "C#5", 2), (6, "B4", 2), (8, "G#4", 6), (14, "B4", 2),
        (16, "C#5", 4), (20, "B4", 2), (22, "G#4", 2), (24, "E4", 6), (30, "F#4", 2),
        (32, "D#5", 4, "A"), (36, "C#5", 2), (38, "B4", 2), (40, "F#5", 4), (44, "E5", 2), (46, "D#5", 2),
        (48, "C#5", 6), (54, "B4", 2), (56, "A#4", 4), (60, "F#4", 4)], vel=104)
    intro = drum(16, {"H": SH_AND, "R": ".... x... .... x...", "E": "x... ..x. .... ..x."})
    groove = drum(16, {"K": K_3STEP, "H": SH_AND, "R": ".... x... .... x...", "E": "x... ..x. .... ..x.",
                       "T": "..x. .... .x.. ...."})
    drop = drum(16, {"K": K_3STEP, "H": SH_AND, "C": ".... x... .... x...", "R": "...x .... ..x. ....",
                     "E": "x... ..x. .... ..x.", "T": "..x. .... .x.. ...x", "B": "..x. ..x. ..x. ..x."})
    drop4 = drum(16, {"K": "x... ..x. .... ....", "H": SH_AND, "C": ".... x... .... x...", "R": "...x .... ..x. ....",
                      "E": "x... ..x. .x.x .x.x", "T": "..x. .... .x.. ....", "B": "..x. ..x. ..x. x.x."})
    run_k = drum(32, {"K": "x... ..x. .... x... x... ..x. .... ...."})
    return dict(
        name="JAPA AND COME BACK", mix=[89, 89, 78, 87], short="JAPA", chant="IJE", line="I GO COME BACK.", bpm=115, cam="1A",
        tracks=[log_drum(Gs, drv=18),
                rhodes(Gs, level=100),
                sax(Gs, level=96, rev=46),
                kit(shaker=74, conga=86, bell=60)],
        globals=dict(DTIME=10, DFDBK=48, DCOLOR=54, DMIX=60, RSIZE=96, RDAMP=58, RTYPE=0),
        sections=[
            ("INTRO", dict(tracks=[[], rh, riff, rep(intro, 4)], motion=[(1, 0, "ED_FX", -26), (2, 0, "LEVEL", 80)])),
            ("GROOVE", dict(tracks=[log, rh, [], rep(groove, 4)])),
            ("DROP", dict(tracks=[log, rh_drop, riff, cat(drop, drop, drop, drop4)])),
            ("RUN", dict(tracks=[run, [], [], run_k])),
        ],
        arrangement=[("INTRO", 2), ("GROOVE", 4), ("RUN", 1), ("DROP", 4), ("INTRO", 2), ("GROOVE", 2), ("RUN", 1),
                     ("DROP", 4), ("GROOVE", 2), ("DROP", 2), ("INTRO", 2)],
    )


SONGS = [up_nepa(), we_outside(), oya_come(), where_you_sleep(), e_go_be(), i_dey_waka(), body_know(), do_am_again(),
         you_dey_whine_me(), turn_am_up(), enjoyment_only(), ring_me(), japa_and_come_back()]
