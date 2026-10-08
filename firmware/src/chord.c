/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Chord keys (SCL > CHORD: P_CHRD, P_VOIC): one key, MIDI note or arp input plays a chord. Included by seq.c
 * after kb_map; runs in the audio ISR with the rest of the note input.
 *   CHRD OFF; DIA3 / DIA7: the triad / seventh of the track's scale (ROOT, SCALE) built on the key, every tone
 *        in key (a key outside the scale takes the scale note below it, as QNT SNAP does; CHR builds on the
 *        major scale of ROOT); MAJ MIN DOM7 MAJ7 MIN7 SUS4 POW: that shape on the key, whatever the scale.
 *   VOIC CLOSE (root position); OPEN (the second tone an octave up: 1-5-3); INV1 / INV2 (the lowest tone, then
 *        the next, an octave up); +OCT (the root an octave down added; a seventh drops its fifth). At most
 *        CHORD_MAX notes, notes outside 0..127 left out.
 * The key is mapped first (kb_map: QNT SNAP / WHITE, TRN, the octave), the chord is built on the note it gives.
 * MONO / LEGATO / UNISON parts play the chord's root only; a kit (the DRUM engine, slices: an
 * engine that maps the keys itself) ignores CHRD. Every source keeps the notes it started (kb_chord for a
 * key, mchord for a MIDI note) and its release ends exactly those, so CHRD / VOIC changed while it is held
 * leave nothing hanging. A note several sources hold sounds once and ends with the last of them.
 *
 * EDDA OS, CHORD+: with CHRD on and QNT WHITE (the white keys walk the scale, the black keys are silent) the
 * black keys of the selected track change the chord while they are held, by their place (key_place):
 *   1 F#3 MIN (the third flattened)   2 G#3 7TH (the minor seventh)   3 A#3 MAJ7   4 C#4 SUS4 (no third, the fourth)
 *   5 D#4 9TH (the ninth; a seventh keeps 1-3-7-9)   6 F#4 INV (the next inversion up)   7 G#4 +BASS (the root an
 *   octave down)   8 A#4 STRUM (the notes 18 ms apart, low to high)   9 C#5 OPEN (the second tone an octave up)
 * (seq.c keyboard_block: chp_key; the modifiers chp_held, chp_strum). They combine (MIN + 7TH: m7; SUS4 + 7TH:
 * 7sus4), the chord's name on the CHORD page follows. VOIC LEAD (VC_LEAD): smooth voice leading: of the chord's
 * inversions and octaves, the one whose notes move the least from the last chord played on the track
 * (chord_last), as a keyboard player's hand would. A strummed chord's later notes wait in a queue the audio
 * block serves (strum_tick); a key let go before they sound cancels them. */
enum { CH_OFF, CH_DIA3, CH_DIA7, CH_MAJ, CH_MIN, CH_DOM7, CH_MAJ7, CH_MIN7, CH_SUS4, CH_POW };
enum { VC_CLOSE, VC_OPEN, VC_INV1, VC_INV2, VC_BASS, VC_LEAD };
enum { CHP_MIN, CHP_7TH, CHP_MAJ7, CHP_SUS4, CHP_9TH, CHP_INV, CHP_BASS, CHP_STRUM, CHP_OPEN, CHP_COUNT };
static uint16_t chp_held;                /* bit CHP_*: the black key is held (seq.c) */
#define CHP_STRUM_GAP 794u               /* 18 ms between a strummed chord's notes */
static const char *const CHP_NAME[CHP_COUNT] = {"MIN", "7TH", "MAJ7", "SUS4", "9TH", "INV", "BASS", "STRUM", "OPEN"};
#define CHORD_MAX 4u
#define MCHORD_N 24u                     /* MIDI notes held as chords at once (more: their root alone) */

static const int8_t CHORD_SHAPE[CH_POW - CH_MAJ + 1][CHORD_MAX] = {
    {0, 4, 7, -1}, {0, 3, 7, -1}, {0, 4, 7, 10}, {0, 4, 7, 11}, {0, 3, 7, 10}, {0, 5, 7, -1}, {0, 7, 12, -1},
};

static uint8_t kb_chord[27][CHORD_MAX], kb_chn[27];   /* the notes key k started (kb_chn 0: none) */
typedef struct { uint8_t id, ch, src, n, note[CHORD_MAX]; } mchord_t;   /* id: track + 1, 0 = free */
static mchord_t mchord[MCHORD_N];
/* the last chord played per track (the CHORD page shows it): its root, notes, and the shape's tones above the
 * root (bit i = i semitones, 0..11) */
static struct { uint8_t root, n, note[CHORD_MAX]; uint16_t mask; uint8_t gen; } chord_last[NTRK];

/* 1 = the track's keys are a kit: no chords */
static int chord_kit(const track_t *t)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    return e->oneshot || (e->keys && e->keys(t, 0) >= 0);
}

/* the tones of the chord (semitones above *root, ascending, iv[0] = 0) -> their number; DIA may move *root
 * down onto the scale */
static uint32_t chord_tones(const track_t *t, uint32_t mode, int32_t *root, int32_t *iv)
{
    uint32_t n = 0, i;
    if (mode >= CH_MAJ) {
        for (i = 0; i < CHORD_MAX && CHORD_SHAPE[mode - CH_MAJ][i] >= 0; i++)
            iv[n++] = CHORD_SHAPE[mode - CH_MAJ][i];
        return n;
    }
    {
        uint32_t mask = scale_mask(t), deg[12], c = 0, at = 0, guard = 12;
        int32_t r = t->p[P_ROOT];
        if (mask == 0xFFFu)
            mask = SCALE_MASK[1];                       /* CHR: no key of its own, the major scale of ROOT */
        while (guard-- && !((mask >> (uint32_t)((*root - r + 120) % 12)) & 1u))
            (*root)--;                                  /* outside the scale: the scale note below */
        for (i = 0; i < 12u; i++)
            if ((mask >> i) & 1u) {
                if (i == (uint32_t)((*root - r + 120) % 12))
                    at = c;
                deg[c++] = i;
            }
        for (i = 0; i < (mode == CH_DIA7 ? 4u : 3u); i++) {   /* every other note of the scale */
            uint32_t k = at + 2u * i;
            iv[n++] = (int32_t)deg[k % c] + 12 * (int32_t)(k / c) - (int32_t)deg[at];
        }
        return n;
    }
}

/* the chord on note `root` as track t plays it now -> out (ascending), the number of notes (1..CHORD_MAX);
 * *rp its root, *maskp its tones above the root (bit i: i semitones; 0 = no chord). The root alone when CHRD
 * is OFF, on a kit, or for MONO / LEGATO / UNISON (their chord's root) */
/* CHORD+ is live on track t: CHRD on, QNT WHITE (the black keys are free), not a kit */
static int chord_plus_on(const track_t *t)
{
    return t->p[P_CHRD] && t->p[P_CHRD] <= CH_POW && t->p[P_QUANT] == QN_WHITE && !chord_kit(t);
}
/* the held modifiers on the tones (semitones above the root, ascending, iv[0] = 0): n tones -> n */
static uint32_t chord_plus(uint32_t held, int32_t *iv, uint32_t n)
{
    uint32_t i, j, third = 0, seventh = 0;
    for (i = 1; i < n; i++) {
        if (iv[i] == 3 || iv[i] == 4) third = i;
        if (iv[i] == 10 || iv[i] == 11) seventh = i;
    }
    if ((held >> CHP_SUS4) & 1u) {                      /* the third becomes the fourth (none: added) */
        if (third) iv[third] = 5;
        else if (n < CHORD_MAX) iv[n++] = 5;
    } else if ((held >> CHP_MIN) & 1u) {
        if (third) iv[third] = 3;
        else if (n < CHORD_MAX) iv[n++] = 3;            /* (POW: 1 5 8 + b3) */
    }
    if (((held >> CHP_7TH) | (held >> CHP_MAJ7)) & 1u) {   /* 7TH wins over MAJ7 when both are held */
        int32_t sev = (held >> CHP_7TH) & 1u ? 10 : 11;
        if (seventh) iv[seventh] = sev;
        else if (n < CHORD_MAX) iv[n++] = sev;
        else iv[n - 1u] = sev;                          /* (a 9th chord: its top becomes the seventh) */
    }
    if ((held >> CHP_9TH) & 1u) {
        if (n < CHORD_MAX) iv[n++] = 14;
        else {                                          /* full: the fifth makes room (1-3-7-9) */
            for (i = 1; i < n; i++)
                if (iv[i] == 7) { iv[i] = 14; break; }
            if (i == n) iv[n - 1u] = 14;
        }
    }
    for (i = 1; i < n; i++)                             /* ascending, each once */
        for (j = i; j > 0 && iv[j - 1u] > iv[j]; j--) {
            int32_t x = iv[j];
            iv[j] = iv[j - 1u];
            iv[j - 1u] = x;
        }
    for (i = 1, j = 1; i < n; i++)
        if (iv[i] != iv[j - 1u])
            iv[j++] = iv[i];
    return j > n ? n : j;
}
/* VOIC LEAD: the notes (n, root r + iv) in the inversion and octave nearest the track's last chord: the sum of
 * each note's distance to the nearest note of the last chord, over the 3 octaves and n inversions */
static void chord_lead(uint32_t k, int32_t r, int32_t *iv, uint32_t n)
{
    int32_t best = 0x7FFFFFFF, cand[CHORD_MAX + 1u], pick[CHORD_MAX + 1u], x;
    uint32_t inv, oct, i, j;
    if (n < 2u || !chord_last[k].n)
        return;
    for (i = 0; i < n; i++)
        pick[i] = iv[i];
    for (inv = 0; inv < n; inv++) {
        for (oct = 0; oct < 3u; oct++) {
            int32_t cost = 0;
            for (i = 0; i < n; i++)                     /* inversion inv: the lowest inv tones an octave up */
                cand[i] = iv[i] + (i < inv ? 12 : 0) + 12 * ((int32_t)oct - 1);
            for (i = 0; i < n; i++) {
                int32_t d = 1000, note = r + cand[i];
                for (j = 0; j < chord_last[k].n; j++) {
                    x = note - (int32_t)chord_last[k].note[j];
                    x = x < 0 ? -x : x;
                    d = x < d ? x : d;
                }
                cost += d;
            }
            if (cost < best) {
                best = cost;
                for (i = 0; i < n; i++)
                    pick[i] = cand[i];
            }
        }
    }
    for (i = 0; i < n; i++)
        iv[i] = pick[i];
}
static uint32_t chord_make(const track_t *t, uint32_t root, uint8_t *out, int32_t *rp, uint16_t *maskp)
{
    int32_t iv[CHORD_MAX + 1u], r = (int32_t)root, x;
    uint32_t mode = (uint32_t)t->p[P_CHRD], n, i, j, m = 0, voic = (uint32_t)t->p[P_VOIC];
    uint32_t held = chord_plus_on(t) && t == &trk[song.sel] ? chp_held : 0u;
    uint16_t mask = 0;
    *rp = r;
    *maskp = 0;
    if (!mode || mode > CH_POW || chord_kit(t)) {
        out[0] = (uint8_t)root;
        return 1;
    }
    n = chord_tones(t, mode, &r, iv);
    if (held & ~(1u << CHP_INV | 1u << CHP_BASS | 1u << CHP_STRUM | 1u << CHP_OPEN))
        n = chord_plus(held, iv, n);                    /* CHORD+: the tones changed */
    for (i = 0; i < n; i++)
        mask |= (uint16_t)(1u << (iv[i] % 12));
    *rp = r;
    *maskp = mask;
    if (trk_vmode(t) != V_POLY) {                       /* one voice: the root */
        out[0] = (uint8_t)clamp(r, 0, 127);
        return 1;
    }
    if ((held >> CHP_OPEN) & 1u)
        voic = VC_OPEN;
    switch (voic) {
    case VC_OPEN:                                       /* 1-5-3(-7): the second tone an octave up */
        if (n >= 3u)
            iv[1] += 12;
        break;
    case VC_INV2:
    case VC_INV1:
        for (j = 0; j < (t->p[P_VOIC] == VC_INV2 ? 2u : 1u); j++) {   /* the lowest tone an octave up */
            x = iv[0] + 12;
            for (i = 1; i < n; i++)
                iv[i - 1u] = iv[i];
            iv[n - 1u] = x;
        }
        break;
    case VC_BASS:                                       /* the root an octave down; a seventh drops its fifth */
        if (n == CHORD_MAX) {
            iv[2] = iv[3];
            n--;
        }
        for (i = n; i > 0; i--)
            iv[i] = iv[i - 1u];
        iv[0] = -12;
        n++;
        break;
    case VC_LEAD:                                       /* EDDA OS: the nearest inversion to the last chord */
        chord_lead(trk_index(t), r, iv, n);
        break;
    default:
        break;
    }
    if ((held >> CHP_INV) & 1u && n >= 2u) {            /* CHORD+ INV: the lowest tone an octave up */
        x = iv[0] + 12;
        for (i = 1; i < n; i++)
            iv[i - 1u] = iv[i];
        iv[n - 1u] = x;
    }
    if ((held >> CHP_BASS) & 1u && voic != VC_BASS) {   /* CHORD+ BASS: the root an octave down (a seventh drops
                                                         * its fifth, as +OCT) */
        if (n == CHORD_MAX) {
            iv[2] = iv[3];
            n--;
        }
        for (i = n; i > 0; i--)
            iv[i] = iv[i - 1u];
        iv[0] = -12;
        n++;
    }
    for (i = 1; i < n; i++)                             /* ascending */
        for (j = i; j > 0 && iv[j - 1u] > iv[j]; j--) {
            x = iv[j];
            iv[j] = iv[j - 1u];
            iv[j - 1u] = x;
        }
    for (i = 0; i < n && m < CHORD_MAX; i++) {          /* inside 0..127, each note once */
        x = r + iv[i];
        if (x < 0 || x > 127 || (m && out[m - 1u] == (uint8_t)x))
            continue;
        out[m++] = (uint8_t)x;
    }
    if (!m) {
        out[0] = (uint8_t)clamp(r, 0, 127);
        return 1;
    }
    return m;
}

/* chord_make for a note played now: a chord is kept in chord_last (the CHORD page shows it) */
static uint32_t chord_build(track_t *t, uint32_t root, uint8_t *out)
{
    int32_t r;
    uint16_t mask;
    uint32_t m = chord_make(t, root, out, &r, &mask), i, k = trk_index(t);
    if (m > 1u) {
        chord_last[k].root = (uint8_t)clamp(r, 0, 127);
        chord_last[k].mask = mask;
        chord_last[k].n = (uint8_t)m;
        for (i = 0; i < CHORD_MAX; i++)
            chord_last[k].note[i] = i < m ? out[i] : 0u;
        chord_last[k].gen++;
    }
    return m;
}

/* a chord's name from its root and tones (bit i: i semitones above the root): "C", "Cm7", "F#m7b5", "Gsus4",
 * "A5" -> b (12 bytes) */
static void chord_name(char *b, uint32_t root, uint32_t mask)
{
    uint32_t mi = (mask >> 3) & 1u, ma = (mask >> 4) & 1u, b5 = (mask >> 6) & 1u, p5 = (mask >> 7) & 1u;
    uint32_t s5 = (mask >> 8) & 1u, d7 = (mask >> 9) & 1u, m7 = (mask >> 10) & 1u, M7 = (mask >> 11) & 1u;
    const char *q, *x = "";
    str_cpy(b, N_NOTE[root % 12u], 12);
    if (mi && !ma && b5 && !p5)
        q = d7 ? "dim7" : m7 ? "m7b5" : "dim";
    else if (ma && !mi && s5 && !p5)
        q = m7 ? "+7" : M7 ? "+M7" : "aug";
    else if (ma && !mi)
        q = m7 ? "7" : M7 ? "maj7" : d7 ? "6" : "";
    else if (mi)
        q = m7 ? "m7" : M7 ? "mM7" : d7 ? "m6" : "m";
    else if ((mask >> 5) & 1u) {                        /* no third: suspended */
        q = m7 ? "7" : "";
        x = "sus4";
    } else if ((mask >> 2) & 1u) {
        q = m7 ? "7" : "";
        x = "sus2";
    } else
        q = "5";
    str_cpy(b + str_len(b), q, 12 - str_len(b));
    str_cpy(b + str_len(b), x, 12 - str_len(b));
}

/* the MIDI note (ch, src) of track id - 1 that plays a chord, 0 = none */
static mchord_t *mchord_of(uint32_t ch, uint32_t src, uint32_t id)
{
    uint32_t i;
    for (i = 0; i < MCHORD_N; i++)
        if (mchord[i].id == id && mchord[i].ch == ch && mchord[i].src == src)
            return &mchord[i];
    return 0;
}

/* a MIDI-held chord of track id - 1 sounds note */
static int mchord_held(uint32_t id, uint32_t note)
{
    uint32_t i, j;
    for (i = 0; i < MCHORD_N; i++)
        if (mchord[i].id == id)
            for (j = 0; j < mchord[i].n; j++)
                if (mchord[i].note[j] == note)
                    return 1;
    return 0;
}

static void mchord_forget(uint32_t track)               /* panic: the track's MIDI chords are gone */
{
    uint32_t i;
    for (i = 0; i < MCHORD_N; i++)
        if (mchord[i].id == track + 1u)
            mchord[i].id = 0;
}

/* ---- EDDA OS: the strum queue (CHORD+ STRUM): a chord's later notes, each a gap after the one before, started by
 * the audio block (strum_tick, from events_block); a key let go first cancels its notes (strum_cancel) */
#define STRUM_N 16u
static void input_on(track_t *t, uint32_t note, uint32_t vel);   /* seq.c (after this file) */
static struct { uint8_t trk, note, vel, key; uint32_t left; } strum_q[STRUM_N];
static uint32_t strum_n;
static void strum_push(track_t *t, uint32_t note, uint32_t vel, uint32_t key, uint32_t delay)
{
    if (strum_n >= STRUM_N)
        return;
    strum_q[strum_n].trk = (uint8_t)trk_index(t);
    strum_q[strum_n].note = (uint8_t)note;
    strum_q[strum_n].vel = (uint8_t)vel;
    strum_q[strum_n].key = (uint8_t)key;
    strum_q[strum_n].left = delay;
    strum_n++;
}
static int strum_cancel(uint32_t key, uint32_t note)     /* 1 = the note was still waiting (never sounded) */
{
    uint32_t i;
    for (i = 0; i < strum_n; i++)
        if (strum_q[i].key == key && strum_q[i].note == note) {
            strum_q[i] = strum_q[--strum_n];
            return 1;
        }
    return 0;
}
static void strum_tick(uint32_t n)                      /* n samples pass */
{
    uint32_t i = 0;
    while (i < strum_n) {
        if (strum_q[i].left > n) {
            strum_q[i].left -= n;
            i++;
        } else {
            track_t *t = &trk[strum_q[i].trk % NTRK];
            uint32_t note = strum_q[i].note, vel = strum_q[i].vel, mc = trk_midi_ch(trk_index(t));
            strum_q[i] = strum_q[--strum_n];           /* (vetted at key_on: its key holds it, no other) */
            input_on(t, note, vel);
            midi_out_event(0x09u | (0x90u | mc) << 8 | note << 16 | vel << 24);
        }
    }
}
/* a black key of the selected track (seq.c keyboard_block) while CHORD+ is live: its modifier held (down) or let
 * go (up); 1 = taken (the key is silent) */
static int chp_key(uint32_t k, int down)
{
    uint32_t place = key_place(k);
    if (!key_black(k) || !chord_plus_on(&trk[song.sel]) || place >= CHP_COUNT)
        return 0;
    if (down)
        chp_held |= (uint16_t)(1u << place);
    else
        chp_held &= (uint16_t)~(1u << place);
    return 1;
}
