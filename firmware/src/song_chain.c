/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* A song uses the sequences of four saved projects, with the current sounds.
 * Sources are copied in the main loop before PLAY. The ISR changes only their
 * index and five timing parameters; the editable steps stay untouched.
 *
 * Lanes (1.2): a section (a row) names the slot each track plays (core.h chain_row_t). Each track reads its steps (with
 * CHANCE, RATCH, NUDGE), its automation and locks, LEN / DIV / SWING / GATE and QUANTIZE from its own slot's copy for
 * that track only; all four slots in use are copied before PLAY (chain_prepare: the four sources were there already
 * for whole rows, so lanes cost no more RAM). Sections keep one boundary for all four tracks: a section lasts `repeat`
 * loops of its clock track (chain_clock): the first track that is not silent there (track 1 when it plays: every song
 * before lanes counts as it did), or, every track silent, a 4/4 bar (CHAIN_SIL_*). At each section all four tracks
 * start together from their step 1, as rows always did.
 * A silent track ("-", CHAIN_SILENT) plays nothing in that section: at its start the track's sequenced notes are
 * released (seq_release, as at every section) and its automation goes back to the sound's own values (motion_restore);
 * no step, automation or lock of any slot plays there. Live keys and the arp still play it (the sound stays). */
typedef struct {
    step_t step[NTRK][NSTEP];
    int16_t timing[NTRK][4];
    uint8_t qnt[NTRK];                       /* each track's QUANTIZE (P_SQNT) as saved */
    motion_store_t motion;
} chain_pattern_t;
static chain_config_t chain_config;
static struct {
    chain_pattern_t source[4];
    chain_config_t config;
    int16_t timing[NTRK][4];                 /* the editable LEN DIV SWING GATE and QUANTIZE, back on STOP */
    uint8_t qnt[NTRK];
    volatile uint8_t armed, running, row, remaining;
    volatile uint8_t cue;        /* EDDA OS: a section cued on the SONG page while playing, slot + 1 (0 none): at the
                                  * end of this pass the song goes on from its next row that plays it (chain_pass_end) */
    uint8_t lane[NTRK];                      /* the slot each track plays in this section, CHAIN_SILENT none */
    uint8_t clk;                             /* the track whose loop counts the section (chain_clock) */
    uint8_t rec;
    uint32_t carry;
} chain;
/* a silent lane: its steps (empty: nothing plays) and its timing (a 4/4 bar of 1/16: the section's length when every
 * track is silent) */
static const step_t chain_silent[NSTEP] = {{{0}}};
#define CHAIN_SIL_LEN 16
#define CHAIN_SIL_DIV 2                      /* 1/16 */

static void seq_release(track_t *t);
static void seq_stop(void);
static uint32_t div_samples(uint32_t div);
static uint32_t step_samples(const track_t *t, uint32_t period, uint32_t idx);
static uint32_t seq_len(const track_t *t, uint32_t period);

static chain_row_t chain_row_of(uint32_t slot, uint32_t repeat)   /* a section with all four tracks on one slot */
{
    chain_row_t r;
    memset(r.slot, (int)slot, sizeof r.slot);
    r.repeat = (uint8_t)repeat;
    return r;
}
static void chain_defaults(chain_config_t *c)
{
    uint32_t i;
    memset(c, 0, sizeof *c);
    for (i = 0; i < CHAIN_ROWS; i++)
        c->row[i].repeat = 1;
}
static int chain_valid(const chain_config_t *c)
{
    uint32_t i, k;
    if (c->count > CHAIN_ROWS)
        return 0;
    for (i = 0; i < c->count; i++) {
        for (k = 0; k < NTRK; k++)
            if (c->row[i].slot[k] > CHAIN_SILENT)
                return 0;
        if (!c->row[i].repeat || c->row[i].repeat > 16u)
            return 0;
    }
    return 1;
}
/* the chain of FUN6..FUN9 (rows {slot, repeat}) -> today's: each row a section with the four tracks on its slot. 0: not
 * a valid one (as chain_valid then) */
static int chain_from_v9(chain_config_t *c, const chain_v9_t *o)
{
    uint32_t i;
    chain_defaults(c);
    if (o->count > CHAIN_ROWS)
        return 0;
    for (i = 0; i < o->count; i++) {
        if (o->row[i].slot >= 4u || !o->row[i].repeat || o->row[i].repeat > 16u)
            return 0;
        c->row[i] = chain_row_of(o->row[i].slot, o->row[i].repeat);
    }
    c->count = o->count;
    return 1;
}
/* the track whose loops count section r's length: the first one not silent; every track silent: track 1 (a bar) */
static uint32_t chain_clock(const chain_row_t *r)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++)
        if (r->slot[k] < CHAIN_SILENT)
            return k;
    return 0;
}
/* a track parameter a playing song sets from its slots (the editor and the pages leave it alone meanwhile) */
static int chain_owns(uint32_t id) { return (id >= P_SLEN && id <= P_SGATE) || id == P_SQNT; }
static const step_t *seq_steps(const track_t *t)
{
    uint32_t k = (uint32_t)(t - trk), s;
    if (!chain.running)
        return t->step;
    s = chain.lane[k];
    return s < 4u ? chain.source[s].step[k] : chain_silent;
}
static void motion_restore(track_t *t);
static __attribute__((noinline)) void edda_section_cue(uint32_t slot);   /* (EDDA OS, edda.c: a section's show cue) */
/* EDDA OS: the section a row is, for the cues (SONG page, playing) and SHOW CUES: its clock track's slot (A..D: 0..3,
 * the first track not silent; every row of THE ARRIVAL's songs has all four on one), CHAIN_SILENT for an all-silent row */
static uint32_t chain_row_slot(const chain_row_t *r) { return r->slot[chain_clock(r)]; }
/* EDDA OS: the row a cue of section s leads to: the next one after the row playing that is s (round to the start),
 * CHAIN_ROWS = none (noinline: the audio ISR runs it once a cue's pass ends, never per sample) */
static __attribute__((noinline)) uint32_t chain_cue_row(uint32_t s)
{
    uint32_t k, n = chain.config.count;
    for (k = 1; k <= n; k++) {
        uint32_t q = (chain.row + k) % n;
        if (chain_row_slot(&chain.config.row[q]) == s)
            return q;
    }
    return CHAIN_ROWS;
}
/* a section starts (chain_start, chain_tick: once a section, not per block; not inlined: off the audio ISR's loop) */
static __attribute__((noinline)) void chain_apply(void)
{
    uint32_t i;
    const chain_row_t *r = &chain.config.row[chain.row];
    chain.clk = (uint8_t)chain_clock(r);
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        uint32_t s = r->slot[i];
        chain.lane[i] = (uint8_t)(s < 4u ? s : CHAIN_SILENT);
        seq_release(t);
        motion_restore(t);
        if (s < 4u) {
            memcpy(&t->p[P_SLEN], chain.source[s].timing[i], sizeof chain.timing[i]);
            t->p[P_SQNT] = chain.source[s].qnt[i];
        } else {
            t->p[P_SLEN] = CHAIN_SIL_LEN;
            t->p[P_SDIV] = CHAIN_SIL_DIV;
            t->p[P_SSWING] = 0;
            t->p[P_SQNT] = 1;
        }
        t->seq_idx = (uint16_t)(t->p[P_SLEN] - 1);
        t->seq_pos = 0x7FFFFFFFu;
        t->seq_base = 0;                        /* (EDDA OS: the exact grid from the slot's step 0) */
        t->rh_n = t->rskip_n = 0;
    }
    edda_section_cue(chain_row_slot(r));
}
static void chain_start(void)
{
    uint32_t i;
    if (!chain.armed)
        return;
    chain.rec = song.rec;
    song.rec = 0;
    for (i = 0; i < NTRK; i++) {
        memcpy(chain.timing[i], &trk[i].p[P_SLEN], sizeof chain.timing[i]);
        chain.qnt[i] = (uint8_t)trk[i].p[P_SQNT];
    }
    chain.row = 0;
    chain.remaining = chain.config.row[0].repeat;
    chain.carry = 0;
    chain.cue = 0;
    chain.running = 1;
    chain.armed = 0;
    chain_apply();
}
static void chain_stop(void)
{
    uint32_t i;
    chain.armed = 0;
    if (!chain.running)
        return;
    for (i = 0; i < NTRK; i++) {
        memcpy(&trk[i].p[P_SLEN], chain.timing[i], sizeof chain.timing[i]);
        trk[i].p[P_SQNT] = chain.qnt[i];
    }
    song.rec = chain.rec;
    chain.running = 0;
    chain.cue = 0;
}
/* the end of the clock track's pass (chain_clock), carry samples past it: the next pass, the next row, a cued section's row (EDDA OS: a cue
 * on the last pass of the song plays on instead of ending), or the song's end. EDDA OS: noinline (with chain_apply),
 * as it runs once a pass: its code stays out of the audio block's loops (tests/target_budget.py) */
static __attribute__((noinline)) void chain_pass_end(uint32_t carry)
{
    uint32_t r = CHAIN_ROWS;
    if (chain.cue) {
        r = chain_cue_row(chain.cue - 1u);
        chain.cue = 0;
    }
    if (r >= CHAIN_ROWS) {
        if (chain.remaining > 1u) {
            chain.remaining--;
            return;
        }
        if (chain.row + 1u >= chain.config.count) {
            seq_stop();
            return;
        }
        r = chain.row + 1u;
    }
    chain.carry = carry;
    chain.row = (uint8_t)r;
    chain.remaining = chain.config.row[r].repeat;
    chain_apply();
}
static void chain_tick(uint32_t n)
{
    const track_t *t = &trk[chain.clk % NTRK];
    uint32_t length;
    if (!chain.running || t->seq_pos >= 0x7FFFFFFFu || t->seq_idx + 1u != (uint32_t)t->p[P_SLEN])
        return;
    length = seq_len(t, div_samples((uint32_t)t->p[P_SDIV]));   /* (EDDA OS: the step as the sequencer counts it) */
    if (t->seq_pos + n < length)
        return;
    chain_pass_end(t->seq_pos + n - length);
}
