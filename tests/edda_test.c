/* SPDX-License-Identifier: GPL-3.0-only */
/* EDDA OS (firmware/src/edda.c) on the host, against the real sequencer, UI and MIDI-out code:
 * the Camelot wheel (every position's root and scale, names, neighbours, the key path), the EDDA pattern bank
 * (lengths, divisions, the drum grids, melodic lines loaded into ROOT, triads), Euclidean rhythms (E(7, 12) is the
 * ogene timeline), the run (phases on the beat clock, the lane mask, the stabs' ramp, the restore at the drop, the
 * cues), the hard stop and the re-entry on the one, REVEAL (the hole every 8th bar, the backbeat's lane in bars
 * 17..24), MUTATE (the bell lane only), MENU > EDDA, the GLO layer's keys, and the FM6 voice bank.
 * Run by tests/run_tests.sh. */
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
static int has_note(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < t->seq_n; i++) if (t->seq_notes[i] == note) return 1;
    return 0;
}

int main(void)
{
    uint32_t i, j, k, n[3], m0;
    char nm[4];
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
        reset(); edda.cues = 0; seq_start(); m0 = mo_w; beats(5);
        ck("SHOW CUES OFF: not one byte on MIDI OUT", mo_w == m0);
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
        lay_combo(B_GLO, white(9)); ok &= edda.fill;
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
             menu_n(MI_REVEAL) == 2u && MI_TAB[MI_KEY] == MTAB_EDDA && MI_TAB[MI_REVEAL] == MTAB_EDDA &&
             str_eq(MTAB_NAME[MTAB_EDDA], "EDDA") && mtab_rows(MTAB_EDDA) == 5u && str_eq(MI_NAME[MI_KEY], "KEY") &&
             MI_KEY < MI_VALUES && MI_REVEAL < MI_VALUES;
        ck("MENU > EDDA: KEY (25 values), SHOW CUES, ACT (5), RUN, REVEAL; all value rows, one tab", ok);
        ok = str_eq(menu_vname(MI_KEY, 15), "8A") && str_eq(menu_vname(MI_KEY, 0), "OFF") && str_eq(menu_vname(MI_KEY, 24), "12B") &&
             str_eq(menu_vname(MI_ACT, 0), "1 BULB") && str_eq(menu_vname(MI_ACT, 4), "5 BULBS") &&
             str_eq(menu_vname(MI_RUNLEN, 1), "LONG") && str_eq(menu_vname(MI_CUES, 1), "ON");
        ck("the rows' value names", ok);
        menu_put(MI_KEY, 15);
        ok = menu_get(MI_KEY) == 15u && trk[0].p[P_ROOT] == 9 && trk[0].p[P_SCALE] == (int16_t)ED_SC_MIN &&
             cues_cc(0, ED_CUE_CC_KEY, &m0) == 1u && m0 == 15u;
        m0 = mo_w;
        menu_put(MI_ACT, 2); ok &= edda.act == 3 && menu_get(MI_ACT) == 2u && cues_cc(m0, ED_CUE_CC_ACT, &k) == 1u && k == 3u;
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
        ok = ui.layer == LAYER_GLO && edda.armed && !gates();
        key_up(white(5)); btn_up(B_GLO); frame();
        lay_combo(B_GLO, white(8)); ok &= edda.act == 2;
        key_up(white(8)); btn_up(B_GLO); frame();
        lay_combo(B_GLO, white(12)); ok &= edda.camelot == 17u;   /* (from the selected track's A minor 8A: 9A) */
        key_up(white(12)); btn_up(B_GLO); frame();
        lay_combo(B_GLO, white(11)); ok &= edda.camelot == 15u;
        key_up(white(11)); btn_up(B_GLO); frame();
        lay_combo(B_GLO, white(13)); ok &= edda.camelot == 16u;
        key_up(white(13)); btn_up(B_GLO); frame();
        lay_combo(B_GLO, white(10)); ok &= edda.mutate;
        key_up(white(10)); btn_up(B_GLO); frame();
        lay_combo(B_GLO, white(6)); ok &= edda.stopped;
        key_up(white(6)); btn_up(B_GLO); frame();
        ok &= !ui.layer && !gates();
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
    printf(bad ? "edda: %d FAILED\n" : "edda: all passed\n", bad);
    return bad != 0;
}
