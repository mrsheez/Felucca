/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DRUM: a drum kit of 8 lanes (drum_voice.c, Felucca's own): KICK, SNARE, CLAP, HAT CL, HAT OP, TOM, RIM,
 * BELL. Before 1.0 it was PHYS's MODEL DRUM; projects and user presets saved then load as this engine
 * (drum_from_phys in core.h).
 *
 * Keys and notes: the keys play the General MIDI drum map with the kick on the first C (C KICK, C# RIM,
 * D SNARE, D# CLAP, F# HAT CL, A# HAT OP, F G A B C D TOMS, C# CYM, G# BELL; OCT -/+ move it), and a note
 * plays its GM drum (35..81: kicks, snares, toms, hats, cymbals, bells, congas, claves; other notes as
 * their octave of 36..47), so GM patterns and MIDI parts play as they did on SAMPLE PERC (retired: its sounds
 * load as this engine, core.h drum_from_perc).
 *
 * Parameters: KIT the kit: STD (Felucca's own: TOM RIM BELL on lanes 6..8, other GM notes their congas, claves,
 * cymbals) or a model kit (80 10 66 55 77: every lane that kit's own piece, drum_voice.c DV_KIT; KICK does not
 * apply). Its stored values 1..3 were HAND CYM H+CYM (STD with CONGA CLAVE / CYM on lanes 6..8) until 1.0.4: they
 * play 66 10 77 now (DK_PLAYS), and whatever stores them lands there (params.c enum_orig, param_fit: the knob
 * steps over them, the editor's SET, projects, user presets, motion). KICK the kick (PUNCH, ROUND). TUNE (64: as
 * designed, +-12 semitones), TONE, DECY and SNAP (each lane's extra: the kick's drive, the snare's snappiness,
 * the clap's spread, the hats' noise, the toms' bend, the rim's drive, the bell's strike) move every lane from its designed value. ACC
 * is the accent at full velocity (velocity scales it), DRV a soft clip on every hit (x1..x4, level kept).
 * Each lane has its LEVEL (P_LN0..P_LN7, EDIT > LANES / LANES 2, #97: square law, 100 % the kit as designed, the
 * default, so every older sound plays as it did), on the voice amplitude after the knee (a quieter lane is cleaner).
 * A hit keeps the drum and the variant it was struck with; the knobs move it while it rings.
 *
 * Polyphony: a lane per drum and part, mono: a hit reuses its lane's voice even at another pitch, the 8 lanes ring
 * together. A sounding voice plays one lane (s[0]); the lane remembers its voice (owner), and a voice
 * whose lane was struck again by another voice, or has rung out (-84 dB), ends itself (drum_amp). The
 * voices come from the shared budget (voice.c), up to 8 per part. A closed hat chokes the open one.
 *
 * What applies (engine_t.oneshot): the hit is the envelope. VOICE (always POLY), GLIDE, the ADSR, ENV DEST,
 * LFO DEST PIT / FLT / SHP, the global TUNE and the matrix's PITCH CUT SHP do nothing; LEVEL, PAN, the
 * sends, DIST, the SLICER, LFO DEST AMP, velocity, the matrix's AMP and its E1..E8 (the knobs, per
 * block) do. A released key does not end a hit.
 *
 * State: 8 lanes per part in the pool section (drum_kit: the parameters, coefficients, voice and metal
 * source of each lane).
 *
 * EDDA OS, user kits (KIT USR1..USR4): a user sample slot (eng_sample.c) is the kit, its zones the pads: a note
 * plays the zone holding it (the kit builder, tools/fm1_sample_upload.py kit / the editor's 16 pads, pins each
 * WAV to one note: the 8 lanes' notes first, so the grid's lanes are pads 1..8, then 41 43 47 48 49 50 51 53), at
 * its own pitch, one-shot, IMA ADPCM through the sample engine's decoder (smp_decode). A note with no pad of its
 * own (a GM pattern's low tom 41 on a kit with 8 pads) plays its lane's pad, pitched by its GM semitones (DRUM_GM),
 * so every DRUM pattern plays on every kit. TUNE +-12 semitones; TONE below 64 darkens (a one-pole low-pass);
 * DECY below 64 shortens (an exponential decay: gone after 25 ms at 0, 0.1 s at 10, 0.8 s at 32; 64 and up: the
 * sample whole); SNAP
 * above 64 skips into the sound (up to 50 ms: a tighter attack), below 64 fades it in (up to 50 ms: softer);
 * ACC the accented hits' lift (velocity scales every hit, as every engine); DRV and the lane LEVELs (the pad's
 * lane) as the synth kit. Polyphony: a pad plays one voice at a time (a hit restarts it), a closed hat (the
 * lane's pad) chokes the open one; the voices come from the part's budget of 8, the oldest stolen. The lane
 * table (drum_kit) is not used: a kit voice carries its own zone (v->s[5]).
 * Voice: s[0] the lane (as the synth kit: the LEVELs, the grid), s[1] s[2] the decoder (predictor, step index),
 * s[3] the last two samples (the previous in the low 16 bits, the current above), s[4] the kit's own amplitude
 * (Q24: the fade-in, DECY, the choke; kit_amp, not the ADSR's env, which env_tick moves), s[5] the zone
 * (smp_zone), s[6] 1 playing / 2 ended / 3 choked (fading), s[7] the low-pass; ph[0] the position, ph[1] its
 * fraction, ph[2] the fade-in: ticks left | ticks << 8, bit 16 primed, the GM semitones + 64 << 24. The engine is marked
 * sampled: a pad hit again starts its sound over (voice.c keeps no positions), as the synth kit restarts its hit. */
#include "drum_voice.c"

enum { DK_STD, DK_HAND, DK_CYM, DK_HCYM, DK_80, DK_10, DK_66, DK_55, DK_77,
       DK_USR1, DK_USR2, DK_USR3, DK_USR4, DK_COUNT };   /* (stored values; USR1..4: EDDA OS's user kits) */
#define DK_USR DK_USR1
/* the kit a stored KIT value plays: HAND CYM H+CYM (retired after 1.0.4) -> 66 (a conga on TOM), 10 (a cymbal on
 * BELL), 77 (claves on RIM, a cymbal on BELL) */
static const uint8_t DK_PLAYS[DK_COUNT] = {DK_STD, DK_66, DK_10, DK_77, DK_80, DK_10, DK_66, DK_55, DK_77,
                                           DK_USR1, DK_USR2, DK_USR3, DK_USR4};
static uint32_t drum_kit_plays(int32_t v) { return DK_PLAYS[clamp(v, 0, DK_COUNT - 1)]; }
static uint32_t drum_user_kit(const int16_t *p)      /* the user slot of the KIT, SMP_USER_SLOTS for a synth kit */
{
    uint32_t kit = drum_kit_plays(p[P_E0]);
    return kit >= DK_USR ? kit - DK_USR : SMP_USER_SLOTS;
}
/* the model kits' pieces where a lane plays another than its own (the lane's name): 1 TOM -> CONGA, 2 RIM -> CLAVE,
 * 4 BELL -> CYM */
static const uint8_t DK_SWAP[DV_NKIT] = {0, 4, 1, 0, 2 | 4};
static const uint8_t DV_TYPE_LANE[DVT_COUNT] = {
    DV_KICK, DV_KICK, DV_SNARE, DV_CLAP, DV_HATC, DV_HATO, DV_TOM, DV_TOM, DV_RIM, DV_RIM, DV_BELL, DV_BELL,
};

typedef struct {
    dv_param_t key;              /* the parameters the coefficients were set up with */
    dv_coef_t c;
    dv_voice_t v;
    dv_metal_t mb;
    uint8_t owner;               /* the voice playing the lane: index + 1, 0 = none */
    uint8_t role;                /* the drum struck (DVT_*) */
    int8_t st;                   /* its semitones from the designed pitch (the GM map) */
    uint8_t pad;
} drum_lane_t;

static drum_lane_t drum_kit[NPART][DV_NLANE] __attribute__((section(".pool")));

/* 1..3 named as the kit they play: aliases, never shown or offered (EDITOR_PROTOCOL.md: retired values) */
static const char *const N_DRUM_KIT[] = {"STD", "66", "10", "77", "80", "10", "66", "55", "77", "USR1", "USR2", "USR3", "USR4"};
static const char *const N_DRUM_KICK[] = {"PUNCH", "ROUND"};

/* General MIDI notes 35..81 -> the drum (DVT_*; DVT_PUNCH: the kick KICK picks) and semitones from its
 * designed pitch */
static const int8_t DRUM_GM[47][2] = {
    {DVT_PUNCH, -2}, {DVT_PUNCH, 0}, {DVT_RIM, 0}, {DVT_SNARE, 0}, {DVT_CLAP, 0}, {DVT_SNARE, 2},     /* 35 */
    {DVT_TOM, -7}, {DVT_HATC, 0}, {DVT_TOM, -4}, {DVT_HATC, -2}, {DVT_TOM, 0}, {DVT_HATO, 0},         /* 41 */
    {DVT_TOM, 3}, {DVT_TOM, 5}, {DVT_CYM, 0}, {DVT_TOM, 8}, {DVT_CYM, -3}, {DVT_CYM, 2},              /* 47 */
    {DVT_BELL, 5}, {DVT_HATC, 5}, {DVT_CYM, 4}, {DVT_BELL, 0}, {DVT_CYM, 1}, {DVT_CLAVE, -12},        /* 53 */
    {DVT_CYM, -2}, {DVT_CONGA, 5}, {DVT_CONGA, 2}, {DVT_CONGA, 0}, {DVT_CONGA, 0}, {DVT_CONGA, -5},   /* 59 */
    {DVT_TOM, 10}, {DVT_TOM, 7}, {DVT_BELL, 7}, {DVT_BELL, 3}, {DVT_HATC, 3}, {DVT_HATC, 7},          /* 65 */
    {DVT_CLAVE, 7}, {DVT_CLAVE, 5}, {DVT_HATC, -4}, {DVT_HATC, -6}, {DVT_CLAVE, 0}, {DVT_CLAVE, -4},  /* 71 */
    {DVT_CLAVE, -7}, {DVT_CONGA, 7}, {DVT_CONGA, 3}, {DVT_BELL, 12}, {DVT_BELL, 12},                  /* 77 */
};
/* the drum of a note (DVT_*, or a model kit's lane: DV_KTYPE) and its semitones; KICK picks the kick; a model kit
 * plays its own piece on the note's lane */
static uint32_t drum_gm(const int16_t *p, uint32_t note, int32_t *st)
{
    uint32_t n = note >= 35u && note <= 81u ? note : 36u + (note + 120u - 36u) % 12u, t = (uint32_t)DRUM_GM[n - 35u][0];
    uint32_t kit = drum_kit_plays(p[P_E0]);           /* STD, the model kits (a user kit: as STD) */
    *st = DRUM_GM[n - 35u][1];
    if (kit >= DK_80 && kit < DK_USR)
        return DV_KTYPE(kit - DK_80 + 1u, DV_TYPE_LANE[t]);
    return t == DVT_PUNCH && p[P_E6] > 0 ? DVT_ROUND : t;
}

/* ----------------------------------------------------- the grid's lanes --- */
/* A step's lane hits (step_t.hit / acc, the DRUM grid: SEQ > STEP on a DRUM track) play these GM notes, on any
 * engine: DRUM strikes its lanes, a synth plays the pitches. Each is the designed pitch of
 * its lane (st 0 in DRUM_GM) */
static const uint8_t DRUM_LANE_NOTE[NLANE] = {36, 38, 39, 42, 46, 45, 37, 56};

/* the lane a note strikes (KIT-proof: a kit plays its own piece inside the lane) */
static uint32_t drum_lane(uint32_t note)
{
    uint32_t n = note >= 35u && note <= 81u ? note : 36u + (note + 120u - 36u) % 12u;
    return DV_TYPE_LANE[DRUM_GM[n - 35u][0]];
}

/* KIT's swaps of lanes 6..8 (1 TOM -> CONGA, 2 RIM -> CLAVE, 4 BELL -> CYM) */
static uint32_t drum_swaps(const track_t *t)
{
    uint32_t kit = drum_kit_plays(t->p[P_E0]);
    return kit >= DK_80 && kit < DK_USR ? DK_SWAP[kit - DK_80] : 0u;
}

/* the lane's name as the track's KIT plays it (5 characters at most) */
static const char *drum_lane_name(const track_t *t, uint32_t l)
{
    static const char *const N[NLANE] = {"KICK", "SNARE", "CLAP", "HATCL", "HATOP", "TOM", "RIM", "BELL"};
    uint32_t kit = drum_swaps(t);
    l &= NLANE - 1u;
    if (l == DV_TOM && (kit & 1u))
        return "CONGA";
    if (l == DV_RIM && (kit & 2u))
        return "CLAVE";
    return (kit & 4u) && l == DV_BELL ? "CYM" : N[l];
}
/* .. in two letters, the drum machine way (the grid's lane column) */
static const char *drum_lane_abbr(const track_t *t, uint32_t l)
{
    static const char *const N[NLANE] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
    uint32_t kit = drum_swaps(t);
    l &= NLANE - 1u;
    if (l == DV_TOM && (kit & 1u))
        return "CG";
    if (l == DV_RIM && (kit & 2u))
        return "CL";
    return (kit & 4u) && l == DV_BELL ? "CY" : N[l];
}

/* the lanes step s strikes: its hits, and its notes on their lanes (a NOTE step only) */
static uint32_t step_lanes(const step_t *s)
{
    uint32_t m = 0, i;
    if (s->time != ST_NOTE)
        return 0;
    for (i = 0; i < s->n && i < 4u; i++)
        m |= 1u << drum_lane(s->note[i]);
    return m | s->hit;
}

/* .. and which of them are accented */
static uint32_t step_accents(const step_t *s) { return s->flags & SF_ACCENT ? step_lanes(s) : s->acc & step_lanes(s); }

/* a step's notes that are a lane's note become that lane's hits (the same note, the same velocity: nothing
 * sounds different). Other notes (a low tom 41, a crash 49) stay notes, shown on their lane. A step accent
 * of a step left with hits only becomes the hits' accents. Pattern loads into a DRUM track, projects of
 * before the grid, the grid's edits */
static void step_to_grid(step_t *s)
{
    uint32_t i, k = 0;
    if (s->time != ST_NOTE)
        return;
    for (i = 0; i < s->n && i < 4u; i++) {
        uint32_t l = drum_lane(s->note[i]);
        if (s->note[i] == DRUM_LANE_NOTE[l])
            s->hit |= (uint8_t)(1u << l);
        else
            s->note[k++] = s->note[i];
    }
    for (i = k; i < 4u; i++)
        s->note[i] = 0;
    s->n = (uint8_t)k;
    if (!k && (s->flags & SF_ACCENT)) {
        s->acc |= s->hit;
        s->flags &= (uint8_t)~SF_ACCENT;
    }
}

static drum_lane_t *drum_kit_of(const track_t *t)
{
    return t >= &trk[0] && t < &trk[NPART] ? drum_kit[t - trk] : 0;
}

/* the lane voice v plays, 0 when it plays none (any more) */
static drum_lane_t *drum_lane_of(track_t *t, const voice_t *v)
{
    drum_lane_t *K = drum_kit_of(t), *L;
    uint32_t i = (uint32_t)(v - t->v);
    if (!K || i >= NVOICE)
        return 0;
    L = &K[(uint32_t)v->s[0] & (DV_NLANE - 1u)];
    return L->owner == i + 1u ? L : 0;
}

/* Different GM pitches on a lane still share its one sounding voice. */
static voice_t *drum_reuse(track_t *t, uint32_t note)
{
    drum_lane_t *K = drum_kit_of(t);
    uint32_t lane = drum_lane(note), owner = K ? K[lane].owner : 0;
    voice_t *v;
    if (drum_user_kit(t->p) < SMP_USER_SLOTS)            /* a user kit: a pad's voice is its note's (voice_alloc) */
        return 0;
    if (!owner || owner > NVOICE)
        return 0;
    v = &t->v[owner - 1u];
    return v->active && (uint32_t)v->s[0] == lane ? v : 0;
}

/* ---- EDDA OS: the user kits */
#define KIT_FADE_MAX 2205u                                   /* SNAP: 50 ms of fade-in or skip at most */
static void kit_note_on(track_t *t, voice_t *v, uint32_t slot)
{
    const int16_t *p = t->p;
    uint32_t lane = drum_lane(v->note), zi = smp_user_zone(slot, v->note), i, skip = 0, fade = 0;
    int32_t st = 0, snap = p[P_E4] - 64;
    if (zi == 0xFFFFu) {                                 /* no pad of its own: its lane's pad, pitched the GM way */
        int32_t s2;
        drum_gm(p, v->note, &s2);
        zi = smp_user_zone(slot, DRUM_LANE_NOTE[lane]);
        st = s2;
    }
    if (snap > 0)
        skip = (uint32_t)snap * (KIT_FADE_MAX / 63u);
    else if (snap < 0)
        fade = ((uint32_t)(-snap) * (KIT_FADE_MAX / 63u) + CTL - 1u) / CTL;   /* in control ticks */
    v->s[0] = (int32_t)lane;
    v->s[1] = v->s[2] = v->s[3] = 0;
    v->s[4] = fade ? 0 : 1 << 24;
    v->s[5] = (int32_t)(zi == 0xFFFFu ? 0x8000u | slot << 5 : zi);
    v->s[6] = zi == 0xFFFFu ? 2 : 1;                     /* an empty pad: a silent hit that ends at once */
    v->s[7] = 0;
    v->ph[0] = 0;
    v->ph[1] = 0;
    v->ph[2] = fade | fade << 8 | (uint32_t)(st + 64) << 24;
    v->env = 1 << 24;
    v->env_out = fade ? 0 : v->vel * 258;
    if (zi != 0xFFFFu) {                                 /* SNAP's skip: decode past the start (the state follows) */
        const smp_zone_t *z = smp_zone(zi);
        uint32_t n = z->n > skip + 64u ? skip : 0u;
        for (i = 0; i < n; i++)
            smp_decode(z, &v->ph[0], &v->s[1], 0);
    }
    if (lane == DV_HATC)                                 /* a closed hat chokes the open one (the lane's pads) */
        for (i = 0; i < NVOICE; i++) {
            voice_t *o = &t->v[i];
            if (o != v && o->active && o->s[6] == 1 && drum_lane(o->note) == DV_HATO)
                o->s[6] = 3;
        }
}
static int32_t kit_amp(track_t *t, voice_t *v)          /* per control tick: the fade-in, DECY's decay, the choke */
{
    int32_t decy = t->p[P_E3], env = v->s[4];
    uint32_t left = v->ph[2] & 0xFFu, total = (v->ph[2] >> 8) & 0xFFu;
    v->env = 1 << 24;                                    /* (the ADSR held at full, as the synth kit: no release) */
    if (v->s[6] == 2 || (v->s[6] == 3 && env < (1 << 12))) {
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = 0;
        return 0;
    }
    if (v->s[6] == 3)
        env >>= 1;                                       /* choked: -6 dB a tick, gone in ~10 ms */
    else if (left) {                                     /* SNAP's fade-in: linear over its ticks */
        left--;
        env = (int32_t)(((1u << 24) / total) * (total - left));
        if (!left)
            env = 1 << 24;
        v->ph[2] = (v->ph[2] & 0xFFFFFF00u) | left;
    } else if (decy < 64) {                              /* tau = 100 + DECY^2 * 8 samples (2 ms .. 0.7 s): the hit is
                                                          * gone (-48 dB) after about 11 tau: 25 ms at 0, 0.1 s at 10,
                                                          * 0.8 s at 32 */
        int32_t tau = 100 + decy * decy * 8;
        env -= env / tau * (int32_t)CTL + (env % tau) * (int32_t)CTL / tau;   /* (env * CTL fits: <= 2^29) */
        if (env < (1 << 16)) {
            v->s[6] = 2;
            env = 0;
        }
    }
    v->s[4] = env;
    return env >> 9;
}
static void kit_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    const smp_zone_t *z = smp_zone((uint32_t)v->s[5]);
    uint32_t i, frac = v->ph[1], r, stepq;
    int32_t tone = p[P_E2], lv = clamp(p[P_LN0 + ((uint32_t)v->s[0] & (DV_NLANE - 1u))], 0, 127);
    int32_t d16 = (p[P_E1] - 64) * 3 + ((int32_t)(v->ph[2] >> 24) - 64) * 16;   /* TUNE +-12 st, the GM semitones */
    int32_t lp = tone >= 64 ? 32767 : 2000 + tone * (30767 / 64), drv = p[P_E7], g = 0, mk = 0;
    int32_t lift = v->vel >= 120u ? 32767 + clamp(p[P_E5], 0, 127) * 129 : 32767;   /* ACC: accented hits up to x1.5 */
    vmod_t ml;
    if (v->s[6] == 2 || !z->n) {
        v->active = 0;
        return;
    }
    if (lv < 127) {                                      /* the lane's LEVEL (square law) on the block's ramp */
        int32_t gl = lv * lv * 2 + (lv * lv >> 6);
        ml.amp0 = (m->amp0 >> 4) * gl >> 11;
        ml.amp1 = (m->amp1 >> 4) * gl >> 11;
        m = &ml;
    }
    r = pow2_q16(clamp(d16, -1536, 576));
    stepq = (r >> 8) * (z->rate >> 8);                   /* Q16 samples per output sample */
    if (drv > 0) {
        g = 4096 + drv * 3 * 4096 / 127;
        mk = (int32_t)((19661u << 15) / (uint32_t)softclip((19661 * g) >> 12));
    }
    {
        int32_t prev = (int16_t)(v->s[3] & 0xFFFF), cur = v->s[3] >> 16;   /* the last two decoded samples */
        if (!(v->ph[2] & 0x10000u)) {                    /* the first block: prime the interpolator */
            v->ph[2] |= 0x10000u;
            if (v->ph[0] < z->n)
                cur = smp_decode(z, &v->ph[0], &v->s[1], 0);
        }
        for (i = 0; i < n; i++) {
            int32_t s;
            frac += stepq;
            while (frac >= 65536u) {
                frac -= 65536u;
                prev = cur;
                if (v->ph[0] >= z->n) {                  /* the end of the sound */
                    v->s[6] = 2;
                    cur = 0;
                    if (frac >= 65536u) {
                        prev = 0;
                        frac &= 65535u;
                    }
                    break;
                }
                cur = smp_decode(z, &v->ph[0], &v->s[1], 0);
            }
            s = prev + (((cur - prev) * (int32_t)(frac >> 1)) >> 15);
            v->s[7] += mulq15(s - v->s[7], lp);          /* TONE */
            s = mulq15(v->s[7], lift);
            if (g)
                s = (softclip((s * g) >> 12) * mk) >> 15;
            out[i] += voice_amp(soft_knee(s + (s >> 2), 24000), m, i) << 1;
            if (v->s[6] == 2)
                break;
        }
        v->s[3] = (int32_t)((uint32_t)prev & 0xFFFFu) | cur << 16;
    }
    v->ph[1] = frac;
}

static void drum_note_on(track_t *t, voice_t *v)
{
    drum_lane_t *K = drum_kit_of(t), *L;
    uint32_t i = (uint32_t)(v - t->v), role, lane, uk = drum_user_kit(t->p);
    int32_t st;
    if (uk < SMP_USER_SLOTS) {                           /* EDDA OS: a user kit (the lane table untouched) */
        kit_note_on(t, v, uk);
        return;
    }
    v->s[6] = 0;
    if (!K || i >= NVOICE)
        return;
    role = drum_gm(t->p, v->note, &st);
    lane = drum_lane(v->note);
    L = &K[(uint32_t)v->s[0] & (DV_NLANE - 1u)];
    if (L->owner == i + 1u)                              /* this voice played another lane: it stops there */
        L->owner = 0;
    L = &K[lane];
    if (L->role != role || L->v.type != dv_run_type(role)) {   /* another drum on this lane: from rest */
        dv_init(&L->v, role);
        L->key.type = 0xFF;
        L->role = (uint8_t)role;
    }
    L->st = (int8_t)st;
    L->owner = (uint8_t)(i + 1u);                        /* (its last voice, if another, ends: drum_amp) */
    v->s[0] = (int32_t)lane;
    v->env_out = v->vel * 258;                           /* the hit starts at its level (no ramp from 0) */
    v->env = 1 << 24;                                    /* the ADSR held at full from here, not from drum_amp:
                                                          * a key-off before the first block (zero-length MIDI
                                                          * notes) would end a fresh voice at env 0 in env_tick */
    dv_trigger(&L->v);
    if (lane == DV_HATC && K[DV_HATO].v.live)            /* a closed hat chokes the open one */
        dv_choke(&K[DV_HATO].v);
}

/* the voice's amplitude: the hit, not the ADSR (which still runs, held at full so a release never ends
 * it). A voice that plays no lane any more, or whose drum has rung out, ends here */
static int32_t drum_amp(track_t *t, voice_t *v, int32_t adsr)
{
    const drum_lane_t *L;
    (void)adsr;
    if (!v->active)                                      /* (taken for another part: env_tick ended it) */
        return 0;
    if (v->s[6])                                         /* a user kit's voice: its own amplitude */
        return kit_amp(t, v);
    L = drum_lane_of(t, v);
    if (!L || (!L->v.live && !L->v.trig)) {
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = 0;
        return 0;
    }
    v->env = 1 << 24;
    return 32767;
}

static void drum_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    drum_lane_t *L = v->s[6] ? 0 : drum_lane_of(t, v);
    dv_param_t k;
    int32_t y[CTL], mb[CTL], acc = clamp(p[P_E5] * v->vel * 4, 0, 65536), drv = p[P_E7], g = 0, mk = 0;   /* acc Q16 */
    int32_t lv = clamp(p[P_LN0 + ((uint32_t)v->s[0] & (DV_NLANE - 1u))], 0, 127);
    uint32_t i, r;
    vmod_t ml;                                           /* (only its amplitude ramp is read: voice_amp) */
    if (v->s[6]) {                                       /* a user kit's voice */
        kit_render(t, v, out, n, m);
        return;
    }
    if (!L)
        return;
    if (lv < 127) {                                      /* the lane's LEVEL (square law) on the block's amplitude
                                                          * ramp, not per sample; 100 %: the ramp as it was */
        int32_t gl = lv * lv * 2 + (lv * lv >> 6);       /* Q15, 127: 32508 (not used), 64: 8256 */
        ml.amp0 = (m->amp0 >> 4) * gl >> 11;
        ml.amp1 = (m->amp1 >> 4) * gl >> 11;
        m = &ml;
    }
    if (n > CTL)
        n = CTL;
    r = L->role;
    dv_default(&k, r);                                   /* the knobs move every lane from its design (64) */
    k.decay = (uint8_t)clamp(k.decay + p[P_E3] - 64, 0, 127);
    k.tone = (uint8_t)clamp(k.tone + p[P_E2] - 64, 0, 127);
    k.extra = (uint8_t)clamp(k.extra + p[P_E4] - 64, 0, 127);
    k.accent = (uint8_t)clamp(acc >> 9, 0, 127);         /* (Q16 -> 0..127) */
    k.tune = (int16_t)(L->st * 16 + (p[P_E1] - 64) * 3);   /* +-12 semitones */
    if (((const uint32_t *)&k)[0] != ((const uint32_t *)&L->key)[0] ||
        ((const uint32_t *)&k)[1] != ((const uint32_t *)&L->key)[1]) {
        dv_setup(&L->c, &k);                             /* (a parameter moved) */
        L->key = k;
        if (dv_uses_metal(L->c.type))
            dv_metal_tune(&L->mb, &L->c);
    }
    if (dv_uses_metal(L->c.type) && (L->v.live || L->v.trig))
        dv_metal_run(&L->mb, mb, n);
    dv_run(&L->c, &L->v, mb, y, n);
    if (drv > 0) {                                       /* DRV: x1..x4 into the soft clip, the level kept (Q12) */
        g = 4096 + drv * 3 * 4096 / 127;
        mk = (int32_t)((19661u << 15) / (uint32_t)softclip((19661 * g) >> 12));
    }
    for (i = 0; i < n; i++) {                            /* x1.25 and the knee below, in 32 bits */
        int32_t s = y[i];
        if (g)
            s = (softclip((s * g) >> 12) * mk) >> 15;
        out[i] += voice_amp(soft_knee(s + (s >> 2), 24000), m, i) << 1;
    }
}

/* the GM drum map, the first C (key 7) is the kick (C2, 36) */
static int32_t drum_keys(const track_t *t, uint32_t k)
{
    (void)t;
    return clamp(29 + 12 * song.octave + (int32_t)k, 0, 127);
}

/* {KIT, TUNE, TONE, DECY, SNAP, ACC, KICK, DRV}; every kit suggests the BEAT pattern (GM notes) */
static const preset_t DRUM_PRESETS[] = {
    {"DRUM KIT", DRUM_KIT_E, {0, 100, 127, 100}, 0, 0, FX(0, 0, 0, 20), PAT(12)},   /* (core.h: SAMPLE PERC's too) */
};

static const engine_t ENG_DRUM = {
    .name = "DRUM",
    .page_title = {"KIT", "HIT"},
    .edit = {
        {"KIT", F_ENUM, 0, DK_COUNT - 1, DK_STD, N_DRUM_KIT, 0},
        {"TUNE", F_PCT, 0, 127, 64, 0, 0},
        {"TONE", F_PCT, 0, 127, 64, 0, 0},
        {"DECY", F_PCT, 0, 127, 64, 0, 0},
        {"SNAP", F_PCT, 0, 127, 64, 0, 0},
        {"ACC", F_PCT, 0, 127, 100, 0, 0},
        {"KICK", F_ENUM, 0, 1, 0, N_DRUM_KICK, 0},
        {"DRV", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = DRUM_PRESETS,
    .npresets = NELEM(DRUM_PRESETS),
    .note_on = drum_note_on,
    .render = drum_render,
    .amp = drum_amp,
    .knob = {P_E1, P_E2, P_E3, P_E4},
    .poly = DV_NLANE,
    .sampled = 1,                /* (EDDA OS, the user kits: a pad hit again starts over; the synth kit keeps no
                                  * phases in the voice anyway) */
    .oneshot = 1,
    .keys = drum_keys,
};
