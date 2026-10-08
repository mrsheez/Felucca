/* SPDX-License-Identifier: GPL-3.0-only
 * EDDA OS: THE ARRIVAL, the first thirteen songs of EDDA, held as music the FM-1 plays (tools/gen_arrival.py writes
 * build/gen/edda_arrival.h from tools/arrival_songs.py). Included by project.c (it loads like a project).
 *
 *   SAVE > ARRIVAL    the list (KNOB 1), OCT+ LOAD, PLAY: load (if it is not the music already) and play the song
 *   a song           four tracks (the sounds: an engine's factory preset and parameter overrides; the kit is DRUM KIT
 *                    EDDA, the log drum SAMPLE SET LOG), the globals (tempo, delay, reverb), up to four sections A..D
 *                    (each track's steps, LEN and DIV, the section's automation) and the arrangement (the SONG page's
 *                    rows: a section and its repeats)
 *   loaded           the sounds and section A's steps are the tracks' (play them, edit them, save them as a project);
 *                    the SONG page's A..D are the song's sections (arv_cur: chain_prepare reads them from here, not
 *                    from the project slots), named there; TOOLS > CLEAR SONG ends that. A project saved after a load
 *                    keeps it (project_t.arv: its rows still play the song's sections, with the sounds as saved)
 *
 * The data (edda_arrival.h): one byte stream, ARV_DATA, read through offsets (0: none):
 *   bar       16 steps, an event per step that holds something: b0 = step | TIE << 4 | EXT << 5 | DRUM << 6 (TIE: that
 *             is all); a NOTE: b1 = vel5 | n << 5 (velocity vel5 * 4 + 3), a DRUM bar's hit and accent bytes, the n
 *             notes, then with EXT the flags and the chance; 0xFF ends the bar. ARV_BAR[id]: where bar id starts
 *   pattern   [n] then n x (bar id lo, hi, transposition): up to four bars, the melodic notes transposed
 *   params    [n] then n x (id, value + 64): a track's sound over its preset (P_*), the song's globals (G_*)
 *   motion    [n] then n x (track << 6 | step, id, value + 64): a section's automation (motion_event_t) */
typedef struct {
    uint8_t engine, preset;      /* the base sound: the engine's factory preset */
    uint16_t par;                /* its overrides (params), 0 none */
} arv_snd_t;
typedef struct {
    uint16_t pat[NTRK];          /* each track's steps (a pattern), 0 none */
    uint8_t len[NTRK], div[NTRK];
    uint16_t mot;                /* the section's automation, 0 none */
    char name[7];                /* "INTRO", "DROP" (the SONG page) */
} arv_sec_t;
typedef struct {
    char name[20], shrt[13], chant[11], line[24];   /* the title (SAVE > ARRIVAL), the music's name once loaded (a
                                                   * project's 12), the Igbo chant word, the line it opens on */
    uint8_t bpm, cam, nsec, nrow, half;   /* cam: the Camelot key (edda.h: 1 1A .. 24 12B); half: a half-time feel */
    uint16_t glob;                        /* global overrides (params: G_* ids) */
    arv_snd_t snd[NTRK];
    arv_sec_t sec[4];
    chain_row_t row[CHAIN_ROWS];
} arv_song_t;
#include "edda_arrival.h"

static uint8_t arv_cur;          /* the song the music was loaded from + 1, 0 = none (the SONG page: its sections) */

/* bar id into st[0..15] (cleared before: RESTs), its notes transposed by tr (a DRUM bar's notes are pads: as they are) */
static void arv_bar(uint32_t id, int32_t tr, step_t *st)
{
    const uint8_t *b = &ARV_DATA[ARV_BAR[id < ARV_NBARS ? id : 0u]];
    while (*b != 0xFFu) {
        uint32_t h = *b++, v, n, k;
        step_t *s = &st[h & 15u];
        if (h & 0x10u) {
            s->time = ST_TIE;
            continue;
        }
        v = *b++;
        n = v >> 5;
        s->time = ST_NOTE;
        s->vel = (uint8_t)((v & 31u) << 2 | 3u);
        if (h & 0x40u) {
            s->hit = *b++;
            s->acc = (uint8_t)(*b++ & s->hit);
        }
        for (k = 0; k < n; k++) {
            int32_t x = (int32_t)*b++ + (h & 0x40u ? 0 : tr);
            if (k < 4u)
                s->note[k] = (uint8_t)clamp(x, 0, 127);
        }
        s->n = (uint8_t)(n < 4u ? n : 4u);
        if (h & 0x20u) {
            s->flags = (uint8_t)(*b++ & (3u | SF_RATCH | SF_EDDA));
            s->probability = *b++;
            if (s->probability > 101u)
                s->probability = 0;
        }
    }
}

/* the pattern at ARV_DATA[off] into st[0..NSTEP) (0: none, every step a REST) */
static void arv_pattern(uint32_t off, step_t *st)
{
    uint32_t i, n;
    for (i = 0; i < NSTEP; i++) {
        memset(&st[i], 0, sizeof st[i]);
        st[i].time = ST_REST;
    }
    if (!off)
        return;
    n = ARV_DATA[off];
    for (i = 0; i < n && i < NSTEP / 16u; i++) {
        const uint8_t *r = &ARV_DATA[off + 1u + 3u * i];
        arv_bar((uint32_t)r[0] | (uint32_t)r[1] << 8, (int8_t)r[2], st + 16u * i);
    }
}

/* a params list's value for id (track params: P_*), def when it has none */
static int32_t arv_param(uint32_t off, uint32_t id, int32_t def)
{
    uint32_t i, n = off ? ARV_DATA[off] : 0u;
    for (i = 0; i < n; i++)
        if (ARV_DATA[off + 1u + 2u * i] == id)
            def = (int32_t)ARV_DATA[off + 2u + 2u * i] - 64;
    return def;
}

/* section s of song n as a chain source (song_chain.c): every track's steps, LEN DIV (SWING none: the EDDA rule; the
 * track's GATE), the section's automation. A section the song does not have: silent, a bar long */
static void arv_section(uint32_t n, uint32_t s, chain_pattern_t *d)
{
    const arv_song_t *sg = &ARV_SONGS[n % ARV_NSONGS];
    const arv_sec_t *sc = &sg->sec[s & 3u];
    uint32_t k, i, have = s < sg->nsec, m;
    memset(&d->motion, 0, sizeof d->motion);
    for (k = 0; k < NTRK; k++) {
        arv_pattern(have ? sc->pat[k] : 0u, d->step[k]);
        d->timing[k][0] = (int16_t)(have ? clamp(sc->len[k], 1, NSTEP) : 16);
        d->timing[k][1] = (int16_t)(have ? clamp(sc->div[k], TP[P_SDIV].min, TP[P_SDIV].max) : TP[P_SDIV].def);
        d->timing[k][2] = 0;
        d->timing[k][3] = (int16_t)clamp(arv_param(sg->snd[k].par, P_SGATE, TP[P_SGATE].def), TP[P_SGATE].min,
                                         TP[P_SGATE].max);
    }
    m = have ? sc->mot : 0u;
    if (m) {
        uint32_t cnt = ARV_DATA[m] < MOTION_MAX ? ARV_DATA[m] : MOTION_MAX;
        for (i = 0; i < cnt; i++) {
            motion_event_t *e = &d->motion.event[d->motion.count];
            e->place = ARV_DATA[m + 1u + 3u * i];
            e->param = ARV_DATA[m + 2u + 3u * i];
            e->value = (int16_t)((int32_t)ARV_DATA[m + 3u + 3u * i] - 64);
            if (!motion_param(MOTION_ID(e)))
                continue;
            d->motion.on |= (uint8_t)(1u << (e->place >> 6));
            d->motion.count++;
        }
    }
}

static uint32_t arv_count(void) { return ARV_NSONGS; }
static const arv_song_t *arv_song(uint32_t n) { return &ARV_SONGS[n % ARV_NSONGS]; }
static uint32_t arv_bpm(uint32_t n) { return arv_song(n)->bpm; }
static uint32_t arv_cam(uint32_t n) { return arv_song(n)->cam; }   /* (edda.c edda_cam_name: "8A") */
/* SAVE > ARRIVAL's row of song n: its title (nlen bytes with the 0) and its tempo and key, "116 8A" (8 bytes) */
static void arv_label(uint32_t n, char *name, uint32_t nlen, char *info)
{
    const arv_song_t *sg = arv_song(n);
    uint32_t k;
    str_cpy(name, sg->name, nlen);
    fmt_int(info, (int32_t)sg->bpm);
    k = str_len(info);
    info[k++] = ' ';
    edda_cam_name(sg->cam, info + k);
}
/* the SONG page's name of section s: the song's, when the music is the song's ("" none) */
static const char *arv_sec_name(uint32_t s)
{
    if (!arv_cur || arv_cur > ARV_NSONGS || s >= ARV_SONGS[arv_cur - 1u].nsec)
        return "";
    return ARV_SONGS[arv_cur - 1u].sec[s].name;
}

/* the globals a song sets (the sound of the room, the tempo); the others (the clock, MIDI, the tuning) are the
 * player's: a song never changes them */
static int arv_glob_ok(uint32_t id)
{
    return id == G_SWING || (id >= G_DTIME && id <= G_CDEPTH) || id == G_RTYPE;
}

/* main loop: song n into the music, as a project loads (project_restore_runtime): the transport stops, every track
 * gets its sound (the preset, then the song's values) and section A's steps, the arrangement becomes the SONG page's
 * rows, the tempo and the room the song's. The name of the music is the song's */
static void arv_load(uint32_t n)
{
    const arv_song_t *sg = arv_song(n);
    chain_pattern_t *a = &chain.source[0];              /* (the chain's: stopped, it is free; a chain play fills it) */
    uint32_t i, k;
    transport_req = 2;
    panic_req = (1u << NTRK) - 1u;
    fm1_irq_off();                                      /* the audio ISR must not see half a song */
    seq_stop();
    transport_req = 0;
    chain.armed = 0;
    arv_section(n, 0, a);                               /* section A: the tracks' steps, LEN DIV, the automation */
    memset(&chain_config, 0, sizeof chain_config);
    chain_config.count = (uint8_t)(sg->nrow < CHAIN_ROWS ? sg->nrow : CHAIN_ROWS);
    for (i = 0; i < CHAIN_ROWS; i++) {
        chain_config.row[i].slot = (uint8_t)(i < chain_config.count ? sg->row[i].slot & 3u : 0u);
        chain_config.row[i].repeat = (uint8_t)(i < chain_config.count ? clamp(sg->row[i].repeat, 1, 16) : 1);
    }
    motion = a->motion;
    memset(motion_active, 0, sizeof motion_active);
    motion_base_valid = 0;
    ui.song_row = 0;
    song.g[G_BPM] = (int16_t)clamp(sg->bpm, GP[G_BPM].min, GP[G_BPM].max);
    for (i = 0; i < G_COUNT; i++)
        if (arv_glob_ok(i))
            song.g[i] = (int16_t)clamp(arv_param(sg->glob, i, GP[i].def), GP[i].min, GP[i].max);
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        const arv_snd_t *sd = &sg->snd[k];
        uint32_t e = sd->engine < NENGINES && eng_ok(sd->engine) ? sd->engine : 0u;
        const engine_t *en = ENGINES[e];
        const preset_t *pr = en->npresets ? &en->presets[sd->preset % en->npresets] : 0;
        static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
        uint32_t np = sd->par ? ARV_DATA[sd->par] : 0u;
        t->eng_req = (uint8_t)e;
        t->preset = (uint8_t)(pr ? sd->preset % en->npresets : 0u);
        t->user = 0;
        for (i = 0; i < P_COUNT; i++)
            t->p[i] = param_desc_of(e, i)->def;
        if (pr) {
            for (i = 0; i < 8u; i++)
                t->p[P_E0 + i] = pr->e[i];
            t->p[P_ATK] = pr->env[0];
            t->p[P_DEC] = pr->env[1];
            t->p[P_SUS] = pr->env[2];
            t->p[P_REL] = pr->env[3];
            t->p[P_ED_FLT] = pr->fenv;
            t->p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;
            for (i = 0; i < 4u; i++)
                t->p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
        }
        for (i = 0; i < np; i++) {
            uint32_t id = ARV_DATA[sd->par + 1u + 2u * i];
            if (id < P_COUNT)
                t->p[id] = (int16_t)((int32_t)ARV_DATA[sd->par + 2u + 2u * i] - 64);
        }
        memcpy(&t->p[P_SLEN], a->timing[k], sizeof a->timing[k]);
        for (i = 0; i < P_COUNT; i++)                    /* every value inside its range */
            t->p[i] = (int16_t)param_fit(param_desc_of(e, i), t->p[i]);
        memcpy(t->step, a->step[k], sizeof t->step);
        fm6_track_loaded(t);                            /* FM6: the patch its SLOT names */
    }
    song.sel = 0;
    edda.camelot = (uint8_t)(sg->cam <= 24u ? sg->cam : 0u);
    fm1_irq_on();
    arv_cur = (uint8_t)(n % ARV_NSONGS + 1u);
    ui.bpick = (uint8_t)(n % ARV_NSONGS);               /* (SAVE > ARRIVAL: the list on the song loaded) */
    str_cpy(proj_name, sg->shrt, sizeof proj_name);
    proj_cur = PROJ_NO_SLOT;
    undo.trk = 0;                                       /* (ui.c) the undo copy belongs to the music before */
    for (k = 0; k < NTRK; k++) {
        pat_sig[k] = ~steps_sig(&trk[k]);               /* (the song's steps: a pattern load asks first) */
        pat_last[k] = 0;
    }
    sync_reload = 1;
    ui.force = 1;
    ui_say("LOADED ", sg->name);
    str_cpy(ui.msg2, sg->line, sizeof ui.msg2);         /* then the line the song opens on: the voice's cue */
    edda.song = arv_cur;                                /* (SHOW CUES: CC 27 the song, CC 24 its key) */
    edda_ui(ED_RQ_CUE_SONG);
    memset(snd_said, 0, sizeof snd_said);
}
