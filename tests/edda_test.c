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
static int has_note(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < t->seq_n; i++) if (t->seq_notes[i] == note) return 1;
    return 0;
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
