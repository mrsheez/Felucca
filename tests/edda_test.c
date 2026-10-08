/* SPDX-License-Identifier: GPL-3.0-only */
/* EDDA OS (firmware/src/edda.c) on the host, against the real sequencer, UI and MIDI-out code:
 * the Camelot wheel (every position's root and scale, names, neighbours, the key path), the EDDA pattern bank
 * (lengths, divisions, the drum grids, melodic lines loaded into ROOT, triads), Euclidean rhythms (E(7, 12) is the
 * ogene timeline), the run (phases on the beat clock, the lane mask, the stabs' ramp, the restore at the drop, the
 * cues), the hard stop and the re-entry on the one, REVEAL (the hole every 8th bar, the backbeat's lane in bars
 * 17..24), MUTATE (the bell lane only), MENU > EDDA, the GLO layer's keys, and the FM6 voice bank.
 * The user drum kits (eng_drum.c KIT USR1..4) on a kit built by tests/edda_kit.py (argv[1]: its prefix; without
 * it those checks are skipped): the pads, the GM fall-back, TUNE DECY SNAP, the choke, the lane LEVELs, one
 * voice per pad, an empty kit, and the 4th slot (USR4: the kit from it, SAMPLE SET 8 on it, its flash address).
 * Run by tests/run_tests.sh. */
#include <stdio.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("edda: %-78s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void reset(void)
{
    ui_power_on();
    usb.config = 1;
    edda_defaults();
    edda.cues = 1;                                    /* (the default is OFF; most checks read the cues) */
    song.g[G_BPM] = 120;                              /* a beat 22050 samples, a bar 88200 */
    mo_w = mo_r = 0;
}
static void blocks(uint32_t n) { while (n--) events_block(CTL); }
static void to_next_bar(void)                         /* run until the sequencer passes the one */
{
    uint32_t b = edda.bar, n = 0;
    while (edda.bar == b && n++ < 4000u)
        blocks(1);
}
static void beats(uint32_t n)                         /* n beats of blocks, CTL at a time */
{
    uint32_t s = n * beat_samples();
    blocks((s + CTL - 1u) / CTL);
}
static uint32_t cues_cc(uint32_t from, uint32_t cc, uint32_t *last)   /* the cue CCs since from, the last value */
{
    uint32_t i, n = 0;
    for (i = from; i < mo_w; i++) {
        uint32_t p = midi_out_q[i % MQ];
        if (((p >> 8) & 0xFFu) == (0xB0u | ED_CUE_CH) && ((p >> 16) & 0x7Fu) == cc) {
            n++;
            if (last)
                *last = (p >> 24) & 0x7Fu;
        }
    }
    return n;
}
static uint32_t cues_note(uint32_t from, uint32_t note)
{
    uint32_t i, n = 0;
    for (i = from; i < mo_w; i++) {
        uint32_t p = midi_out_q[i % MQ];
        if (((p >> 8) & 0xFFu) == (0x90u | ED_CUE_CH) && ((p >> 16) & 0x7Fu) == note)
            n++;
    }
    return n;
}
static uint32_t popcount(uint32_t m) { uint32_t n = 0; while (m) { n += m & 1u; m >>= 1; } return n; }
/* ---- the user kits: helpers */
static long load_file(const char *path, void *dst, long max)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f)
        return -1;
    n = (long)fread(dst, 1, (size_t)max, f);
    fclose(f);
    return n;
}
static int kit_load(const char *prefix, uint32_t slot)   /* the built kit (.hdr / .bin) into user slot k, scanned */
{
    char path[512];
    uint8_t *img = (uint8_t *)host_slots + slot * SMP_USER_SIZE;
    long n;
    memset(img, 0, SMP_USER_SIZE);
    snprintf(path, sizeof path, "%s.hdr", prefix);
    n = load_file(path, img, SMP_USER_DATA);
    if (n != (long)sizeof(smp_user_hdr_t))
        return 0;
    snprintf(path, sizeof path, "%s.bin", prefix);
    n = load_file(path, img + SMP_USER_DATA, SMP_USER_SIZE - SMP_USER_DATA);
    smp_user_scan(slot);
    return n > 0 && usr_nz[slot] == 16u;
}
static voice_t *kvoice(track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].note == note)
            return &t->v[i];
    return 0;
}
static uint32_t kvoices(track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active != 0;
    return n;
}
static uint32_t render_peak(uint32_t nblocks)          /* the peak of the next blocks of the mix */
{
    uint32_t i, k, pk = 0;
    for (k = 0; k < nblocks; k++) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            pk = (uint32_t)abs(o[2 * i]) > pk ? (uint32_t)abs(o[2 * i]) : pk;
    }
    return pk;
}
static uint32_t blocks_until_end(track_t *t, uint32_t note, uint32_t max)   /* blocks until the pad's voice ends */
{
    uint32_t k;
    for (k = 0; k < max; k++) {
        int32_t o[2 * CTL];
        if (!kvoice(t, note))
            return k;
        mix_block(o, CTL);
    }
    return max;
}
static void voices_off(void)                        /* nothing sounding */
{
    uint32_t i, k;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            trk[k].v[i].active = 0;
}
static void settle(void)                            /* nothing sounding, the effects' tails gone */
{
    uint32_t k;
    voices_off();
    for (k = 0; k < 400u; k++) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
    }
}
static void kit_track(track_t *t, int32_t kit)      /* a DRUM track on user kit `kit` (DK_USR1..), the defaults */
{
    uint32_t i;
    voices_off();
    song.master_q12 = 4096;                          /* (the MASTER knob: 0 under ui_test's stubs) */
    host_preset(t, ENGI_DRUM, 0);
    t->p[P_E0] = (int16_t)kit;
    for (i = 0; i < 4u; i++)
        t->p[P_DIST + i] = 0;
}
static const uint8_t KIT_PAD_NOTES[16] = {36, 38, 39, 42, 46, 45, 37, 56, 41, 43, 47, 48, 49, 50, 51, 53};
/* ---- micro timing: the blocks at which track t's voices start (by their age), over nblocks of the sequencer */
#define MT_MAX 96u
static uint32_t mt_blk[MT_MAX], mt_note[MT_MAX], mt_n;
static void mt_run(const track_t *t, uint32_t nblocks)
{
    uint32_t b, i, top = 0;
    mt_n = 0;
    for (i = 0; i < NVOICE; i++)
        top = t->v[i].age > top ? t->v[i].age : top;
    for (b = 0; b < nblocks; b++) {
        uint32_t nt = top;
        events_block(CTL);
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].age > top) {
                if (mt_n < MT_MAX) {
                    mt_blk[mt_n] = b;
                    mt_note[mt_n] = t->v[i].note;
                    mt_n++;
                }
                nt = t->v[i].age > nt ? t->v[i].age : nt;
            }
        top = nt;
    }
}
static int32_t mt_at(uint32_t note, uint32_t nth)     /* the block of note's nth start, -1: none */
{
    uint32_t i, k = 0;
    for (i = 0; i < mt_n; i++)
        if (mt_note[i] == note && k++ == nth)
            return (int32_t)mt_blk[i];
    return -1;
}
static void mt_track(track_t *t, uint32_t base, const int8_t *nudge)   /* 4 steps at 1/16: base +0 +2 +4 +5 */
{
    static const uint8_t IV[4] = {0, 2, 4, 5};
    uint32_t i;
    track_defaults_steps(t);
    t->p[P_SLEN] = 4; t->p[P_SDIV] = 2; t->p[P_SSWING] = 0; t->p[P_AMODE] = 0; t->p[P_SGATE] = 64;
    for (i = 0; i < 4u; i++) {
        t->step[i] = (step_t){{(uint8_t)(base + IV[i]), 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
        step_set_nudge(&t->step[i], nudge ? nudge[i] : 0);
    }
}
/* ---- the exact grid: the block each beat's note starts in, per track (0..2), and EDDA's beat changes (3) */
#define GRID_MAX 1400u
static uint32_t grid_blk[4][GRID_MAX], grid_n[4];
static void grid_run(uint32_t nblocks)
{
    uint32_t b, i, k, top[3] = {0, 0, 0}, eb = edda.beat;
    for (k = 0; k < 4u; k++)
        grid_n[k] = 0;
    for (k = 0; k < 3u; k++)
        for (i = 0; i < NVOICE; i++)
            top[k] = trk[k].v[i].age > top[k] ? trk[k].v[i].age : top[k];
    for (b = 0; b < nblocks; b++) {
        events_block(CTL);
        for (k = 0; k < 3u; k++) {
            uint32_t nt = top[k];
            for (i = 0; i < NVOICE; i++)
                if (trk[k].v[i].age > top[k]) {
                    if (grid_n[k] < GRID_MAX && (!grid_n[k] || grid_blk[k][grid_n[k] - 1u] != b))
                        grid_blk[k][grid_n[k]++] = b;
                    nt = trk[k].v[i].age > nt ? trk[k].v[i].age : nt;
                }
            top[k] = nt;
        }
        if (edda.beat != eb || (!b && edda.playing)) {
            if (grid_n[3] < GRID_MAX)
                grid_blk[3][grid_n[3]++] = b;
            eb = edda.beat;
        }
    }
}
static void grid_track(track_t *t, uint32_t div, uint32_t len, uint32_t every, uint32_t note)   /* a note on every beat */
{
    uint32_t i;
    track_defaults_steps(t);
    t->p[P_SLEN] = (int16_t)len; t->p[P_SDIV] = (int16_t)div; t->p[P_SSWING] = 0; t->p[P_AMODE] = 0; t->p[P_SGATE] = 32;
    for (i = 0; i < len; i += every)
        t->step[i] = (step_t){{(uint8_t)note, 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
}
static void mt_quiet(void)                            /* the other tracks: empty, LEN 4 */
{
    uint32_t i;
    for (i = 1; i < NTRK; i++) {
        track_defaults_steps(&trk[i]);
        trk[i].p[P_SLEN] = 4;
    }
    song.g[G_SWING] = 0;
}
static int has_note(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < t->seq_n; i++) if (t->seq_notes[i] == note) return 1;
    return 0;
}

/* ---- SEQ OUT: USB MIDI OUT as a host reads it (drained after every block), each message with its block */
#define MO_MAX 65536u
static uint32_t mo_log[MO_MAX], mo_blk[MO_MAX], mo_n, mo_b;
static uint32_t st_blk[8192], st_idx[8192], st_n;       /* the blocks track 0 moved to a step in, and the step */
static void mo_reset(void) { mo_r = mo_w; mo_n = mo_b = 0; st_n = 0; }
static void mo_drain(void)
{
    while (mo_r != mo_w) {
        if (mo_n < MO_MAX) {
            mo_log[mo_n] = midi_out_q[mo_r % MQ];
            mo_blk[mo_n++] = mo_b;
        }
        mo_r++;
    }
}
static void mo_run(uint32_t nblocks)
{
    while (nblocks--) {
        uint32_t i0 = trk[0].seq_idx, p0 = trk[0].seq_pos;
        events_block(CTL);
        if (song.playing && (trk[0].seq_idx != i0 || p0 >= 0x7FFFFFFFu) && trk[0].seq_pos < 0x7FFFFFFFu && st_n < 8192u) {
            st_blk[st_n] = mo_b;
            st_idx[st_n++] = trk[0].seq_idx;
        }
        mo_drain();
        mo_b++;
    }
}
static uint32_t mo_st(uint32_t i) { return (mo_log[i] >> 8) & 0xFFu; }
static uint32_t mo_d1(uint32_t i) { return (mo_log[i] >> 16) & 0x7Fu; }
static uint32_t mo_d2(uint32_t i) { return (mo_log[i] >> 24) & 0x7Fu; }
static int mo_on(uint32_t i) { return (mo_st(i) & 0xF0u) == 0x90u && mo_d2(i); }
static int mo_off(uint32_t i) { return (mo_st(i) & 0xF0u) == 0x80u || ((mo_st(i) & 0xF0u) == 0x90u && !mo_d2(i)); }
/* every note-on followed by its note-off, never on twice, nothing left on, the cue channel aside: the note-ons, -1 broken */
static int32_t mo_pairs(void)
{
    static uint8_t on[16][128];
    uint32_t i, c, x, ons = 0;
    memset(on, 0, sizeof on);
    for (i = 0; i < mo_n; i++) {
        c = mo_st(i) & 15u;
        x = mo_d1(i);
        if (mo_st(i) >= 0xF0u || c == ED_CUE_CH)
            continue;
        if (mo_on(i)) {
            if (on[c][x])
                return -1;
            on[c][x] = 1;
            ons++;
        } else if (mo_off(i)) {
            if (!on[c][x])
                return -1;
            on[c][x] = 0;
        }
    }
    for (c = 0; c < 16u; c++)
        for (x = 0; x < 128u; x++)
            if (on[c][x])
                return -1;
    return (int32_t)ons;
}
static uint32_t mo_count(uint32_t st)                 /* the messages of status st (0xF8, 0x90 | ch: its note-ons..) */
{
    uint32_t i, n = 0;
    for (i = 0; i < mo_n; i++)
        n += mo_st(i) == st && ((st & 0xF0u) != 0x90u || mo_d2(i));
    return n;
}
/* channel ch's notes as text, "+60/90 -60 .." (on / velocity, off), at most max characters */
static void mo_text(uint32_t ch, char *out, uint32_t max)
{
    uint32_t i, n = 0;
    out[0] = 0;
    for (i = 0; i < mo_n && n + 12u < max; i++) {
        if ((mo_st(i) & 15u) != ch || mo_st(i) >= 0xF0u)
            continue;
        if (mo_on(i))
            n += (uint32_t)snprintf(out + n, max - n, "%s+%u/%u", n ? " " : "", mo_d1(i), mo_d2(i));
        else if (mo_off(i))
            n += (uint32_t)snprintf(out + n, max - n, "%s-%u", n ? " " : "", mo_d1(i));
    }
}

int main(int argc, char **argv)
{
    uint32_t i, j, k, n[3], m0;
    char nm[4];
    const char *kit = argc > 1 ? argv[1] : 0;
    /* ------------------------------------------------------------- Camelot */
    {
        static const uint8_t RA[12] = {8, 3, 10, 5, 0, 7, 2, 9, 4, 11, 6, 1};   /* 1A..12A: G#m Ebm Bbm Fm Cm Gm Dm Am Em Bm F#m C#m */
        static const uint8_t RB[12] = {11, 6, 1, 8, 3, 10, 5, 0, 7, 2, 9, 4};   /* 1B..12B: B F# Db Ab Eb Bb F C G D A E */
        int ok = 1;
        for (i = 1; i <= 12u; i++) {
            ok &= edda_cam_root(i * 2u - 1u) == RA[i - 1u] && edda_cam_scale(i * 2u - 1u) == ED_SC_MIN;
            ok &= edda_cam_root(i * 2u) == RB[i - 1u] && edda_cam_scale(i * 2u) == ED_SC_MAJ;
            ok &= edda_cam_of(RA[i - 1u], ED_SC_MIN) == i * 2u - 1u && edda_cam_of(RB[i - 1u], ED_SC_MAJ) == i * 2u;
        }
        ck("Camelot wheel: 1A..12B -> the right root and scale, and back (A minor is 8A, C major 8B)", ok);
        edda_cam_name(15, nm); ok = str_eq(nm, "8A");
        edda_cam_name(16, nm); ok &= str_eq(nm, "8B");
        edda_cam_name(24, nm); ok &= str_eq(nm, "12B");
        edda_cam_name(1, nm); ok &= str_eq(nm, "1A");
        edda_cam_name(0, nm); ok &= str_eq(nm, "OFF");
        ck("Camelot names: 8A 8B 12B 1A, 0 is OFF", ok);
        edda_cam_neighbours(15, n);
        ck("8A's neighbours: 8B (relative), 9A (a fifth up), 7A (a fifth down)", n[0] == 16u && n[1] == 17u && n[2] == 13u);
        edda_cam_neighbours(1, n);
        ck("1A's wheel neighbours wrap: 2A up, 12A down", n[1] == 3u && n[2] == 23u);
        reset();
        trk[0].p[P_QUANT] = QN_OFF; trk[1].p[P_QUANT] = QN_WHITE;
        edda_camelot_apply(15);
        ok = edda.camelot == 15;
        for (i = 0; i < NTRK; i++)
            if (!drum_track(&trk[i]))
                ok &= trk[i].p[P_ROOT] == 9 && trk[i].p[P_SCALE] == (int16_t)ED_SC_MIN && trk[i].p[P_QUANT] != QN_OFF;
        ok &= trk[1].p[P_QUANT] == QN_WHITE;        /* (a quantiser already on is left as it is) */
        ok &= drum_track(&trk[3]) && trk[3].p[P_ROOT] == 0;   /* (the kit untouched) */
        ck("KEY 8A: every synth track to A minor, OFF quantisers to SNAP, the kit untouched", ok);
        edda_key_step(1); ok = edda.camelot == 17 && trk[0].p[P_ROOT] == 4;
        edda_key_step(-1); ok &= edda.camelot == 15;
        edda_key_step(0); ok &= edda.camelot == 16 && trk[0].p[P_ROOT] == 0 && trk[0].p[P_SCALE] == (int16_t)ED_SC_MAJ;
        ck("the key path: +1 to 9A (E minor), -1 back, across to 8B (C major) with the tracks following", ok);
        edda.camelot = 0; trk[0].p[P_ROOT] = 2; trk[0].p[P_SCALE] = ED_SC_MIN; song.sel = 0;
        edda_key_step(1);
        ck("no KEY set: the path starts from the selected track's own key (D minor 7A -> 8A)", edda.camelot == 15u);
    }
    /* ------------------------------------------------------------- Euclid */
    {
        int ok = 1;
        for (k = 1; k <= 16u; k++)
            for (i = 1; i <= k; i++)
                for (j = 0; j < 3u; j++)
                    ok &= popcount(edda_euclid(i, k, j)) == i;
        ck("E(k, n): k onsets for every k <= n <= 16, any rotation", ok);
        for (ok = 0, j = 0; j < 12u; j++)
            if (edda_euclid(7, 12, j) == 0xAB5u)
                ok = 1;                               /* bits 0 2 4 5 7 9 11: x.x.xx.x.x.x */
        ck("E(7, 12) is the ogene / gankogui timeline (2 2 1 2 2 2 1) at one rotation", ok);
        for (ok = 0, j = 0; j < 16u; j++)
            if (edda_euclid(5, 16, j) == 0x1249u)
                ok = 1;                               /* bits 0 3 6 9 12: the bossa-nova bell */
        ck("E(5, 16) is the bossa-nova bell (3 3 3 3 4) at one rotation; E(3, 8) the tresillo", ok && edda_euclid(3, 8, 0) == 0x49u);
    }
    /* ------------------------------------------------------------ patterns */
    {
        int ok = 1;
        uint32_t idx_ogene = 5, idx_log = 8, idx_sub = 10, idx_bitter = 12, idx_shkr = 7;
        ok = EDDA_NPAT == 17u && str_eq(EDDA_PAT[idx_ogene].name, "OGENE12") && str_eq(EDDA_PAT[idx_log].name, "LOGDRUM") &&
             str_eq(EDDA_PAT[idx_sub].name, "SUBROLL") && str_eq(EDDA_PAT[idx_bitter].name, "BITTERSW") &&
             str_eq(EDDA_PAT[idx_shkr].name, "SHKR16") && str_eq(EDDA_PAT[15].name, "LOG2BAR") && EDDA_PAT[15].len == 32u;
        for (i = 0; i < EDDA_NPAT; i++) {
            uint32_t l = 0;
            while (EDDA_PAT[i].name[l]) l++;
            ok &= l <= 8u && EDDA_PAT[i].len >= 1u && EDDA_PAT[i].len <= 32u && (EDDA_PAT[i].fill >> EDDA_PAT[i].len) == 0u;
        }
        ck("the bank: 17 patterns, names of at most 8 characters, 1..32 steps, fills inside the length", ok);
        reset();
        trk[0].p[P_ROOT] = 9;
        pat_load(&trk[0], NPATTERNS + 15);               /* LOG2BAR: two bars at 1/16 */
        ok = trk[0].p[P_SLEN] == 32 && trk[0].step[0].note[0] == 33 && trk[0].step[19].note[0] == 36 &&
             trk[0].step[29].time == ST_TIE && trk[0].step[30].time == ST_TIE && (trk[0].step[31].flags & SF_SLIDE) &&
             trk[0].step[32].time == ST_REST;
        ck("LOG2BAR: a 32-step phrase loads with LEN 32, its ties and the slide home", ok);
        reset();
        ok = pat_count() == NPATTERNS + EDDA_NPAT + up_pat_count();
        pat_label(NPATTERNS + idx_log, nm, (char[13]){0});
        ok &= str_eq(nm, "E09");
        {
            char name[13];
            pat_label(NPATTERNS + idx_log, nm, name);
            ok &= str_eq(name, "LOGDRUM");
            pat_label(NPATTERNS, nm, name);
            ok &= str_eq(nm, "E01") && str_eq(name, "3STEP");
        }
        ck("SEQ > PATTERNS lists the bank after the factory patterns as E01.. with their names", ok);
        /* a drum grid: OGENE12 on the drums track */
        pat_load(&trk[3], NPATTERNS + idx_ogene);
        ok = trk[3].p[P_SLEN] == 12 && trk[3].p[P_SDIV] == (int16_t)ED_DIV_8T;
        for (m0 = 0, i = 0; i < 12u; i++)
            if ((trk[3].step[i].hit >> 7) & 1u)
                m0 |= 1u << i;
        ok &= m0 == 0xAB5u && (trk[3].step[0].hit & 1u) && (trk[3].step[6].hit & 1u) && trk[3].step[0].time == ST_NOTE &&
              trk[3].step[1].time == ST_REST && trk[3].step[12].time == ST_REST && trk[3].step[0].vel == 96;
        ck("OGENE12: 12 steps at 8T, the bell lane on the timeline, kicks on pulses 1 and 7, accents as written", ok);
        pat_load(&trk[3], NPATTERNS + idx_shkr);
        for (ok = 1, i = 0; i < 16u; i++)
            ok &= (trk[3].step[i].hit == (1u << 3)) && (((trk[3].step[i].acc >> 3) & 1u) == !(i & 1u)) && trk[3].step[i].vel == 60;
        ck("SHKR16: closed hats on every sixteenth, accents on the beats and the ands, ghosts at 60", ok);
        /* melodic lines land in the track's root */
        trk[0].p[P_ROOT] = 9; trk[0].p[P_SCALE] = ED_SC_MIN;   /* A minor: C -> A, three semitones down */
        pat_load(&trk[0], NPATTERNS + idx_log);
        ok = trk[0].step[0].note[0] == 33 && trk[0].step[0].n == 1 && (trk[0].step[0].flags & SF_ACCENT) &&
             trk[0].step[6].note[0] == 28 && trk[0].step[14].note[0] == 33 && (trk[0].step[14].flags & SF_SLIDE) &&
             trk[0].step[1].time == ST_REST && trk[0].step[0].vel == 100 && trk[0].p[P_SLEN] == 16 &&
             trk[0].p[P_SDIV] == (int16_t)ED_DIV_16;
        ck("LOGDRUM into A minor: C2 -> A1, the fifth and flat seventh below, the slide kept, rests between", ok);
        trk[0].p[P_ROOT] = 2;                             /* D: two up */
        pat_load(&trk[0], NPATTERNS + idx_sub);
        ok = trk[0].step[0].note[0] == 26 && trk[0].step[1].time == ST_TIE && trk[0].step[8].time == ST_TIE &&
             trk[0].step[9].time == ST_REST && trk[0].step[10].note[0] == 21;
        ck("SUBROLL into D: the root held through ties, the fifth below on the and of 3", ok);
        trk[0].p[P_ROOT] = 9; trk[0].p[P_SCALE] = ED_SC_MIN;
        pat_load(&trk[0], NPATTERNS + idx_bitter);
        ok = trk[0].step[2].n == 3 && trk[0].step[2].note[0] == 57 && trk[0].step[2].note[1] == 60 && trk[0].step[2].note[2] == 64;
        trk[0].p[P_SCALE] = ED_SC_MAJ;
        pat_load(&trk[0], NPATTERNS + idx_bitter);
        ok &= trk[0].step[2].n == 3 && trk[0].step[2].note[1] == 61;
        ck("BITTERSW: the i-VI-III-VII stabs as triads, minor thirds in a minor key, major in a major one", ok);
        pat_load(&trk[0], 0);                             /* a factory pattern still loads as before */
        ok = trk[0].step[0].note[0] == 45 && trk[0].p[P_SLEN] == 16;
        ck("the factory patterns are untouched (01 ACID)", ok);
    }
    /* ---------------------------------------------------------------- run */
    {
        int ok = 1;
        uint32_t m, lvl;
        reset();
        pat_load(&trk[3], NPATTERNS);                     /* 3STEP on the drums: kick + hat on step 0 */
        trk[1].p[P_LEVEL] = 100; trk[2].p[P_LEVEL] = 90; trk[0].p[P_LEVEL] = 110;
        seq_start();
        blocks(1);
        ok = edda.playing && edda.bar == 0 && cues_note(0, ED_CUE_NOTE_BAR) == 1u;
        beats(1); beats(1);
        edda_run_request();
        ok &= edda.armed && edda.phase == ED_IDLE;
        to_next_bar();
        ok &= edda.bar == 1 && edda.phase == ED_SHAKERS && !edda.armed && edda.left == 4u;
        ok &= edda_lane_mute == ((1u << 2) | (1u << 3) | (1u << 4) | (1u << 6)) && trk[2].p[P_MUTE] == 1 &&
              cues_note(0, ED_CUE_NOTE_RUN) == 1u;
        /* the step that just fired: the kick sounds, the hat (lane 3) does not */
        ok &= has_note(&trk[3], 36) && !has_note(&trk[3], 42);
        ck("D4 arms the run; on the one: SHAKERS, the hats / claps / rims rest, the lead mutes, the kick plays", ok);
        beats(4);
        ok = edda.phase == ED_STABS && edda.left == 4u;
        lvl = (uint32_t)trk[1].p[P_LEVEL];
        beats(2);
        m = (uint32_t)trk[1].p[P_LEVEL];
        ok &= edda.phase == ED_STABS && m < lvl && m > 30u && m < 70u;   /* (half way: about half) */
        beats(2);
        ok &= edda.phase == ED_LOG && trk[1].p[P_LEVEL] == 0 && edda_lane_mute == (uint8_t)~1u && edda.left == 8u;
        ck("STABS ramps the stabs' level to nothing over its bars; LOG: the log drum and the kick alone, two bars", ok);
        beats(8);
        ok = edda.phase == ED_SILENCE && edda.left == 1u;
        for (i = 0; i < NTRK; i++) ok &= trk[i].p[P_MUTE] == 1;
        m0 = mo_w;
        beats(1);
        ok &= edda.phase == ED_IDLE && trk[1].p[P_LEVEL] == 100 && trk[2].p[P_LEVEL] == 90 && trk[0].p[P_LEVEL] == 110 &&
              edda_lane_mute == 0 && cues_note(m0, ED_CUE_NOTE_DROP) == 1u;
        for (i = 0; i < NTRK; i++) ok &= trk[i].p[P_MUTE] == 0;
        ck("SILENCE: one beat, everything muted; the DROP: the mix exactly as before, the lanes back, the cue", ok);
        edda.run_len = 1;
        edda_run_request(); to_next_bar();
        ok = edda.phase == ED_SHAKERS && edda.left == 8u;
        beats(8); ok &= edda.phase == ED_STABS && edda.left == 8u;
        ck("RUN LONG: two bars of SHAKERS and of STABS", ok);
        transport_req = 2; blocks(2);                     /* PLAY: a stop ends the run and restores at once */
        ok = !song.playing && edda.phase == ED_IDLE && trk[1].p[P_LEVEL] == 100 && edda_lane_mute == 0;
        for (i = 0; i < NTRK; i++) ok &= trk[i].p[P_MUTE] == 0;
        ck("a stop in the middle of a run: the mix comes back at once", ok);
        reset(); edda_defaults(); seq_start(); m0 = mo_w; beats(5);
        ck("SHOW CUES OFF (the default): not one byte on MIDI OUT", mo_w == m0 && !edda.cues);
        reset(); seq_start(); beats(5);
        ok = cues_cc(0, ED_CUE_CC_BAR, &m) >= 2u && m == 1u && cues_cc(0, ED_CUE_CC_BEAT, 0) >= 5u &&
             cues_cc(0, ED_CUE_CC_ACT, &m) == 1u && m == 1u;
        ck("SHOW CUES ON: bar and beat CCs as the clock goes, the act once at play", ok);
    }
    /* ---------------------------------------------------------- hard stop */
    {
        int ok;
        reset(); trk[1].p[P_LEVEL] = 77;
        seq_start(); beats(1); beats(1);
        m0 = mo_w;
        edda_stop_toggle();
        ok = edda.stopped && edda_lane_mute == 0xFFu && cues_note(m0, ED_CUE_NOTE_STOP) == 1u && song.playing;
        for (i = 0; i < NTRK; i++) ok &= trk[i].p[P_MUTE] == 1;
        beats(1);
        ok &= song.playing && edda.stopped;             /* (the clock runs on, still silent) */
        edda_stop_toggle();
        ok &= edda.resume && edda.stopped;
        for (i = 0; i < NTRK; i++) ok &= trk[i].p[P_MUTE] == 1;   /* (not yet: on the one) */
        m0 = mo_w;
        to_next_bar();
        ok &= !edda.stopped && !edda.resume && edda_lane_mute == 0 && trk[1].p[P_LEVEL] == 77 &&
              cues_note(m0, ED_CUE_NOTE_DROP) == 1u;
        for (i = 0; i < NTRK; i++) ok &= trk[i].p[P_MUTE] == 0;
        ck("E4: the hard stop at once (clock running), E4 again: the re-entry on the next one, the cue", ok);
        edda_run_request();
        ck("a run cannot be asked for while stopped", !edda.armed || !edda.stopped);
    }
    /* ------------------------------------------------------------- REVEAL */
    {
        int ok = 1;
        uint32_t seen8 = 0, seen17 = 0, seen16 = 0;
        reset(); edda.reveal = 1; seq_start(); blocks(1);
        for (i = 0; i < 25u; i++) {
            uint32_t b = edda.bar + 1u;                   /* the bar playing, 1-based */
            if (b == 8u) seen8 = edda_hole_mute;
            if (b == 16u) seen16 = edda_hole_mute;
            if (b == 17u) seen17 = edda_lane_map[1];
            if (b == 7u || b == 9u) ok &= edda_hole_mute == 0;
            if (b == 16u || b == 25u) ok &= edda_lane_map[1] == 1;
            to_next_bar();
        }
        ok &= seen8 == 1u && seen16 == 1u && seen17 == 6u;
        ck("REVEAL: the kick rests on every 8th bar (the hole), SNARE moves to RIM in bars 17..24 of 32", ok);
        reset(); edda.reveal = 0; seq_start(); blocks(1);
        for (ok = 1, i = 0; i < 9u; i++) { ok &= edda_hole_mute == 0 && edda_lane_map[1] == 1; to_next_bar(); }
        ck("REVEAL OFF: no hole, no move", ok);
    }
    /* --------------------------------------------------------------- FILL */
    {
        int ok;
        uint32_t period;
        reset();
        pat_load(&trk[3], NPATTERNS);                     /* 3STEP: steps 14..16 are fill-only (toms, a snare pickup) */
        ok = (trk[3].step[13].flags & SF_FILL) && (trk[3].step[14].flags & SF_FILL) && (trk[3].step[15].flags & SF_FILL) &&
             !(trk[3].step[12].flags & SF_FILL) && (trk[3].step[15].hit & 2u) && (trk[3].step[15].hit & 32u);
        period = div_samples((uint32_t)trk[3].p[P_SDIV]);
        trk[3].seq_idx = 14;
        seq_step(&trk[3], &trk[3].step[14], period, 0);
        ok &= trk[3].seq_n == 0;                          /* FILL off: the step rests */
        edda.fill = 1;
        seq_step(&trk[3], &trk[3].step[14], period, 0);
        ok &= has_note(&trk[3], 45);                      /* FILL on: the tom plays */
        edda.fill = 0;
        seq_step(&trk[3], &trk[3].step[12], period, 0);
        ok &= has_note(&trk[3], 36) && has_note(&trk[3], 37);   /* a plain step plays either way */
        ck("fill-only steps rest until FILL; the bank's grids carry fills on their last steps", ok);
        reset(); pat_load(&trk[3], NPATTERNS); seq_start(); beats(2);
        m0 = mo_w;
        edda_fill_request();
        ok = edda.fill && edda.fill_until == edda.bar + 2u && cues_cc(m0, ED_CUE_CC_FILL, &k) == 1u && k == 1u;
        to_next_bar(); ok &= edda.fill;                   /* the rest of this bar and the next */
        to_next_bar(); ok &= !edda.fill && cues_cc(m0, ED_CUE_CC_FILL, &k) == 2u && k == 0u;
        edda_fill_request(); edda_fill_request(); ok &= !edda.fill;   /* pressed twice: off at once */
        lay_combo(B_GLO, white(9)); blocks(1); ok &= edda.fill;   /* (the UI's actions: run by the next block) */
        key_up(white(9)); btn_up(B_GLO); frame();
        ck("A4 FILL: on through the end of the next bar, the cue; again: off; the GLO key sets it", ok);
    }
    /* ------------------------------------------------------------- MUTATE */
    {
        int ok;
        uint8_t before[16];
        reset();
        pat_load(&trk[3], NPATTERNS);                     /* 3STEP, no bell */
        for (i = 0; i < 16u; i++) before[i] = trk[3].step[i].hit;
        seq_start(); beats(1);
        edda_mutate_request();
        ok = edda.mutate;
        for (i = 0; i < 16u; i++) ok &= trk[3].step[i].hit == before[i];   /* (not yet) */
        m0 = mo_w;
        to_next_bar();
        for (k = 0, i = 0; i < 16u; i++) {
            k += (trk[3].step[i].hit >> 7) & 1u;
            ok &= (trk[3].step[i].hit & 0x7Fu) == before[i];          /* every other lane exactly as it was */
        }
        ok &= (k == 3u || k == 5u || k == 7u) && !edda.mutate && cues_note(m0, ED_CUE_NOTE_MUTATE) == 1u;
        ck("B4 MUTATE on the one: the bell lane gets E(3|5|7, 16), the kick / backbeat / shakers untouched", ok);
        transport_req = 2; blocks(2);
        edda_mutate_request();
        for (k = 0, i = 0; i < 16u; i++) k += (trk[3].step[i].hit >> 7) & 1u;
        ck("stopped: MUTATE at once", (k == 3u || k == 5u || k == 7u));
        edda.seed = 1; edda_mutate_now(); for (m0 = 0, i = 0; i < 16u; i++) m0 |= ((trk[3].step[i].hit >> 7) & 1u) << i;
        edda.seed = 1; edda_mutate_now(); for (k = 0, i = 0; i < 16u; i++) k |= ((trk[3].step[i].hit >> 7) & 1u) << i;
        ck("the same seed rolls the same bell lane (a night can be replayed)", m0 == k);
    }
    /* --------------------------------------------------------------- MENU */
    {
        int ok;
        reset();
        ok = menu_n(MI_KEY) == 25u && menu_n(MI_ACT) == ED_ACTS && menu_n(MI_CUES) == 2u && menu_n(MI_RUNLEN) == 2u &&
             menu_n(MI_REVEAL) == 2u && menu_n(MI_SEQOUT) == 3u && MI_TAB[MI_KEY] == MTAB_EDDA && MI_TAB[MI_REVEAL] == MTAB_EDDA &&
             MI_TAB[MI_SEQOUT] == MTAB_EDDA && MI_SEQOUT == MI_CUES + 1u &&
             str_eq(MTAB_NAME[MTAB_EDDA], "EDDA") && mtab_rows(MTAB_EDDA) == 6u && str_eq(MI_NAME[MI_KEY], "KEY") &&
             str_eq(MI_NAME[MI_SEQOUT], "SEQ OUT") && MI_KEY < MI_VALUES && MI_REVEAL < MI_VALUES;
        ck("MENU > EDDA: KEY (25 values), SHOW CUES, SEQ OUT (3), ACT (5), RUN, REVEAL; all value rows, one tab", ok);
        ok = str_eq(menu_vname(MI_KEY, 15), "8A") && str_eq(menu_vname(MI_KEY, 0), "OFF") && str_eq(menu_vname(MI_KEY, 24), "12B") &&
             str_eq(menu_vname(MI_ACT, 0), "1 BULB") && str_eq(menu_vname(MI_ACT, 4), "5 BULBS") &&
             str_eq(menu_vname(MI_RUNLEN, 1), "LONG") && str_eq(menu_vname(MI_CUES, 1), "ON") &&
             str_eq(menu_vname(MI_SEQOUT, 0), "OFF") && str_eq(menu_vname(MI_SEQOUT, 1), "NOTES") &&
             str_eq(menu_vname(MI_SEQOUT, 2), "+CLOCK");
        ck("the rows' value names", ok);
        menu_put(MI_KEY, 15);
        ok = menu_get(MI_KEY) == 15u && trk[0].p[P_ROOT] == 9 && trk[0].p[P_SCALE] == (int16_t)ED_SC_MIN &&
             cues_cc(0, ED_CUE_CC_KEY, &m0) == 0u;     /* (the cue: from the audio block, never the main loop) */
        blocks(1);
        ok &= cues_cc(0, ED_CUE_CC_KEY, &m0) == 1u && m0 == 15u;
        m0 = mo_w;
        menu_put(MI_ACT, 2); blocks(1);
        ok &= edda.act == 3 && menu_get(MI_ACT) == 2u && cues_cc(m0, ED_CUE_CC_ACT, &k) == 1u && k == 3u;
        menu_put(MI_CUES, 0); ok &= !edda.cues;
        menu_put(MI_REVEAL, 1); ok &= edda.reveal == 1;
        menu_put(MI_RUNLEN, 1); ok &= edda.run_len == 1;
        ok &= menu_step(MI_KEY, 1) == 16u;
        menu_put(MI_KEY, 24); ok &= menu_step(MI_KEY, 1) == 24u;   /* (stops at the end, as every row but COLOR) */
        menu_put(MI_KEY, 0); ok &= menu_step(MI_KEY, -1) == 0u;
        ck("the rows set the key (tracks follow, the cue), the act (the cue), cues, REVEAL, RUN; KEY stops at its ends", ok);
    }
    /* ---------------------------------------------------------- GLO layer */
    {
        int ok;
        reset(); seq_start(); beats(1);
        lay_combo(B_GLO, white(5));
        ok = ui.layer == LAYER_GLO && !edda.armed && !gates();   /* (asked: the audio block runs it) */
        blocks(1); ok &= edda.armed;
        key_up(white(5)); btn_up(B_GLO); frame();
        lay_combo(B_GLO, white(8)); blocks(1); ok &= edda.act == 2;
        key_up(white(8)); btn_up(B_GLO); frame();
        lay_combo(B_GLO, white(12)); blocks(1); ok &= edda.camelot == 17u;   /* (from the selected track's A minor 8A: 9A) */
        key_up(white(12)); btn_up(B_GLO); frame();
        lay_combo(B_GLO, white(11)); blocks(1); ok &= edda.camelot == 15u;
        key_up(white(11)); btn_up(B_GLO); frame();
        lay_combo(B_GLO, white(13)); blocks(1); ok &= edda.camelot == 16u;
        key_up(white(13)); btn_up(B_GLO); frame();
        lay_combo(B_GLO, white(10)); blocks(1); ok &= edda.mutate;
        key_up(white(10)); btn_up(B_GLO); frame();
        lay_combo(B_GLO, white(6)); blocks(1); ok &= edda.stopped;
        key_up(white(6)); btn_up(B_GLO); frame();
        ok &= !ui.layer && !gates() && edda_rq_r == edda_rq_w;
        ck("GLO + D4 arms the run, E4 stops, G4 the next act, B4 MUTATE, C5 / D5 / E5 the key path; keys silent", ok);
        {
            uint32_t a = leds_at(0), b2 = leds_at(250);
            btn_down(B_GLO); frames(600);
            a = leds_at(0); b2 = leds_at(250);
            ok = ((a | b2) >> white(6)) & 1u;         /* STOP lit while stopped */
            btn_up(B_GLO); frame();
            ck("the GLO map: E4 lit while stopped", ok);
        }
    }
    /* ---------------------------------------------------------- the bank */
    {
        int ok = FM6_NFACTORY == 13u && str_eq(N_FM6_PATCH[8], "E1") && str_eq(N_FM6_PATCH[12], "E5") &&
                 str_eq(N_FM6_PATCH[FM6_OWN], "OWN") && NELEM(FM6_PRESETS) == 13u;
        ok &= !memcmp(FM6_FACTORY[8] + 118, "OGENE IRON", 10) && !memcmp(FM6_FACTORY[9] + 118, "OJA FLUTE ", 10) &&
              !memcmp(FM6_FACTORY[10] + 118, "HILIFE GTR", 10) && !memcmp(FM6_FACTORY[11] + 118, "TALK DRUM ", 10) &&
              !memcmp(FM6_FACTORY[12] + 118, "LOG DRUM  ", 10);
        ok &= str_eq(FM6_PRESETS[8].name, "OGENE") && FM6_PRESETS[8].e[7] == 8 && FM6_PRESETS[12].mono == 1 &&
              str_eq(FM6_PRESETS[12].name, "LOG DRUM");
        ck("FM6: thirteen factory patches, E1..E5 the EDDA voice bank, their presets select them, LOG DRUM is MONO", ok);
        reset();
        set_engine_of(&trk[1], ENGI_FM6); apply_preset_to(&trk[1], 8);
        ok = trk[1].p[P_E7] == 8 && str_eq(UI_PALETTES[UI_BW_INDEX + 1u].name, "EDDA") && NPALETTES == UI_BW_INDEX + 2u;
        ck("the OGENE preset loads with SLOT E1; the EDDA palette is the last one", ok);
    }
    /* ------------------------------------------------------ CHORD+ */
    {
        track_t *t = &trk[0];
        uint32_t kc = 7u;                                /* C4 (keys from F3) */
        int ok;
        reset();
        song.sel = 0;
        song.master_q12 = 4096;
        set_engine_of(t, 0); apply_preset_to(t, 0);
        t->p[P_CHRD] = CH_MAJ; t->p[P_QUANT] = QN_WHITE; t->p[P_ROOT] = 0; t->p[P_SCALE] = 1; t->p[P_VOICE] = V_POLY;
        t->p[P_VOIC] = VC_CLOSE;
        blocks(2);                                       /* (the preset's panic request served) */
        ok = chord_plus_on(t) && !chp_held;
        key_down(kc); frame();
        ok &= kb_chn[kc] == 3u && kb_chord[kc][0] == 60 && kb_chord[kc][1] == 64 && kb_chord[kc][2] == 67;
        key_up(kc); frame();
        {   /* each black key: its modifier on C major */
            static const uint8_t WANT[CHP_COUNT][4] = {
                {60, 63, 67, 0}, {60, 64, 67, 70}, {60, 64, 67, 71}, {60, 65, 67, 0}, {60, 64, 67, 74},
                {64, 67, 72, 0}, {48, 60, 64, 67}, {60, 64, 67, 0}, {60, 67, 76, 0}};
            for (i = 0; i < CHP_COUNT && ok; i++) {
                uint32_t kb = black(i), n = WANT[i][3] ? 4u : 3u;
                key_down(kb); frame();
                ok &= chp_held == (1u << i) && kb_note[kb] == KB_SILENT && !gates() && ((kb_layer >> kb) & 1u);
                key_down(kc); frame();
                ok &= kb_chn[kc] == n;
                for (j = 0; j < n; j++)
                    ok &= kb_chord[kc][j] == WANT[i][j];
                if (!ok)
                    printf("  black %u (%s): %u notes %u %u %u %u\n", i, CHP_NAME[i], kb_chn[kc], kb_chord[kc][0],
                           kb_chord[kc][1], kb_chord[kc][2], kb_chord[kc][3]);
                key_up(kc); key_up(kb); frame();
                ok &= !chp_held && !gates();
            }
        }
        ck("CHORD+ (CHRD on, QNT WHITE): each black key held changes the chord: MIN 7TH MAJ7 SUS4 9TH INV BASS STRUM OPEN; silent, let go: off", ok);
        /* combinations: MIN + 7TH = m7, SUS4 + 7TH = 7sus4, 7TH + 9TH = 1-3-7-9; the name follows */
        key_down(black(CHP_MIN)); key_down(black(CHP_7TH)); key_down(kc); frame();
        ok = kb_chn[kc] == 4u && kb_chord[kc][1] == 63 && kb_chord[kc][3] == 70 && chord_last[0].mask == 0x489u;
        key_up(kc); key_up(black(CHP_MIN)); key_up(black(CHP_7TH)); frame();
        key_down(black(CHP_SUS4)); key_down(black(CHP_7TH)); key_down(kc); frame();
        ok &= kb_chn[kc] == 4u && kb_chord[kc][1] == 65 && kb_chord[kc][3] == 70;
        key_up(kc); key_up(black(CHP_SUS4)); key_up(black(CHP_7TH)); frame();
        key_down(black(CHP_7TH)); key_down(black(CHP_9TH)); key_down(kc); frame();
        ok &= kb_chn[kc] == 4u && kb_chord[kc][1] == 64 && kb_chord[kc][2] == 70 && kb_chord[kc][3] == 74;
        {
            char nm2[12];
            chord_name(nm2, 0, chord_last[0].mask);
            ok &= str_eq(nm2, "C7");                     /* (the ninth is not named: the seventh's chord) */
        }
        key_up(kc); key_up(black(CHP_7TH)); key_up(black(CHP_9TH)); frame();
        ck("CHORD+ combinations: MIN + 7TH is m7, SUS4 + 7TH is 7sus4, 7TH + 9TH drops the fifth (1-3-7-9)", ok);
        /* STRUM: the second and third notes 18 and 36 ms later; a key let go before cancels what is left */
        key_down(black(CHP_STRUM)); key_down(kc); frame();
        ok = kb_chn[kc] == 3u && gates() == 1u && strum_n == 2u;
        blocks((CHP_STRUM_GAP + CTL - 1u) / CTL);
        ok &= gates() == 2u && strum_n == 1u;
        blocks((CHP_STRUM_GAP + CTL - 1u) / CTL);
        ok &= gates() == 3u && strum_n == 0u;
        key_up(kc); frame();
        ok &= !gates();
        mo_r = mo_w;                                     /* (the ring drained: the counts below) */
        m0 = mo_w;
        key_down(kc); frame();
        key_up(kc); frame();                             /* at once: the strummed notes never sound, no note-off */
        ok &= !gates() && strum_n == 0u && mo_w - m0 == 2u;   /* (the root's note-on and note-off only) */
        key_up(black(CHP_STRUM)); frame();
        ck("CHORD+ STRUM: the notes 18 ms apart, low to high; let go first: the waiting ones never sound", ok);
        /* VOIC LEAD: the nearest inversion to the last chord; CHORD+ needs QNT WHITE; a kit has none */
        t->p[P_VOIC] = VC_LEAD;
        key_down(kc); frame();                           /* C E G */
        ok = kb_chn[kc] == 3u && kb_chord[kc][0] == 60;
        key_up(kc); frame();
        key_down(12u); frame();                          /* F4 (key 12): F major -> C F A (its second inversion) */
        ok &= kb_chn[12] == 3u && kb_chord[12][0] == 60 && kb_chord[12][1] == 65 && kb_chord[12][2] == 69;
        key_up(12u); frame();
        key_down(14u); frame();                          /* G4: G major -> B D G (near C F A) */
        ok &= kb_chn[14] == 3u && kb_chord[14][0] == 59 && kb_chord[14][1] == 62 && kb_chord[14][2] == 67;
        key_up(14u); frame();
        t->p[P_VOIC] = VC_CLOSE;
        t->p[P_QUANT] = QN_SNAP;
        ok &= !chord_plus_on(t);
        key_down(black(CHP_MIN)); frame();
        ok &= !chp_held && kb_note[black(CHP_MIN)] != KB_SILENT;   /* (SNAP: a black key is a note) */
        key_up(black(CHP_MIN)); frame();
        ok &= str_eq(N_VOIC[VC_LEAD], "LEAD") && TP[P_VOIC].max == VC_LEAD;
        ck("VOIC LEAD: F after C is C F A, G after it B D G (the smallest move); CHORD+ only with QNT WHITE", ok);
    }
    /* ------------------------------------------------------ the track filter */
    {
        track_t *t = &trk[0];
        int ok;
        uint32_t k2;
        static int32_t x0[4096], x1[4096];
        /* a square wave from the sequencer-free path: the track's buffer through track_filter alone */
        for (i = 0; i < 4096u; i++)
            x0[i] = (i / 64u) & 1u ? 8000 : -8000;       /* 344 Hz, rich in harmonics */
        reset();
        ok = str_eq(TP[P_ED_FX].label, "FILT") && TP[P_ED_FX].min == -64 && TP[P_ED_FX].max == 63 && TP[P_ED_FX].def == 0;
        t->p[P_ED_FX] = 0;
        memcpy(x1, x0, sizeof x1);
        track_filter(t, x1, 4096);
        ok &= !memcmp(x0, x1, sizeof x0);                /* 0: bit for bit */
        {   /* -64 (a 100 Hz low-pass): the 344 Hz square is nearly gone; -8 (8 kHz): nearly whole */
            int32_t pk = 0, pk8 = 0, hf = 0, hf0 = 0;
            t->p[P_ED_FX] = -64;
            t->flt_y1 = t->flt_y2 = 0;
            memcpy(x1, x0, sizeof x1);
            track_filter(t, x1, 4096);
            for (i = 2048; i < 4096u; i++) pk = abs(x1[i]) > pk ? abs(x1[i]) : pk;
            t->p[P_ED_FX] = -8;
            t->flt_y1 = t->flt_y2 = 0;
            memcpy(x1, x0, sizeof x1);
            track_filter(t, x1, 4096);
            for (i = 2048; i < 4096u; i++) {
                pk8 = abs(x1[i]) > pk8 ? abs(x1[i]) : pk8;
                hf += abs(x1[i] - x1[i - 1u]);
                hf0 += abs(x0[i] - x0[i - 1u]);
            }
            ok &= pk < 1500 && pk8 > 7000 && hf < hf0;   /* (the edges softened) */
            /* +63 (a 7.8 kHz high-pass): the square's body is gone, only its edges remain */
            t->p[P_ED_FX] = 63;
            t->flt_y1 = t->flt_y2 = 0;
            memcpy(x1, x0, sizeof x1);
            track_filter(t, x1, 4096);
            k2 = 0;
            for (i = 2048; i < 4096u; i++)
                if (i % 64u == 32u) k2 += (uint32_t)abs(x1[i]);   /* mid-plateau: near 0 */
            ok &= k2 < 32u * 300u && abs(x1[2048]) > 1000;   /* (the edge at 2048 passes) */
            /* +1 (22 Hz): nearly whole */
            t->p[P_ED_FX] = 1;
            t->flt_y1 = t->flt_y2 = 0;
            memcpy(x1, x0, sizeof x1);
            track_filter(t, x1, 4096);
            pk = 0;
            for (i = 2048; i < 4096u; i++) pk = abs(x1[i]) > pk ? abs(x1[i]) : pk;
            ok &= pk > 7000;
        }
        {   /* the page: FILTER in the FX family after SLICER, its one knob the track's FILT; the knob steps it */
            uint32_t pi;
            for (pi = 0; pi < NPAGES && !str_eq(PAGES[pi].title, "FILTER"); pi++) ;
            ok &= pi < NPAGES && PAGES[pi].fam == FAM_FX && PAGES[pi].id[0] == P_ED_FX && PAGES[pi].id[1] == 0xFFu &&
                  str_eq(PAGES[pi - 1u].title, "SLICER") && PAGES[pi].graph == GR_FILT;
            ui_power_on();
            ui.home = 0; ui.page = (uint8_t)pi; page_entered(); frame();
            t = TSEL;
            t->p[P_ED_FX] = 0;
            turn(EN_K1, -3); frame();
            ok &= t->p[P_ED_FX] == -3;
            turn(EN_K1, 10); frame();
            ok &= t->p[P_ED_FX] == 7;
            t->p[P_ED_FX] = 0;
        }
        ck("FILT (the track's one-knob filter): 0 bit for bit, -64 a 100 Hz low-pass, +63 a 7.8 kHz high-pass, the FILTER page after SLICER", ok);
    }
    /* ------------------------------------------------------ micro timing */
    {
        static const int8_t N0[4] = {0, 0, 0, 0}, NL[4] = {0, 2, 0, 0}, NE[4] = {0, 0, -3, 0}, S3[4] = {3, 0, 0, 0},
                            SE[4] = {-2, 0, 0, 0};
        static int32_t base[4][4];
        track_t *t = &trk[0];
        int ok = 1;
        uint32_t lp, s;
        int32_t d;
        reset(); mt_quiet();
        ok &= step_nudge(&t->step[0]) == 0 && SF_EARLY == 4u && !(SF_EDDA & (SF_ACCENT | SF_SLIDE | SF_RATCH));
        {   /* the nudge's bits round trip, clamped to -3..3, the step's other flags kept */
            step_t st = {{60, 0, 0, 0}, 1, ST_NOTE, SF_ACCENT | SF_SLIDE | SF_FILL, 96, 0, 0};
            step_set_ratchet(&st, 3);
            for (d = -5; d <= 5; d++) {
                step_set_nudge(&st, d);
                ok &= step_nudge(&st) == (d < -3 ? -3 : d > 3 ? 3 : d) && step_ratchet(&st) == 3u &&
                      (st.flags & (SF_ACCENT | SF_SLIDE | SF_FILL)) == (SF_ACCENT | SF_SLIDE | SF_FILL);
            }
            step_set_nudge(&st, 0);
            ok &= !(st.flags & (SF_EARLY | SF_NUDGE));
        }
        mt_track(t, 60, N0); seq_start(); mt_run(t, 2800); seq_stop();
        for (lp = 0; lp < 4u; lp++)
            for (s = 0; s < 4u; s++)
                base[lp][s] = mt_at(60u + (uint32_t[]){0, 2, 4, 5}[s], lp);
        ok &= base[0][0] == 0 && base[1][0] > 0 && base[3][3] > 0;
        ck("micro timing: a step's nudge is -3..+3 sixteenths of the step (SF_EARLY, SF_NUDGE), its other flags kept", ok);

        reset(); mt_quiet(); mt_track(t, 60, NL); seq_start(); mt_run(t, 2800); seq_stop();
        ok = 1;
        for (lp = 0; lp < 4u; lp++)
            for (s = 0; s < 4u; s++) {
                d = mt_at(60u + (uint32_t[]){0, 2, 4, 5}[s], lp) - base[lp][s];
                ok &= s == 1u ? d >= 21 && d <= 22 : d == 0;  /* +2/16 of 5512 samples: 688, 21.5 blocks */
            }
        reset(); mt_quiet(); mt_track(t, 60, NE); seq_start(); mt_run(t, 2800); seq_stop();
        for (lp = 0; lp < 4u; lp++)
            for (s = 0; s < 4u; s++) {
                d = mt_at(60u + (uint32_t[]){0, 2, 4, 5}[s], lp) - base[lp][s];
                ok &= s == 2u ? d >= -33 && d <= -32 : d == 0;   /* -3/16: 1032 samples early */
            }
        ck("NUDGE +2 starts the step 688 samples late, -3 1032 early; every other step on the grid, four bars on", ok);

        reset(); mt_quiet(); mt_track(t, 60, S3); seq_start(); mt_run(t, 2800); seq_stop();
        ok = 1;
        for (lp = 0; lp < 4u; lp++)
            for (s = 0; s < 4u; s++) {
                d = mt_at(60u + (uint32_t[]){0, 2, 4, 5}[s], lp) - base[lp][s];
                ok &= s == 0u ? d >= 32 && d <= 33 : d == 0;
            }
        reset(); mt_quiet(); mt_track(t, 60, SE); seq_start(); mt_run(t, 2800); seq_stop();
        for (lp = 0; lp < 4u; lp++)
            for (s = 0; s < 4u; s++) {
                d = mt_at(60u + (uint32_t[]){0, 2, 4, 5}[s], lp) - base[lp][s];
                ok &= s == 0u ? (lp == 0u ? d == 0 : d >= -22 && d <= -21) : d == 0;   /* (never before PLAY) */
            }
        ck("step 1 nudged at PLAY: late waits for its nudge, early starts at once; the grid never drifts", ok);

        {   /* the song chain: slot A's step 1 early (-2): slot B starts on the grid, not 688 samples early */
            int32_t b70[2];
            uint32_t pass;
            for (pass = 0; pass < 2u; pass++) {
                reset(); mt_quiet();
                mt_track(t, 60, pass ? SE : N0);
                project_save(0);
                mt_track(t, 70, N0);
                project_save(1);
                chain_config.count = 2;
                chain_config.row[0] = (chain_row_t){0, 1};
                chain_config.row[1] = (chain_row_t){1, 1};
                ok = chain_prepare() == 0u;
                mt_run(t, 1400);
                b70[pass] = mt_at(70, 0);
                ok &= mt_at(72, 0) - b70[pass] >= 172 && mt_at(72, 0) - b70[pass] <= 173;
                seq_stop();
                chain_config.count = 0;
            }
            ok &= b70[0] > 600 && b70[1] - b70[0] >= -1 && b70[1] - b70[0] <= 1;
            ck("a song chain: slot A's step 1 nudged early, slot B still starts on the grid (the carry from the plain boundary)", ok);
        }

        {   /* the CHANCE page: KNOB 4 NUDGE of the cursor's step */
            reset(); mt_quiet(); mt_track(t, 60, N0);
            song.sel = 0;
            go_page(GR_CHANCE); frame();
            ui.cursor = 1;
            turn(EN_K4, 2); frame();
            ok = step_nudge(&t->step[1]) == 2;
            turn(EN_K4, -7); frame();
            ok &= step_nudge(&t->step[1]) == -3 && (t->step[1].flags & SF_EARLY);
            turn(EN_K4, 3); frame();
            ok &= step_nudge(&t->step[1]) == 0 && !(t->step[1].flags & (SF_EARLY | SF_NUDGE)) && step_nudge(&t->step[0]) == 0;
            ck("SEQ > CHANCE: KNOB 4 NUDGE moves the cursor's step -3..+3 (clamped), 0 clears it", ok);
        }

        {   /* projects keep the nudges and the fills; a project without them is written as Felucca writes it */
            static project_t pa, pb;
            static project_store_t ps;
            uint32_t pos = 68u + NTRK * (P_COUNT + 2u + NSTEP * 9u) + (uint32_t)sizeof(chain_config_t) +
                           (uint32_t)sizeof(motion_store_t), zero = 1, top = 0, i;
            reset(); mt_quiet(); mt_track(t, 60, NE);
            step_set_nudge(&t->step[3], 1);
            t->step[1].flags |= SF_FILL;
            trk[2].step[40].flags |= SF_FILL;
            project_capture(&pa);
            ok = proj_pack(&ps, &pa) && proj_import(&pb, &ps, sizeof ps);
            ok &= step_nudge(&pb.t[0].step[2]) == -3 && step_nudge(&pb.t[0].step[3]) == 1 && step_nudge(&pb.t[0].step[0]) == 0 &&
                  (pb.t[0].step[1].flags & SF_FILL) && (pb.t[2].step[40].flags & SF_FILL) && !(pb.t[0].step[2].flags & SF_FILL) &&
                  pb.t[0].step[2].note[0] == 64 && pb.t[0].step[3].note[0] == 65 && !memcmp(ps.raw + pos, "EDD1", 4);
            for (i = 0; i < NSTEP; i++)
                t->step[i].flags &= (uint8_t)~SF_EDDA;
            trk[2].step[40].flags &= (uint8_t)~SF_FILL;
            project_capture(&pa);
            ok &= proj_pack(&ps, &pa) != 0;
            for (i = pos; i < PROJ_FM6_OFF; i++)
                zero &= ps.raw[i] == 0u;
            for (i = 0; i < NSTEP; i++)
                top |= ps.raw[68u + P_COUNT + 2u + i * 9u + 1u] | ps.raw[68u + P_COUNT + 2u + i * 9u + 2u] |
                       ps.raw[68u + P_COUNT + 2u + i * 9u + 3u];
            ok &= zero && !(top & 128u);
            ps.raw[68u + P_COUNT + 2u] |= 128u;          /* a note's top bit proj_pack never sets: refused */
            {
                uint32_t sum = proj_hash(ps.raw, PROJ_STORE_SIZE - 4u);
                memcpy(ps.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
            }
            ok &= !proj_import(&pb, &ps, sizeof ps);
            ck("projects: the nudges in the notes' top bits, the fills in the tail (EDD1); none: the tail and those bits zero; junk refused", ok);
        }
        {   /* a user preset never reads SF_EARLY as its tie (editor_test: STEP_SET keeps the nudge) */
            static up_rec_t r;
            reset(); mt_quiet(); mt_track(t, 60, SE);
            up_pat_from(&r, t->step);
            ok = r.note[0] == 60 && !(r.flags[0] & 4u) && r.note[1] == 62;
            ck("a user preset keeps an early-nudged step a note (SF_EARLY is not its tie)", ok);
        }
    }
    /* ------------------------------------------------------ the exact grid */
    {
        static const int32_t BPMS[3] = {128, 97, 173};
        static const uint32_t MINUTES[3] = {10, 2, 2};
        uint32_t bi;
        int ok = 1, okd = 1;
        for (bi = 0; bi < 3u; bi++) {
            uint32_t beats = (uint32_t)BPMS[bi] * MINUTES[bi], nb, m;
            reset(); mt_quiet();
            song.g[G_BPM] = (int16_t)BPMS[bi];
            grid_track(&trk[0], 2, 16, 4, 36);             /* 1/16: a note every 4 steps */
            grid_track(&trk[1], 4, 12, 3, 60);             /* 8T: every 3 (a 12/8 timeline's beat) */
            grid_track(&trk[2], 1, 8, 2, 72);              /* 1/8: every 2 */
            nb = (uint32_t)((uint64_t)beats * FS * 60u / (uint32_t)BPMS[bi] / CTL) + 2u;
            seq_start();
            grid_run(nb);
            seq_stop();
            if (grid_n[0] < beats || grid_n[1] < beats || grid_n[2] < beats || grid_n[3] < beats)
                okd = 0;
            for (m = 0; m < beats && m < GRID_MAX && okd; m++) {
                uint32_t want = (uint32_t)(((uint64_t)m * FS * 60u / (uint32_t)BPMS[bi] + CTL - 1u) / CTL);
                if (grid_blk[0][m] != want || grid_blk[1][m] != want || grid_blk[2][m] != want || grid_blk[3][m] != want) {
                    printf("  %d BPM beat %u: 1/16 %u, 8T %u, 1/8 %u, EDDA %u, the exact beat %u\n", BPMS[bi], m, grid_blk[0][m],
                           grid_blk[1][m], grid_blk[2][m], grid_blk[3][m], want);
                    okd = 0;
                }
            }
        }
        ck("the exact grid: 1/16, 8T and 1/8 tracks and EDDA's beat on every beat, 10 min at 128 BPM, 2 at 97 and 173, no drift", okd);
        {   /* the ARP: its steps at the exact grid's lengths from its key, the remainder kept (before: 0.4 % slow) */
            track_t *t = &trk[0];
            uint32_t b, i, top = 0, n0 = 0, cnt = 0, first = 0, last = 0, c = 256;
            reset(); mt_quiet();
            song.g[G_BPM] = 128;
            track_defaults_steps(t);
            t->p[P_AMODE] = 1; t->p[P_ARATE] = 2; t->p[P_AOCT] = 1; t->p[P_ASWING] = 0; t->p[P_APROB] = 127;
            arp_add(t, 60);
            for (i = 0; i < NVOICE; i++)
                top = t->v[i].age > top ? t->v[i].age : top;
            for (b = 0; b < 400000u && cnt <= c; b++) {
                uint32_t nt = top;
                events_block(CTL);
                for (i = 0; i < NVOICE; i++)
                    if (t->v[i].age > top) {
                        if (!cnt) first = b;
                        if (cnt == c) last = b;
                        cnt++;
                        nt = t->v[i].age > nt ? t->v[i].age : nt;
                    }
                top = nt;
            }
            n0 = (uint32_t)(((uint64_t)c * FS * 60u / (128u * 4u) + CTL - 1u) / CTL);   /* 256 sixteenths */
            ok = cnt > c && last - first >= n0 - 1u && last - first <= n0 + 1u;
            if (!ok) printf("  ARP: %u steps in %u blocks, the exact grid %u\n", c, last - first, n0);
        }
        ck("the ARP at 1/16, 128 BPM: 256 steps in the exact grid's time from its key (the remainder kept)", ok);
    }
    /* ------------------------------------------------------ dotted echoes */
    {
        uint32_t q;
        int ok;
        reset();
        song.g[G_BPM] = 120;
        q = beat_samples();
        ok = GP[G_DTIME].names == N_DLYDIV && GP[G_DTIME].max == 12 && str_eq(N_DLYDIV[10], "1/4D") &&
             str_eq(N_DLYDIV[11], "1/8D") && str_eq(N_DLYDIV[12], "1/16D") && str_eq(N_DLYDIV[9], "4BAR") &&
             NELEM(N_DIV) == 10u;                        /* (SEQ DIV and ARP RATE as before) */
        ok &= div_samples(10) == q * 3u / 2u && div_samples(11) == q * 3u / 4u && div_samples(12) == q * 3u / 8u &&
              div_samples(0) == q && div_samples(1) == q / 2u;
        song.g[G_DTIME] = 11;
        ok &= delay_samples() == q * 3u / 4u;
        song.g[G_DTIME] = 1;
        ok &= delay_samples() == q / 2u;
        ok &= param_turn(&GP[G_DTIME], 1, 1) == 12 && param_turn(&GP[G_DTIME], 12, 1) == 4 &&   /* 1/8 -> 1/16D -> 8T */
              param_turn(&GP[G_DTIME], 0, -1) == 10 && param_turn(&GP[G_DTIME], 10, -1) == 6;   /* 1/4 -> 1/4D -> 1/2 */
        ck("dotted echoes: DLY TIME 1/4D 1/8D 1/16D (3/2 of the plain ones, values 10..12), between their neighbours on the knob", ok);
    }
    /* ------------------------------------------------------ SEQ OUT */
    {
        track_t *t = &trk[0], *d = &trk[3];
        int ok;
        uint32_t loops, b0;
        char txt[2048];
        static const char LOOP[] = "+60/90 -60 +62/127 +65/127 +69/127 -62 -65 -69 +48/100 +55/100 -48 -55 +57/80 -57 "
                                   "+59/80 -59 +59/80 -59 +59/80 -59";
        reset(); edda.cues = 0; mt_quiet();
        track_defaults_steps(t);
        t->p[P_SLEN] = 8; t->p[P_SDIV] = 2; t->p[P_SSWING] = 0; t->p[P_AMODE] = 0; t->p[P_SGATE] = 64;
        t->step[0] = (step_t){{60, 0, 0, 0}, 1, ST_NOTE, 0, 90, 0, 0};
        t->step[1] = (step_t){{62, 65, 69, 0}, 3, ST_NOTE, SF_ACCENT, 90, 0, 0};   /* a chord, accented */
        t->step[2] = (step_t){{0, 0, 0, 0}, 0, ST_REST, 0, 0, 0, 0};
        t->step[3] = (step_t){{48, 0, 0, 0}, 1, ST_NOTE, SF_SLIDE, 100, 0, 0};      /* a slide into 55 */
        t->step[4] = (step_t){{55, 0, 0, 0}, 1, ST_NOTE, 0, 100, 0, 0};
        t->step[5] = (step_t){{57, 0, 0, 0}, 1, ST_NOTE, 0, 80, 0, 0};
        t->step[6] = (step_t){{0, 0, 0, 0}, 0, ST_TIE, 0, 0, 0, 0};                  /* 57 held through it */
        t->step[7] = (step_t){{59, 0, 0, 0}, 1, ST_NOTE, 0, 80, 0, 0};
        step_set_ratchet(&t->step[7], 3);
        blocks(2);
        mo_reset();
        seq_start(); mo_run(4u * 22050u / CTL);                                          /* two bars, OFF */
        transport_req = 2; mo_run(2);
        ok = mo_n == 0u && !edda.seq_out;
        ck("SEQ OUT OFF (the default): the sequencer sends nothing (the keys still do)", ok);
        menu_put(MI_SEQOUT, 1);
        mo_reset();
        seq_start(); mo_run(8u * 22050u / CTL + 1u);                                     /* two bars: 4 loops, the 5th not yet */
        transport_req = 2; mo_run(2);
        mo_text(0, txt, sizeof txt);
        for (ok = 1, loops = 0, b0 = 0; loops < 4u; loops++) {
            ok &= !strncmp(txt + b0, LOOP, sizeof LOOP - 1u);
            b0 += sizeof LOOP;                                                           /* (the space after it) */
        }
        if (!ok) printf("  %s\n", txt);
        ck("NOTES: channel 1, the velocities (accent 127), the chord, a slide legato (55 on before 48 off), a TIE held, RATCH x3", ok);
        ok = mo_pairs() == 4 * 10;
        ck("every note-on has its note-off, never on twice, none left on after STOP", ok);
        {   /* the slide: 48's note-off right after 55's note-on, in the block step 5 plays in (legato, not at 55's gate) */
            uint32_t i, on55 = mo_n, off48 = mo_n;
            for (i = 0; i < mo_n && (on55 == mo_n || off48 == mo_n); i++) {
                if (on55 == mo_n && mo_on(i) && mo_d1(i) == 55u)
                    on55 = i;
                if (off48 == mo_n && mo_off(i) && mo_d1(i) == 48u)
                    off48 = i;
            }
            ok = on55 < mo_n && off48 == on55 + 1u && mo_blk[off48] == mo_blk[on55];
            ck("a slide on MIDI: the next note on, then (the same block) the slid-from note off: legato", ok);
        }
        {   /* each step's note-ons leave in the block the step fires in */
            uint32_t i, k, j;
            for (ok = st_n >= 30u, k = 0; k < st_n; k++) {
                const step_t *st = &t->step[st_idx[k]];
                if (st->time != ST_NOTE)
                    continue;
                for (j = 0; j < st->n; j++) {
                    for (i = 0; i < mo_n && !(mo_on(i) && mo_d1(i) == st->note[j] && mo_blk[i] == st_blk[k]); i++)
                        ;
                    ok &= i < mo_n;
                }
            }
            ck("each step's notes go out in the very block the step plays in", ok);
        }
        /* drums: the lanes as their GM notes on the drum track's channel (4); the run's lanes never go out */
        reset(); edda.cues = 0; mt_quiet(); menu_put(MI_SEQOUT, 1);
        track_defaults_steps(t); t->p[P_SLEN] = 4;
        d->p[P_SLEN] = 4; d->p[P_SDIV] = 2; d->p[P_SSWING] = 0;
        d->step[0] = (step_t){{0, 0, 0, 0}, 0, ST_NOTE, 0, 100, 1u | 8u, 1u, 0};       /* kick (accent) + hat */
        d->step[1] = (step_t){{0, 0, 0, 0}, 0, ST_NOTE, 0, 100, 8u, 0, 0};
        d->step[2] = (step_t){{0, 0, 0, 0}, 0, ST_NOTE, 0, 100, 2u | 8u, 0, 0};          /* snare + hat */
        blocks(2); mo_reset();
        seq_start(); mo_run(22050u / CTL);
        transport_req = 2; mo_run(2);
        mo_text(3, txt, sizeof txt);
        ok = !strcmp(txt, "+36/127 +42/100 -36 -42 +42/100 -42 +38/100 +42/100 -38 -42") && mo_count(0x93u) == 5u &&
             mo_count(0x90u) == 0u && mo_pairs() == 5;
        ok &= strstr(txt, "+38/100") != 0;
        if (!ok) printf("  %s\n", txt);
        mo_reset();
        edda_lane_mute = 8u;                                                             /* (as the run: the hats rest) */
        seq_start(); mo_run(22050u / CTL);
        transport_req = 2; mo_run(2);
        edda_lane_mute = 0;
        mo_text(3, txt, sizeof txt);
        ok &= !strstr(txt, "42") && mo_count(0x93u) == 2u && mo_pairs() == 2;
        ck("drum lanes: their GM notes on the track's channel (KICK 36, SNARE 38, HAT 42), accents 127; rested lanes never sent", ok);
        /* mutes: a muted track starts nothing; one muted while its note sounds still ends it; the FX layer's mute too */
        reset(); edda.cues = 0; mt_quiet(); menu_put(MI_SEQOUT, 1);
        track_defaults_steps(t);
        t->p[P_SLEN] = 4; t->p[P_SDIV] = 2; t->p[P_SGATE] = 120;
        t->step[0] = (step_t){{60, 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
        t->step[2] = (step_t){{64, 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
        blocks(2); mo_reset();
        seq_start(); mo_run(1);
        ok = mo_count(0x90u) == 1u;                                                      /* 60 on */
        t->p[P_MUTE] = 1;
        mo_run(22050u / CTL);                                                            /* 60 ends, 64 never starts */
        ok &= mo_count(0x90u) == 1u && mo_count(0x80u) == 1u;
        t->p[P_MUTE] = 0;
        pf.act |= 1u << PF_M1;                                                           /* (the FX layer's mute) */
        mo_run(22050u / CTL);
        ok &= mo_count(0x90u) == 1u;
        pf.act &= ~(1u << PF_M1);
        mo_run(22050u / CTL);
        ok &= mo_count(0x90u) == 3u;
        transport_req = 2; mo_run(2);
        ok &= mo_pairs() == 3;
        ck("MUTE (and the FX layer's): a muted track sends no note-on, the note it was playing still ends", ok);
        /* SEQ OUT turned OFF with a note on: it ends at once; nothing after */
        reset(); edda.cues = 0; mt_quiet(); menu_put(MI_SEQOUT, 1);
        track_defaults_steps(t);
        t->p[P_SLEN] = 1; t->p[P_SDIV] = 9;                                               /* (one long step) */
        t->step[0] = (step_t){{60, 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
        blocks(2); mo_reset();
        seq_start(); mo_run(4);
        ok = mo_count(0x90u) == 1u && mo_count(0x80u) == 0u;
        menu_put(MI_SEQOUT, 0); mo_run(1);
        ok &= mo_count(0x80u) == 1u;
        mo_run(4u * 22050u / CTL);
        ok &= mo_n == 2u && mo_pairs() == 1;
        transport_req = 2; mo_run(2);
        ck("SEQ OUT OFF while a note sounds: its note-off at once, then nothing", ok);
        /* the queue full: a note-off is owed, sent first when there is room; a note-on leaves room for them */
        reset(); edda.cues = 0; mt_quiet(); menu_put(MI_SEQOUT, 1);
        track_defaults_steps(t);
        t->p[P_SLEN] = 2; t->p[P_SDIV] = 2; t->p[P_SGATE] = 64;
        t->step[0] = (step_t){{60, 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
        t->step[1] = (step_t){{62, 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
        blocks(2); mo_reset();
        seq_start(); events_block(CTL);                                                  /* 60 on (not drained) */
        ok = mo_w - mo_r == 1u;
        while (mo_w - mo_r < MQ)
            midi_out_event(0xFEu << 8 | 0x0Fu);                                          /* (a host not reading) */
        for (k = 0; k < 2000u && t->seq_n; k++)
            events_block(CTL);                                                           /* 60's gate ends */
        ok &= seq_mo_owe[0][1] == 1u << (60 - 32) && !seq_mo[0][1];
        for (k = 0; k < 2000u && t->seq_idx != 1u; k++)
            events_block(CTL);                                                           /* 62: no room, not sent */
        ok &= !(seq_mo[0][1] >> (62 - 32) & 1u);
        mo_r = mo_w - 4u;                                                                /* (the host reads a few) */
        events_block(CTL);
        ok &= !seq_mo_owe[0][1] && midi_out_q[(mo_w - 1u) % MQ] == (0x08u | 0x80u << 8 | 60u << 16);
        ck("the queue full: 60's note-off owed and sent first when the host reads; 62 not started (room kept for offs)", ok);
        mo_r = mo_w;
        transport_req = 2; blocks(2);
        /* +CLOCK: START and the first pulse with step 1, 24 a beat on the exact grid (the steps' own), STOP after the
         * notes' offs; free-running while stopped; nothing on an external clock */
        reset(); edda.cues = 0; mt_quiet(); menu_put(MI_SEQOUT, 2);
        track_defaults_steps(t);
        t->p[P_SLEN] = 16; t->p[P_SDIV] = 2; t->p[P_SSWING] = 0; t->p[P_SGATE] = 64;
        for (k = 0; k < 16u; k++)
            t->step[k] = (step_t){{(uint8_t)(48u + k), 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
        song.g[G_BPM] = 128;
        blocks(2); mo_reset();
        mo_run(44100u / CTL);                                                            /* a second, stopped */
        k = mo_count(0xF8u);
        ok = k >= 50u && k <= 52u && !mo_count(0xFAu);                                    /* (128 BPM: 51.2 a second) */
        if (!ok) printf("  stopped: %u pulses\n", k);
        mo_reset();
        seq_start();
        mo_run(10u * 60u * 44100u / CTL);                                                /* ten minutes */
        {
            uint32_t i, f = 0, j = 0, fa = mo_n, steps_ok = 1, nsteps = 0;
            for (i = 0; i < mo_n && fa == mo_n; i++)
                if (mo_st(i) == 0xFAu)
                    fa = i;
            ok &= fa == 0u && mo_st(1) == 0xF8u && mo_blk[0] == 0u && mo_st(2) == 0x90u;   /* START, a pulse, step 1 */
            /* pulse 6k with step k (1/16): the same block, every step for ten minutes */
            for (i = 0; i < mo_n; i++) {
                if (mo_st(i) != 0xF8u)
                    continue;
                if (f % 6u == 0u) {
                    while (j < st_n && st_blk[j] < mo_blk[i])
                        j++;
                    steps_ok &= j < st_n && st_blk[j] == mo_blk[i];
                    nsteps++;
                }
                f++;
            }
            {   /* the count: pulse j at floor(j N / 24 D) samples (N 2646000, D 128): those at or before the last block */
                uint64_t x = (uint64_t)(mo_b - 1u) * CTL, want = ((x + 1u) * 24u * 128u + 2646000u - 1u) / 2646000u;
                ok &= f == (uint32_t)want;
                if (f != (uint32_t)want) printf("  pulses %u, the grid %u\n", f, (uint32_t)want);
            }
            ok &= steps_ok && nsteps > 5000u;
        }
        transport_req = 2; mo_run(2);
        {   /* STOP once, after every note's off (the clock runs on: the pulses after it) */
            uint32_t i, fc = mo_n;
            for (i = 0; i < mo_n; i++) {
                if (mo_st(i) == 0xFCu)
                    fc = i;
                ok &= !(fc < mo_n && mo_st(i) < 0xF0u);
            }
            ok &= fc < mo_n && mo_pairs() == 5120 && mo_count(0xFCu) == 1u;
        }
        ck("+CLOCK: START then a pulse with step 1; 24 a beat, pulse 6k in step k's block for 10 min at 128 BPM; STOP after the offs", ok);
        mo_reset();
        transport_req = 3; song.playing = 1; mo_run(4);                                  /* (GLO + PLAY from stopped: a restart) */
        ok = mo_count(0xFAu) == 1u;
        transport_req = 3; mo_run(4);                                                    /* GLO + PLAY playing: STOP, START */
        ok &= mo_count(0xFCu) == 1u && mo_count(0xFAu) == 2u;
        transport_req = 2; mo_run(2);
        mo_reset();
        song.g[G_CLOCK] = 1; mo_run(4);                                                  /* external clock: nothing of ours */
        transport_req = 1; mo_run(44100u / CTL);
        ok &= !mo_count(0xF8u) && !mo_count(0xFAu) && !mo_count(0xFCu);
        song.g[G_CLOCK] = 0; mo_run(4);
        mo_reset(); mo_run(44100u / CTL);
        ok &= mo_count(0xF8u) >= 50u && !mo_count(0xFAu);
        ck("GLO + PLAY: STOP and START; CLK EXT: no clock, START or STOP of ours; back on INT: the pulses again", ok);
        /* +CLOCK chosen while playing: the pulses from now, no START mid-bar (the next PLAY sends it) */
        reset(); edda.cues = 0; mt_quiet(); menu_put(MI_SEQOUT, 1);
        blocks(2); seq_start(); blocks(100);
        mo_reset(); menu_put(MI_SEQOUT, 2); mo_run(22050u / CTL);
        ok = !mo_count(0xFAu) && mo_count(0xF8u) >= 23u && mo_count(0xF8u) <= 25u;
        menu_put(MI_SEQOUT, 1); mo_run(4);
        ok &= mo_count(0xFCu) == 0u;                                                     /* (never started: no STOP) */
        menu_put(MI_SEQOUT, 2); transport_req = 2; mo_run(2); transport_req = 1; mo_run(2);
        ok &= mo_count(0xFAu) == 1u;
        menu_put(MI_SEQOUT, 1); mo_run(2);
        ok &= mo_count(0xFCu) == 1u;                                                     /* (+CLOCK left while running) */
        transport_req = 2; mo_run(2);
        ck("+CLOCK chosen while playing: the pulses, no START mid-bar; NOTES again: STOP for what had started", ok);
        /* the run on SEQ OUT: the stabs' velocity follows their ramp, from LOG on they rest; the lead (muted) is silent */
        reset(); edda.cues = 0; mt_quiet(); menu_put(MI_SEQOUT, 1);
        {
            track_t *sb = &trk[ED_T_STABS];
            uint32_t i, v_first = 0, v_last = 0, log_on = 0, n_st = 0;
            track_defaults_steps(sb);
            sb->p[P_SLEN] = 4; sb->p[P_SDIV] = 2; sb->p[P_SGATE] = 32; sb->p[P_LEVEL] = 100;
            sb->step[0] = (step_t){{67, 0, 0, 0}, 1, ST_NOTE, 0, 120, 0, 0};
            blocks(2); mo_reset();
            seq_start(); mo_run(1);
            edda_run_request(); to_next_bar();                                           /* SHAKERS */
            mo_run(4u * 22050u / CTL + 4u);                                              /* (a bar of it) */
            mo_reset();
            mo_run(4u * 22050u / CTL);                                                   /* STABS, a bar */
            for (i = 0; i < mo_n; i++)
                if (mo_on(i) && (mo_st(i) & 15u) == ED_T_STABS) {
                    v_first = v_first ? v_first : mo_d2(i);
                    v_last = mo_d2(i);
                    n_st++;
                }
            ok = edda.phase == ED_LOG && n_st >= 3u && v_first <= 120u && v_last < v_first && v_last < 40u;
            if (!ok) printf("  stabs: %u ons, %u .. %u, phase %u\n", n_st, v_first, v_last, edda.phase);
            mo_reset(); mo_run(8u * 22050u / CTL);                                       /* LOG: two bars */
            for (i = 0; i < mo_n; i++)
                log_on += mo_on(i) && (mo_st(i) & 15u) == ED_T_STABS;
            ok &= !log_on;
            transport_req = 2; mo_run(2);
            ck("the run on SEQ OUT: the stabs fade by velocity over STABS, none from LOG on (a LEVEL itself is never sent)", ok);
        }
        /* the setting is kept: edda_prefs, applied at power-on; a stored 3 (no value) reads as OFF */
        reset();
        menu_put(MI_SEQOUT, 2); menu_put(MI_CUES, 1); menu_put(MI_RUNLEN, 1); menu_put(MI_REVEAL, 1);
        ok = edda_prefs == (1u | 2u << 1 | 1u << 3 | 1u << 4) && edda_prefs_bits() == edda_prefs;
        {
            persist_t p;
            uint8_t keep = edda_prefs;
            memset(&p, 0, sizeof p);
            settings_export(&p);
            ok &= p.favorites.factory[15][26] == keep;
            edda_defaults();
            ok &= !edda.seq_out && !edda.cues && !edda.run_len && !edda.reveal;
            edda_prefs_apply(edda_prefs);
            ok &= edda.seq_out == 2u && edda.cues && edda.run_len && edda.reveal;
            menu_put(MI_SEQOUT, 0);
            ok &= edda_prefs == (1u | 1u << 3 | 1u << 4);
            p.favorites.factory[15][26] = (uint8_t)(6u | 1u | 0x80u);                    /* SEQ OUT 3; a future bit */
            ok &= settings_import(&p, sizeof p) && p.favorites.factory[15][26] == (1u | 0x80u);
            edda_prefs_apply(edda_prefs);
            ok &= !edda.seq_out && edda.cues && !edda.run_len && !edda.reveal;
            menu_put(MI_REVEAL, 1);
            ok &= edda_prefs == (1u | 1u << 4 | 0x80u);                                  /* (the future bit kept) */
        }
        ck("SHOW CUES, SEQ OUT, RUN, REVEAL kept with the settings (one byte); applied at power-on; SEQ OUT 3 reads OFF", ok);
        reset();
    }
    /* ------------------------------------------------------ the user kits */
    {
        int ok = ENG_DRUM.edit[0].max == DK_COUNT - 1 && DK_COUNT == 13u && str_eq(N_DRUM_KIT[DK_USR1], "USR1") &&
                 str_eq(N_DRUM_KIT[DK_USR4], "USR4") && drum_kit_plays(DK_USR4) == DK_USR4 && DK_USR == 9u;
        int16_t pp[P_COUNT];
        memset(pp, 0, sizeof pp);
        pp[P_E0] = DK_USR2;
        ok &= drum_user_kit(pp) == 1u && smp_user_addr(3) == 0xE7000u && smp_user_addr(2) == 0xC8000u &&
              SMP_USER_SLOTS == 4u && str_eq(SMP_ALL_NAMES[SMP_NALL - 1u], "USR4");
#ifdef FL_STORE_OK
        ok &= FL_STORE_OK(0xE7000u, 0x14000u) && !FL_STORE_OK(0xFB000u, 0x1000u) && !FL_STORE_OK(0xE6000u, 0x2000u);
#endif
        ck("DRUM KIT: USR1..USR4 after the synth kits (values 9..12); USR4 is the slot at 0xE7000 (in the flash guard)", ok);
        {   /* the track with a user kit: the synth kit's lane names, no model swaps */
            track_t *t = &trk[0];
            reset();
            kit_track(t, DK_USR4);
            ok = drum_swaps(t) == 0u && str_eq(drum_lane_name(t, DV_TOM), "TOM") && str_eq(drum_lane_abbr(t, DV_BELL), "CB");
            ck("a user kit keeps the lanes' names (KICK .. BELL), no model-kit swap", ok);
        }
    }
    if (!kit) {
        printf("edda: (no kit given: the user kit checks skipped; tests/edda_kit.py builds one)\n");
    } else {
        track_t *t = &trk[0];
        int ok;
        uint32_t pk, nb[2];
        voice_t *v;
        reset();
        ok = kit_load(kit, 0) && kit_load(kit, 3);
        for (i = 0; ok && i < 16u; i++)
            ok &= usr_zone[0][i].lo == KIT_PAD_NOTES[i] && usr_zone[0][i].hi == KIT_PAD_NOTES[i] &&
                  usr_zone[0][i].root16 == KIT_PAD_NOTES[i] * 16 && !usr_zone[0][i].looped &&
                  usr_zone[0][i].rate == ((i == 12u || i == 14u) ? 16384u : 32768u);   /* crash, ride: 11025 Hz */
        ok &= smp_user_zone(0, 36) == 0x8000u && smp_user_zone(0, 53) == (0x8000u | 15u) && smp_user_zone(0, 44) == 0xFFFFu &&
              smp_user_zone(3, 38) == (0x8000u | 3u << 5 | 1u);
        ck("the kit built by tests/edda_kit.py: 16 pads on their notes, the cymbals at 11025 Hz (the fit), in USR1 and USR4", ok);

        /* every pad plays its zone, one-shot, at its own level; the voice ends with the sound */
        kit_track(t, DK_USR1);
        ok = 1;
        for (i = 0; i < 16u; i++) {
            uint32_t note = KIT_PAD_NOTES[i];
            trk_note_on(t, note, 100);
            v = kvoice(t, note);
            ok &= v && v->s[6] == 1 && (uint32_t)v->s[5] == (0x8000u | i) && v->s[0] == (int32_t)drum_lane(note) &&
                  (v->ph[2] >> 24) == 64u;
            pk = render_peak(8);
            ok &= pk > 1500u;
            trk_note_off(t, note);                       /* (a hit does not end with its key) */
            ok &= kvoice(t, note) != 0;
            k = blocks_until_end(t, note, 6000);
            ok &= k < 6000u && k > 2u;
            if (!ok) { printf("  pad %u (note %u): peak %u, ended after %u blocks\n", i + 1u, note, pk, k); break; }
        }
        ck("KIT USR1: each of the 16 pads plays its sound (one-shot: the key let go changes nothing), the voice ends with it", ok);

        /* the GM fall-back: a note without a pad plays its lane's pad, pitched by its GM semitones */
        kit_track(t, DK_USR1);
        trk_note_on(t, 44, 100);                         /* pedal hat: HAT CL -2 st */
        v = kvoice(t, 44);
        ok = v && v->s[6] == 1 && (uint32_t)v->s[5] == (0x8000u | 3u) && (v->ph[2] >> 24) == 62u;
        trk_note_on(t, 35, 100);                         /* kick 2: KICK -2 st */
        v = kvoice(t, 35);
        ok &= v && (uint32_t)v->s[5] == 0x8000u && (v->ph[2] >> 24) == 62u;
        trk_note_on(t, 57, 100);                         /* crash 2: lane BELL (the GM map): the bell pad, +1 st */
        v = kvoice(t, 57);
        ok &= v && (uint32_t)v->s[5] == (0x8000u | 7u) && (v->ph[2] >> 24) == 65u;
        ck("a note with no pad of its own plays its lane's pad at its GM semitones (44 -> HAT CL -2, 35 -> KICK -2)", ok);

        /* TUNE: +12 semitones plays the sound twice as fast (it ends in half the blocks) */
        kit_track(t, DK_USR1);
        trk_note_on(t, 45, 100); nb[0] = blocks_until_end(t, 45, 6000);
        t->p[P_E1] = 127;
        trk_note_on(t, 45, 100); nb[1] = blocks_until_end(t, 45, 6000);
        ok = nb[0] > 150u && nb[1] * 2u + nb[0] / 40u >= nb[0] && nb[1] * 2u <= nb[0] + nb[0] / 40u;
        ck("TUNE 127 (+12 st): the tom ends in half the blocks", ok);

        /* DECY below 64 shortens: the bell (0.5 s) at DECY 10 is gone within 0.2 s */
        kit_track(t, DK_USR1);
        trk_note_on(t, 56, 100); nb[0] = blocks_until_end(t, 56, 6000);
        t->p[P_E3] = 10;
        trk_note_on(t, 56, 100); nb[1] = blocks_until_end(t, 56, 6000);
        ok = nb[0] >= 330u && nb[1] < 200u && nb[1] > 60u;
        t->p[P_E3] = 100;
        trk_note_on(t, 56, 100); k = blocks_until_end(t, 56, 6000);
        ok &= k == nb[0];
        ck("DECY 10 ends the bell in about 0.1 s (an exponential decay); DECY 64 and up: the sound whole", ok);

        /* SNAP: above 64 skips into the sound (not a sound too short for it), below 64 fades it in */
        kit_track(t, DK_USR1);
        t->p[P_E4] = 127;
        trk_note_on(t, 36, 100); v = kvoice(t, 36);
        ok = v && v->ph[0] == 2205u;
        trk_note_on(t, 37, 100); v = kvoice(t, 37);     /* the rim: 882 samples: no skip */
        ok &= v && v->ph[0] == 0u;
        t->p[P_E4] = 1;
        voices_off();
        trk_note_on(t, 38, 100); v = kvoice(t, 38);
        ok &= v && v->ph[0] == 0u && v->s[4] == 0 && (v->ph[2] & 0xFFu) == 69u;
        pk = render_peak(1);
        ok &= pk < 400u;                                 /* the fade's first block: nearly silent (231) */
        render_peak(80);
        ok &= v->active && v->s[4] == (1 << 24) && (v->ph[2] & 0xFFu) == 0u;
        ck("SNAP 127 skips 50 ms into the kick (never a 40 ms rim), SNAP 1 fades the snare in over 50 ms", ok);

        /* the closed hat chokes the open one; one voice per pad; the lane LEVEL */
        kit_track(t, DK_USR1);
        trk_note_on(t, 46, 100); v = kvoice(t, 46);
        render_peak(2);
        trk_note_on(t, 42, 100);
        ok = v && v->s[6] == 3 && kvoice(t, 42);
        k = blocks_until_end(t, 46, 100);
        ok &= k < 20u;
        trk_note_on(t, 36, 100); render_peak(4); trk_note_on(t, 36, 100);
        v = kvoice(t, 36);
        ok &= kvoices(t) == 2u && v && v->ph[0] < 64u;  /* the kick again: its one voice, from the start */
        t->p[P_LN0] = 0;
        settle();
        k = render_peak(8);                              /* (what is left of the reverb's tail) */
        trk_note_on(t, 36, 100);
        pk = render_peak(8);
        ok &= pk <= k + 2u;
        t->p[P_LN0] = 127;
        ck("HAT CL chokes HAT OP (gone in 10 ms); a pad hit again restarts its one voice; LANE 1 LEVEL 0 silences the kick", ok);

        /* an empty kit (USR2 holds nothing): silent hits that end at once, nothing else disturbed */
        kit_track(t, DK_USR2);
        settle();
        k = render_peak(8);
        trk_note_on(t, 36, 100); v = kvoice(t, 36);
        ok = v && v->s[6] == 2;
        ok &= render_peak(8) <= k + 2u && !kvoice(t, 36);
        ck("KIT USR2 on an empty slot: a hit is silent and ends at once", ok);

        /* USR4: the same kit from the 4th slot, on DRUM and on SAMPLE SET 8 */
        kit_track(t, DK_USR4);
        trk_note_on(t, 38, 100); v = kvoice(t, 38);
        ok = v && (uint32_t)v->s[5] == (0x8000u | 3u << 5 | 1u) && render_peak(8) > 1500u;
        voices_off();
        song.master_q12 = 4096;
        host_preset(t, ENGI_SAMPLE, 0);
        t->p[P_E0] = (int16_t)(SMP_NALL - 1u);
        t->p[P_ATK] = 0; t->p[P_SUS] = 127;
        trk_note_on(t, 36, 100); v = kvoice(t, 36);
        ok &= v && (uint32_t)v->s[4] == (0x8000u | 3u << 5) && v->s[6] == 0 && render_peak(8) > 1500u;
        ck("USR4: DRUM KIT USR4 plays the kit from the 4th slot; SAMPLE SET 8 plays its kick", ok);
    }
    printf(bad ? "edda: %d FAILED\n" : "edda: all passed\n", bad);
    return bad != 0;
}
