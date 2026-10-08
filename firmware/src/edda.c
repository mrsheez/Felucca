/* SPDX-License-Identifier: GPL-3.0-only
 * EDDA OS: the Mr. Sheez performance layer on Felucca. Included at the end of seq.c (it uses the
 * sequencer's statics: beat_n, beat_samples(), midi_out_event, trk[]). See EDDA-OS.md.
 *
 *   Camelot key lock   MENU > EDDA > KEY: 1A..12B sets ROOT and SCALE (minor for A, major for B) on every
 *                      synth track and turns the key quantiser on, so the keys and the step notes stay in the
 *                      key of the record on the decks. edda_camelot_apply.
 *   EDDA patterns      SEQ > PATTERNS, after the factory ones ("E01".."E16"): drum grids (3-step, sgija, gqom,
 *                      kizomba, Afrobeat, the 12/8 ogene bell timeline, 3-2 clave, straight shakers with
 *                      accents) and root-relative melodic lines (log drum, sub, stabs, highlife guitar, oja
 *                      riff, talking drum, the bittersweet i-VI-III-VII). Loaded into the track's root.
 *   The run            GLO layer, D4: the transition by subtraction (the EDDA rule): shakers cut, stabs filtered
 *                      down, a two-bar log drum run, a beat of silence, then everything back on the one: the
 *                      drop. Phases are counted in beats from the sequencer's beat clock, so it lands on the
 *                      grid whatever the step length. SAVE held (undo) does not touch it; a stop (PLAY) ends it.
 *   Hard stop          GLO layer, E4: everything mutes at once, the clock keeps running; E4 again re-enters on
 *                      the next one (the hard stop -> re-entry of the EDDA set). No tempo change, no restart.
 *   Show cues          USB MIDI OUT channel 16: bar / beat / act / phase as CCs, bar / run / drop / stop as
 *                      notes, for the visuals rig (edda.h). MENU > EDDA > CUES.
 * Nothing here changes a step the user wrote except through SEQ > PATTERNS, and nothing is stored: the
 * settings reset at power-on (the key lives on in the project's ROOT / SCALE). */

/* ------------------------------------------------------------ Camelot --- */
/* value 1..24 -> the wheel's number (1..12) and letter (0 A minor, 1 B major) */
static uint32_t edda_cam_num(uint32_t v) { return (v - 1u) / 2u + 1u; }
static uint32_t edda_cam_major(uint32_t v) { return (v - 1u) & 1u; }
/* the root (pitch class, C = 0) of a Camelot position: 1A is G# minor; a step on the wheel is a fifth up;
 * B is the relative major, three semitones up */
static uint32_t edda_cam_root(uint32_t v)
{
    uint32_t a = (8u + 7u * (edda_cam_num(v) - 1u)) % 12u;
    return edda_cam_major(v) ? (a + 3u) % 12u : a;
}
#define ED_SC_MAJ 1u             /* params.c N_SCALE: MAJ, MIN */
#define ED_SC_MIN 2u
static uint32_t edda_cam_scale(uint32_t v) { return edda_cam_major(v) ? ED_SC_MAJ : ED_SC_MIN; }
/* "8A", "12B" (4 bytes) */
static void edda_cam_name(uint32_t v, char *out)
{
    uint32_t n = edda_cam_num(v);
    if (!v) {
        out[0] = 'O'; out[1] = 'F'; out[2] = 'F'; out[3] = 0;
        return;
    }
    if (n >= 10u) {
        *out++ = '1';
        n -= 10u;
    }
    *out++ = (char)('0' + n);
    *out++ = edda_cam_major(v) ? 'B' : 'A';
    *out = 0;
}
/* the Camelot position of a root and scale, 0 if the scale is neither major nor minor */
static uint32_t edda_cam_of(uint32_t root, uint32_t scale)
{
    uint32_t v;
    for (v = 1; v <= 24u; v++)
        if (edda_cam_root(v) == root % 12u && edda_cam_scale(v) == scale)
            return v;
    return 0;
}
/* the keys a DJ mixes into from v: the same number's other letter, and the numbers either side (the Camelot
 * one-step path of the EDDA set). out: 3 values */
static void edda_cam_neighbours(uint32_t v, uint32_t out[3])
{
    uint32_t n = edda_cam_num(v), m = edda_cam_major(v);
    out[0] = (n - 1u) * 2u + !m + 1u;
    out[1] = (n % 12u) * 2u + m + 1u;                 /* +1 on the wheel (a fifth up) */
    out[2] = ((n + 10u) % 12u) * 2u + m + 1u;         /* -1 */
}
/* KEY set: every synth track (not a kit) into the key; the quantiser to SNAP where it was OFF, so a key is never
 * out of the record's key with a mask on */
static void edda_camelot_apply(uint32_t v)
{
    uint32_t i;
    edda.camelot = (uint8_t)(v > 24u ? 0u : v);
    if (!v)
        return;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        if (drum_track(t))
            continue;
        t->p[P_ROOT] = (int16_t)edda_cam_root(v);
        t->p[P_SCALE] = (int16_t)edda_cam_scale(v);
        if (t->p[P_QUANT] == QN_OFF)
            t->p[P_QUANT] = QN_SNAP;
    }
}

/* ---------------------------------------------------------- patterns --- */
/* An EDDA pattern: a drum grid (kind 1: hit / acc bits per step, the DRUM lanes KICK SNARE CLAP HATCL HATOP TOM
 * RIM BELL = bits 0..7; vel: the plain hits' velocity, ghosts below 96) or a melodic line (kind 0: notes as
 * written in C, loaded into the track's ROOT; flags 1 accent, 2 slide, 4 tie). len 1..16 steps, div as N_DIV
 * (2 = 1/16, 4 = 8T for the 12-pulse timelines). The styles are the EDDA set's: the family identity (3-step kick,
 * log drum, straight sixteenths with velocity accents and no swing) and the lanes around it */
#define ED_KIND_MEL 0u
#define ED_KIND_DRUM 1u
#define ED_DIV_16 2u
#define ED_DIV_8T 4u
#define ED_T 4u                  /* tie */
#define K_ 1u                    /* lanes */
#define S_ 2u
#define C_ 4u
#define H_ 8u
#define O_ 16u
#define T_ 32u
#define R_ 64u
#define B_ 128u
typedef struct {
    const char *name;            /* at most 8 characters */
    uint8_t kind, len, div, vel; /* len 1..32: a two-bar phrase at 1/16 is 32 steps */
    uint32_t fill;               /* bit i: step i is fill-only (SF_FILL): it plays while FILL is on (GLO + A4) */
    uint8_t a[32];               /* notes (MEL) or hits (DRUM) */
    uint8_t b[32];               /* flags (MEL) or accents (DRUM) */
} edda_pat_t;
static const edda_pat_t EDDA_PAT[] = {
    /* drums: the 3-step kick (1, the and of 2, 4), dry rims on 2 and 4, straight closed hats accented on the
     * beats: the EDDA family identity */
    {"3STEP", ED_KIND_DRUM, 16, ED_DIV_16, 84, (1u << 13) | (1u << 14) | (1u << 15),
     {K_ | H_, H_, H_, H_, R_ | H_, H_, K_ | H_, H_, H_, H_, H_, H_, K_ | R_ | H_, T_ | H_, T_ | H_, S_ | T_ | H_},
     {K_ | H_, 0, 0, 0, R_ | H_, 0, K_, 0, H_, 0, 0, 0, K_ | R_ | H_, 0, T_, S_ | T_}},
    /* sgija / Pretoria bacardi bounce: a syncopated kick, claps answering, hats on the off-beats */
    {"SGIJA", ED_KIND_DRUM, 16, ED_DIV_16, 80, (1u << 14) | (1u << 15),
     {K_, 0, H_, K_, C_, 0, K_ | H_, 0, 0, 0, K_ | H_, 0, K_ | C_, 0, H_ | T_, K_ | T_},
     {K_, 0, 0, 0, C_, 0, K_, 0, 0, 0, 0, 0, K_ | C_, 0, 0, T_}},
    /* gqom: a broken kick, toms as the melody, no hats at all */
    {"GQOM", ED_KIND_DRUM, 16, ED_DIV_16, 90, 0,
     {K_, 0, 0, K_, 0, 0, K_, 0, 0, 0, T_, 0, T_ | K_, 0, T_, 0},
     {K_, 0, 0, 0, 0, 0, K_, 0, 0, 0, 0, 0, T_ | K_, 0, 0, 0}},
    /* kizomba: the kick with the push on the and of 2, rim clicks on 2 and 4, soft hats */
    {"KIZOMBA", ED_KIND_DRUM, 16, ED_DIV_16, 70, (1u << 15),
     {K_ | H_, 0, H_, 0, R_ | H_, 0, K_ | H_, 0, K_ | H_, 0, H_, 0, R_ | H_, 0, H_, R_},
     {K_, 0, 0, 0, R_, 0, K_, 0, 0, 0, 0, 0, R_, 0, 0, 0}},
    /* Afrobeat (the Africa 70 feel): closed hats in sixteenths, the open hat on the and of 2 and 4, the kick
     * on 1 and the and of 3, snare on 3 (a rim ghost on 2) */
    {"AFROBT", ED_KIND_DRUM, 16, ED_DIV_16, 76, (1u << 13) | (1u << 15),
     {K_ | H_, H_, H_, H_, R_ | H_, H_, O_, H_, S_ | H_, H_, K_ | H_, H_, H_, S_ | H_, O_, S_ | H_},
     {K_ | H_, 0, 0, 0, 0, 0, O_, 0, S_ | H_, 0, K_, 0, H_, 0, O_, 0}},
    /* the 12-pulse ogene / gankogui bell timeline (2 2 1 2 2 2 1) on the BELL lane, a kick on the first pulse
     * and the seventh: twelve steps at eighth triplets = one bar of 12/8 */
    {"OGENE12", ED_KIND_DRUM, 12, ED_DIV_8T, 96, 0,
     {B_ | K_, 0, B_, 0, B_, B_, K_, B_, 0, B_, 0, B_, 0, 0, 0, 0},
     {B_ | K_, 0, 0, 0, 0, 0, K_, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    /* 3-2 son clave on the RIM (CLAVE in the 77 kit), a kick under the one */
    {"CLAVE32", ED_KIND_DRUM, 16, ED_DIV_16, 100, 0,
     {R_ | K_, 0, 0, R_, 0, 0, R_, 0, 0, 0, R_, 0, R_, 0, 0, 0},
     {R_ | K_, 0, 0, 0, 0, 0, R_, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    /* straight sixteenth shakers (closed hats): the bounce from velocity, not timing: accents on the beats,
     * ghosts on the e and a (the EDDA timing rule: everything on the grid) */
    {"SHKR16", ED_KIND_DRUM, 16, ED_DIV_16, 60, 0,
     {H_, H_, H_, H_, H_, H_, H_, H_, H_, H_, H_, H_, H_, H_, H_, H_},
     {H_, 0, H_, 0, H_, 0, H_, 0, H_, 0, H_, 0, H_, 0, H_, 0}},
    /* melodic lines, written in C and loaded into ROOT. LOGDRUM: the amapiano log drum conversing with a sparse
     * kick: the root, the fifth below, the flat seventh below, slides into the one */
    {"LOGDRUM", ED_KIND_MEL, 16, ED_DIV_16, 100, 0,
     {36, 0, 0, 36, 0, 0, 31, 0, 0, 0, 36, 0, 34, 0, 36, 36},
     {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 2, 1}},
    /* LOG 2: the answer, a longer phrase up to the flat third and back */
    {"LOG2", ED_KIND_MEL, 16, ED_DIV_16, 100, 0,
     {36, 0, 36, 0, 0, 39, 0, 36, 0, 0, 34, 0, 36, 0, 31, 0},
     {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2, 0, 1, 0, 2, 0}},
    /* SUB: the deep rolling sub under it, held on the root, the fifth below on the and of 3 */
    {"SUBROLL", ED_KIND_MEL, 16, ED_DIV_16, 110, 0,
     {24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 19, 0, 0, 0, 24, 0},
     {1, ED_T, ED_T, ED_T, ED_T, ED_T, ED_T, ED_T, ED_T, 0, 1, ED_T, ED_T, 0, 2, ED_T}},
    /* STABS: skeletal minor stabs on the off-beats (the and of 1, the and of 2, the 4): root, flat third,
     * fifth as one chord each step */
    {"STABS", ED_KIND_MEL, 16, ED_DIV_16, 96, 0,
     {0, 0, 60, 0, 0, 0, 63, 0, 0, 0, 67, 0, 60, 0, 0, 0},
     {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}},
    /* BITTERSW: the i-VI-III-VII progression as stabs, one chord a beat, each stab on the and: 4 chords of 3
     * notes (the loader builds the triads: the step holds the root of the chord, flag 8 = build a triad) */
    {"BITTERSW", ED_KIND_MEL, 16, ED_DIV_16, 96, 0,
     {0, 0, 60, 0, 0, 0, 56, 0, 0, 0, 63, 0, 0, 0, 58, 0},
     {0, 0, 1 | 8, 0, 0, 0, 1 | 8, 0, 0, 0, 1 | 8, 0, 0, 0, 1 | 8, 0}},
    /* HILIFE: the highlife guitar line, palm-wine picking in sixteenths over I-IV-V-I (major; in a minor key
     * the quantiser keeps it in the scale) */
    {"HILIFE", ED_KIND_MEL, 16, ED_DIV_16, 90, 0,
     {60, 64, 67, 64, 65, 69, 72, 69, 67, 71, 74, 71, 72, 67, 64, 60},
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    /* OJA: a pentatonic flute call, a held note answered by a short figure */
    {"OJA", ED_KIND_MEL, 16, ED_DIV_16, 84, 0,
     {79, 0, 0, 0, 0, 0, 77, 75, 0, 72, 0, 0, 75, 0, 72, 0},
     {1, ED_T, ED_T, ED_T, ED_T, 0, 0, 0, 0, 1, ED_T, 0, 0, 0, 1, ED_T}},
    /* LOG2BAR: the log drum as a two-bar phrase (32 steps): the call in bar one, the answer with the flat third and a
     * long slide home in bar two; the SLOOP-style "steps up to two bars", on Felucca's 64-step tracks */
    {"LOG2BAR", ED_KIND_MEL, 32, ED_DIV_16, 100, 0,
     {36, 0, 0, 36, 0, 0, 31, 0, 0, 0, 36, 0, 34, 0, 36, 0, 36, 0, 0, 39, 0, 0, 36, 0, 0, 34, 0, 0, 31, 0, 0, 36},
     {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 2, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 2, 0, 0, 1, ED_T, ED_T, 2}},
    /* TALKDRM: the talking drum: slides between two pitches, a pair of quick strokes at the end of the bar */
    {"TALKDRM", ED_KIND_MEL, 16, ED_DIV_16, 100, 0,
     {55, 0, 60, 0, 0, 55, 0, 60, 0, 0, 55, 60, 0, 0, 62, 60},
     {1, 0, 2, 0, 0, 1, 0, 2, 0, 0, 0, 2, 0, 0, 1, 2}},
};
#define EDDA_NPAT (sizeof EDDA_PAT / sizeof EDDA_PAT[0])
#undef K_
#undef S_
#undef C_
#undef H_
#undef O_
#undef T_
#undef R_
#undef B_

/* pattern n (0..EDDA_NPAT-1) into track t: steps 1..len, the rest empty, LEN and DIV set; a melodic line is
 * transposed into the track's ROOT (C -> ROOT, downward from F#: the closest), a chord root with flag 8 gets
 * its triad from the track's scale (minor: 0 3 7, else 0 4 7). Drum hits as the grid, velocity from the
 * pattern, accents at 127. The caller wraps it in load_begin / load_end (ui.c pat_load) */
static void edda_pat_fill(track_t *t, uint32_t n)
{
    const edda_pat_t *p = &EDDA_PAT[n % EDDA_NPAT];
    int32_t root = t->p[P_ROOT] % 12;
    int32_t up = root > 6 ? root - 12 : root;
    uint32_t i, minor = t->p[P_SCALE] != (int16_t)ED_SC_MAJ;
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &t->step[i];
        s->note[0] = s->note[1] = s->note[2] = s->note[3] = 0;
        s->n = 0;
        s->time = ST_REST;
        s->flags = 0;
        s->vel = 0;
        s->hit = s->acc = 0;
        s->probability = 0;
        if (i >= p->len)
            continue;
        if (p->kind == ED_KIND_DRUM) {
            if (!p->a[i])
                continue;
            s->time = ST_NOTE;
            s->hit = p->a[i];
            s->acc = (uint8_t)(p->b[i] & p->a[i]);
            s->vel = p->vel;
            if ((p->fill >> i) & 1u)
                s->flags = SF_FILL;
        } else {
            uint32_t fl = p->b[i];
            if (fl & ED_T) {
                s->time = ST_TIE;
                continue;
            }
            if (!p->a[i])
                continue;
            s->time = ST_NOTE;
            s->note[0] = (uint8_t)clamp((int32_t)p->a[i] + up, 0, 127);
            s->n = 1;
            if (fl & 8u) {                            /* a triad on the chord's root */
                s->note[1] = (uint8_t)clamp((int32_t)s->note[0] + (minor ? 3 : 4), 0, 127);
                s->note[2] = (uint8_t)clamp((int32_t)s->note[0] + 7, 0, 127);
                s->n = 3;
            }
            s->flags = (uint8_t)(fl & (SF_ACCENT | SF_SLIDE));
            if ((p->fill >> i) & 1u)
                s->flags |= SF_FILL;
            s->vel = p->vel;
        }
    }
    t->p[P_SLEN] = (int16_t)p->len;
    t->p[P_SDIV] = (int16_t)p->div;
}

/* ------------------------------------------------- the run, the stop --- */
static void edda_cue(uint32_t cin_status, uint32_t d1, uint32_t d2)
{
    if (!edda.cues)
        return;
    midi_out_event(cin_status | d1 << 16 | d2 << 24);
}
static void edda_cue_note(uint32_t note)
{
    edda_cue(0x09u | (0x90u | ED_CUE_CH) << 8, note, 127u);
    edda_cue(0x08u | (0x80u | ED_CUE_CH) << 8, note, 0u);
}
static void edda_cue_cc(uint32_t cc, uint32_t v) { edda_cue(0x0Bu | (0xB0u | ED_CUE_CH) << 8, cc, v & 0x7Fu); }

/* the levels and mutes before the run or the stop, once (a stop during a run keeps the run's copy) */
static void edda_save_mix(void)
{
    uint32_t i;
    if (edda.phase != ED_IDLE || edda.stopped)
        return;
    for (i = 0; i < NTRK; i++) {
        edda.lvl[i] = trk[i].p[P_LEVEL];
        edda.mute[i] = (uint8_t)(trk[i].p[P_MUTE] != 0);
    }
}
static void edda_restore_mix(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) {
        trk[i].p[P_LEVEL] = edda.lvl[i];
        trk[i].p[P_MUTE] = edda.mute[i];
    }
    edda_lane_mute = 0;
}
static void edda_mute_all(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        trk[i].p[P_MUTE] = 1;
}
/* beats of a phase: SHORT 1 + 1 + 2 bars, LONG 2 + 2 + 2, the silence one beat */
static uint32_t edda_phase_beats(uint32_t phase)
{
    switch (phase) {
    case ED_SHAKERS: return edda.run_len ? 8u : 4u;
    case ED_STABS: return edda.run_len ? 8u : 4u;
    case ED_LOG: return 8u;
    case ED_SILENCE: return 1u;
    }
    return 0;
}
static void edda_phase_enter(uint32_t phase)
{
    edda.phase = (uint8_t)phase;
    edda.left = (uint8_t)edda_phase_beats(phase);
    edda_cue_cc(ED_CUE_CC_PHASE, phase);
    switch (phase) {
    case ED_SHAKERS:                                  /* the shakers cut: hats, claps, rims; the lead rests */
        edda_lane_mute = (uint8_t)((1u << 2) | (1u << 3) | (1u << 4) | (1u << 6));
        trk[ED_T_LEAD].p[P_MUTE] = 1;
        edda.stab_lvl0 = (uint32_t)(trk[ED_T_STABS].p[P_LEVEL] > 0 ? trk[ED_T_STABS].p[P_LEVEL] : 0);
        edda_cue_note(ED_CUE_NOTE_RUN);
        break;
    case ED_STABS:                                    /* the stabs filtered down: their level ramps to nothing */
        break;
    case ED_LOG:                                      /* the two-bar log drum run: the log drum and the kick alone */
        trk[ED_T_STABS].p[P_LEVEL] = 0;
        edda_lane_mute = (uint8_t)~1u;
        break;
    case ED_SILENCE:                                  /* a beat of silence */
        edda_mute_all();
        break;
    case ED_DROP:                                     /* everything back, on the one */
        edda_restore_mix();
        edda.phase = ED_IDLE;
        edda_cue_note(ED_CUE_NOTE_DROP);
        edda_cue_cc(ED_CUE_CC_PHASE, ED_IDLE);
        break;
    }
}
/* GLO layer D4 (or the editor): the run starts on the next one; pressed during a run: nothing */
static void edda_run_request(void)
{
    if (!song.playing || edda.phase != ED_IDLE || edda.stopped)
        return;
    edda.armed = 1;
}
/* GLO layer E4: the hard stop now; again: re-entry on the next one. Stopped transport: nothing */
static void edda_stop_toggle(void)
{
    if (!song.playing)
        return;
    if (edda.stopped) {
        edda.resume = 1;
        return;
    }
    if (edda.phase != ED_IDLE) {                      /* a stop ends a run: its mix is the one to restore */
        edda.phase = ED_IDLE;
        edda.armed = 0;
        edda_lane_mute = 0;
    } else
        edda_save_mix();
    edda.stopped = 1;
    edda.resume = 0;
    edda_mute_all();
    edda_lane_mute = (uint8_t)~0u;
    edda_cue_note(ED_CUE_NOTE_STOP);
}
/* the transport stopped (PLAY) or a project loaded: whatever the run or the stop did comes back at once */
static void edda_reset_runtime(void)
{
    if (edda.phase != ED_IDLE || edda.stopped)
        edda_restore_mix();
    edda.phase = ED_IDLE;
    edda.armed = edda.stopped = edda.resume = 0;
    edda.left = 0;
    edda_lane_mute = 0;
    edda.bar = 0;
    edda.mutate = 0;
    edda.fill = 0;
    edda_reveal_bar(1);                               /* (the hole and the map cleared) */
}
static void edda_defaults(void)
{
    uint32_t i;
    edda.camelot = 0;
    edda.cues = 0;                                    /* (opt-in: a lighting rig is a deliberate setup; nothing
                                                       * leaves MIDI OUT a DAW did not ask for) */
    edda.act = 1;
    edda.run_len = 0;
    edda.reveal = 0;
    edda.seed = 0;
    for (i = 0; i < 8u; i++)
        edda_lane_map[i] = (uint8_t)i;
    edda_hole_mute = 0;
    edda_reset_runtime();
    edda.playing = 0;
    edda.beat = 0;
}

/* every block from events_block, before the steps play, with the samples the beat clock is about to move by (n):
 * beats and bars, the run's phases, the ramps. A phase that begins on the one is in force for the one's own step: on
 * the exact grid (fx.c grid_div) the beat clock (beat_pos / beat_n, advanced at the end of each block) has turned over
 * by the start of the block the step on the beat fires in (seq_tick: the first block that starts at or past it), so
 * this block's beat is beat_n itself (no look-ahead: that was for the truncated steps, which came a block early) */
static void edda_block(uint32_t n)
{
    uint32_t beat_now, new_beat, new_bar;
    if (!song.playing) {
        if (edda.playing)
            edda_reset_runtime();
        edda.playing = 0;
        return;
    }
    if (!edda.playing) {                              /* play started: the first block is the first one */
        edda.playing = 1;
        edda.beat = 0;
        edda.bar = 0;
        edda_cue_cc(ED_CUE_CC_ACT, edda.act);
        edda_cue_cc(ED_CUE_CC_BAR, 0);
        edda_cue_cc(ED_CUE_CC_BEAT, 0);
        edda_cue_note(ED_CUE_NOTE_BAR);
        new_beat = 1;
        new_bar = 1;
        edda_reveal_bar(1);
    } else {
        beat_now = beat_n & 3u;
        (void)n;
        new_beat = beat_now != edda.beat;
        new_bar = new_beat && beat_now == 0u;
        if (new_beat) {
            edda.beat = (uint8_t)beat_now;
            if (new_bar) {
                edda.bar++;
                edda_cue_cc(ED_CUE_CC_BAR, edda.bar);
                edda_cue_note(ED_CUE_NOTE_BAR);
            }
            edda_cue_cc(ED_CUE_CC_BEAT, beat_now);
        }
    }
    if (new_bar) {
        if (edda.fill && edda.bar >= edda.fill_until) {
            edda.fill = 0;
            edda_cue_cc(ED_CUE_CC_FILL, 0);
        }
        edda_reveal_bar(edda.bar + 1u);
        if (edda.mutate) {
            edda.mutate = 0;
            edda_mutate_now();
        }
        if (edda.stopped && edda.resume) {            /* the re-entry */
            edda.stopped = edda.resume = 0;
            edda_restore_mix();
            edda_cue_note(ED_CUE_NOTE_DROP);
        }
        if (edda.armed && edda.phase == ED_IDLE && !edda.stopped) {
            edda.armed = 0;
            edda_save_mix();
            edda_phase_enter(ED_SHAKERS);
            new_beat = 0;                             /* this one is the phase's first beat */
        }
    }
    if (edda.phase != ED_IDLE && new_beat) {
        if (edda.left)
            edda.left--;
        if (!edda.left)
            edda_phase_enter(edda.phase + 1u);
    }
    if (edda.phase == ED_STABS) {                     /* the stabs: level down over the phase, per block */
        uint32_t total = edda_phase_beats(ED_STABS), bs = beat_samples();
        uint32_t done = (total - edda.left) * bs + (beat_pos < bs ? beat_pos : bs);   /* samples into the phase */
        uint32_t span = total * bs;
        if (span)
            trk[ED_T_STABS].p[P_LEVEL] = (int16_t)(edda.stab_lvl0 - edda.stab_lvl0 * (done > span ? span : done) / span);
    }
}

/* ------------------------------------------------- the arrangement --- */
/* Euclidean rhythm E(k, n): k onsets spread as evenly as n steps allow (Bjorklund), rotated by rot steps, as a
 * step mask. The West African bell timelines are Euclidean: the 12-pulse ogene / gankogui pattern is E(7, 12)
 * (Toussaint, 2005); E(5, 16) is the bossa-nova bell, E(3, 8) the tresillo */
static uint32_t edda_euclid(uint32_t k, uint32_t n, uint32_t rot)
{
    uint32_t i, m = 0;
    if (!n || n > 16u)
        return 0;
    if (k > n)
        k = n;
    for (i = 0; i < n; i++)
        if ((i * k) % n < k)
            m |= 1u << ((i + rot) % n);
    return m;
}
static uint32_t edda_rnd(void)                        /* xorshift32: the mutations' own, so a seed replays a night */
{
    uint32_t x = edda.seed ? edda.seed : 0x45444441u;   /* "EDDA" */
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    edda.seed = x;
    return x;
}
/* MUTATE (GLO layer, B4): the ear-candy lane re-rolls on the next one: the BELL lane of the drums track gets
 * E(k, LEN) with k from the bell family (3, 5, 7) and a random rotation, the first onset accented; every other
 * lane (the kick, the backbeat, the shakers) stays exactly as it is: constant placement for entrainment, a changing
 * figure for novelty (the EDDA arrangement psychology). Touches hit / acc bits of lane 7 only */
static void edda_mutate_now(void)
{
    track_t *t = &trk[ED_T_DRUMS];
    uint32_t len = t->p[P_SLEN] > 0 && t->p[P_SLEN] <= 16 ? (uint32_t)t->p[P_SLEN] : 16u;
    static const uint8_t K[3] = {3, 5, 7};
    uint32_t r = edda_rnd(), m = edda_euclid(K[r % 3u], len, (r >> 4) % len), i, first = 1;
    for (i = 0; i < len; i++) {
        step_t *st = &t->step[i];
        st->hit &= (uint8_t)~(1u << 7);
        st->acc &= (uint8_t)~(1u << 7);
        if ((m >> i) & 1u) {
            st->hit |= 1u << 7;
            if (first)
                st->acc |= 1u << 7;
            first = 0;
            if (st->time != ST_NOTE) {
                st->time = ST_NOTE;
                if (!st->vel)
                    st->vel = 80;
            }
        } else if (!st->hit && !st->n)
            st->time = ST_REST;
    }
    edda_cue_note(ED_CUE_NOTE_MUTATE);
}
static void edda_mutate_request(void) { if (song.playing) edda.mutate = 1; else edda_mutate_now(); }
/* REVEAL at a bar: bar b (1-based since play). The hole: every 8th bar the kick rests; the reveal: bars 17..24
 * of every 32, SNARE hits play on the RIM lane (the backbeat changes voice, not place) */
static void edda_reveal_bar(uint32_t b)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        edda_lane_map[i] = (uint8_t)i;
    edda_hole_mute = 0;
    if (!edda.reveal)
        return;
    if (b % 8u == 0u)
        edda_hole_mute = 1u;                          /* the kick */
    if (b % 32u >= 17u && b % 32u <= 24u)
        edda_lane_map[1] = 6;                         /* SNARE -> RIM */
}
/* the Camelot path (GLO layer, C5 / D5 / E5): one step down / up the wheel, or across to the relative key:
 * the one-step path of the EDDA set, applied to the tracks */
static void edda_key_step(int32_t how)
{
    uint32_t v = edda.camelot, n[3];
    if (!v)
        v = edda_cam_of((uint32_t)TSEL->p[P_ROOT], (uint32_t)TSEL->p[P_SCALE]);
    if (!v)
        v = 15u;                                      /* 8A, the EDDA home key */
    edda_cam_neighbours(v, n);
    edda_camelot_apply(how < 0 ? n[2] : how > 0 ? n[1] : n[0]);
    edda_cue_cc(ED_CUE_CC_KEY, edda.camelot);
}
/* FILL (GLO layer, A4): the fill-only steps play from now through the end of the next bar (SLOOP's "key 10: the
 * whole next bar"), then rest again; pressed while on: off at once. Stopped: toggles */
static void edda_fill_request(void)
{
    if (edda.fill) {
        edda.fill = 0;
        edda_cue_cc(ED_CUE_CC_FILL, 0);
        return;
    }
    edda.fill = 1;
    edda.fill_until = edda.bar + 2u;
    edda_cue_cc(ED_CUE_CC_FILL, 1);
}

/* ACT+ (GLO layer, G4): the next bulb, round to the first after the fifth */
static void edda_act_next(void)
{
    edda.act = (uint8_t)(edda.act >= ED_ACTS ? 1u : edda.act + 1u);
    edda_cue_cc(ED_CUE_CC_ACT, edda.act);
}

/* MENU > EDDA: ACT set by hand (the bulbs): the rig hears it at once */
static void edda_act_set(uint32_t act)
{
    edda.act = (uint8_t)(act < 1u ? 1u : act > ED_ACTS ? ED_ACTS : act);
    edda_cue_cc(ED_CUE_CC_ACT, edda.act);
}
