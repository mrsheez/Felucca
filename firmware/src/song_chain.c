/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* A song uses the sequences of four saved projects, with the current sounds.
 * Sources are copied in the main loop before PLAY. The ISR changes only their
 * index and four timing parameters; the editable steps stay untouched. */
typedef struct {
    step_t step[NTRK][NSTEP];
    int16_t timing[NTRK][4];
    motion_store_t motion;
} chain_pattern_t;
static chain_config_t chain_config;
static struct {
    chain_pattern_t source[4];
    chain_config_t config;
    int16_t timing[NTRK][4];
    volatile uint8_t armed, running, row, remaining;
    volatile uint8_t cue;        /* EDDA OS: a section cued on the SONG page while playing, slot + 1 (0 none): at the
                                  * end of this pass the song goes on from its next row that plays it (chain_tick) */
    uint8_t slot, rec;
    uint32_t carry;
    int32_t carry_n;             /* EDDA OS, micro timing: track 1's step-1 nudge where the slot ended (samples): carry
                                  * counts from that nudged boundary, the new slot's grid from the plain one */
} chain;

static void seq_release(track_t *t);
static void seq_stop(void);
static uint32_t div_samples(uint32_t div);
static uint32_t step_samples(const track_t *t, uint32_t period, uint32_t idx);
static uint32_t seq_len(const track_t *t, uint32_t period);

static void chain_defaults(chain_config_t *c)
{
    uint32_t i;
    memset(c, 0, sizeof *c);
    for (i = 0; i < CHAIN_ROWS; i++)
        c->row[i].repeat = 1;
}
static int chain_valid(const chain_config_t *c)
{
    uint32_t i;
    if (c->count > CHAIN_ROWS)
        return 0;
    for (i = 0; i < c->count; i++)
        if (c->row[i].slot >= 4u || !c->row[i].repeat || c->row[i].repeat > 16u)
            return 0;
    return 1;
}
static const step_t *seq_steps(const track_t *t)
{
    return chain.running ? chain.source[chain.slot].step[t - trk] : t->step;
}
static void motion_restore(track_t *t);
/* EDDA OS: the row a cue of section s leads to: the next one after the row playing that plays s (round to the
 * start), CHAIN_ROWS = none */
static uint32_t chain_cue_row(uint32_t s)
{
    uint32_t k, n = chain.config.count;
    for (k = 1; k <= n; k++) {
        uint32_t q = (chain.row + k) % n;
        if (chain.config.row[q].slot == s)
            return q;
    }
    return CHAIN_ROWS;
}
static void chain_apply(void)
{
    uint32_t i;
    chain.slot = chain.config.row[chain.row].slot;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        seq_release(t);
        motion_restore(t);
        memcpy(&t->p[P_SLEN], chain.source[chain.slot].timing[i], sizeof chain.timing[i]);
        t->seq_idx = (uint16_t)(t->p[P_SLEN] - 1);
        t->seq_pos = 0x7FFFFFFFu;
        t->seq_base = 0;                        /* (EDDA OS: the exact grid from the slot's step 0) */
        t->rh_n = t->rskip_n = 0;
    }
}
static void chain_start(void)
{
    uint32_t i;
    if (!chain.armed)
        return;
    chain.rec = song.rec;
    song.rec = 0;
    for (i = 0; i < NTRK; i++)
        memcpy(chain.timing[i], &trk[i].p[P_SLEN], sizeof chain.timing[i]);
    chain.row = 0;
    chain.remaining = chain.config.row[0].repeat;
    chain.carry = 0;
    chain.carry_n = 0;
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
    for (i = 0; i < NTRK; i++)
        memcpy(&trk[i].p[P_SLEN], chain.timing[i], sizeof chain.timing[i]);
    song.rec = chain.rec;
    chain.running = 0;
    chain.cue = 0;
}
static void chain_tick(uint32_t n)
{
    const track_t *t = &trk[0];
    uint32_t length;
    if (!chain.running || t->seq_pos >= 0x7FFFFFFFu || t->seq_idx + 1u != (uint32_t)t->p[P_SLEN])
        return;
    length = seq_len(t, div_samples((uint32_t)t->p[P_SDIV]));   /* (EDDA OS: the step as the sequencer counts it) */
    if (t->seq_pos + n < length)
        return;
    if (chain.cue) {                                    /* EDDA OS: a section cued: this pass was the last (a cue
                                                         * on the last pass of the song plays on instead of ending) */
        uint32_t r = chain_cue_row(chain.cue - 1u);
        chain.cue = 0;
        if (r < CHAIN_ROWS) {
            chain.carry = t->seq_pos + n - length;
            chain.carry_n = step_nudge(&seq_steps(t)[0]) * (int32_t)(div_samples((uint32_t)t->p[P_SDIV]) / 16u);
            chain.row = (uint8_t)r;
            chain.remaining = chain.config.row[r].repeat;
            chain_apply();
            return;
        }
    }
    if (chain.remaining > 1u) {
        chain.remaining--;
        return;
    }
    if (chain.row + 1u >= chain.config.count) {
        seq_stop();
        return;
    }
    chain.carry = t->seq_pos + n - length;
    chain.carry_n = step_nudge(&seq_steps(t)[0]) * (int32_t)(div_samples((uint32_t)t->p[P_SDIV]) / 16u);   /* (the
                                                       * old slot's steps: chain.slot moves in chain_apply) */
    chain.row++;
    chain.remaining = chain.config.row[chain.row].repeat;
    chain_apply();
}
