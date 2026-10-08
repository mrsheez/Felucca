/* SPDX-License-Identifier: GPL-3.0-only
 * EDDA OS: the Mr. Sheez performance layer on Felucca (see EDDA-OS.md).
 * Declarations seq.c needs before the module (edda.c, included at the end of seq.c). */
#pragma once
#include <stdint.h>

/* The four tracks as the EDDA set template lays them out (the power-on default: bass, pad, lead, drums):
 * the subtraction run and the hard stop act on these roles */
enum { ED_T_LOG = 0, ED_T_STABS = 1, ED_T_LEAD = 2, ED_T_DRUMS = 3 };

/* the subtraction run (edda_drop): its phases, bar by bar */
enum { ED_IDLE, ED_SHAKERS, ED_STABS, ED_LOG, ED_SILENCE, ED_DROP };

/* show cues over USB MIDI OUT, channel 16 (0-based 15): what a visuals rig (TouchDesigner, Resolume, OBS)
 * maps. Notes carry events, CCs carry state */
#define ED_CUE_CH 15u
#define ED_CUE_NOTE_BAR 36u      /* C2: every bar, on the one */
#define ED_CUE_NOTE_DROP 38u     /* D2: the drop (the run's end, or the re-entry after a hard stop) */
#define ED_CUE_NOTE_STOP 40u     /* E2: a hard stop */
#define ED_CUE_NOTE_RUN 41u      /* F2: the subtraction run starts */
#define ED_CUE_CC_BAR 20u        /* bars since play, mod 128 */
#define ED_CUE_CC_BEAT 22u       /* the beat of the bar, 0..3 */
#define ED_CUE_CC_ACT 21u        /* the act: bulbs lit, 1..5 */
#define ED_CUE_CC_PHASE 23u      /* the run's phase (ED_IDLE..) */
#define ED_CUE_CC_KEY 24u        /* the Camelot key, 1..24 (1A 1B .. 12A 12B), 0 none */
#define ED_CUE_NOTE_MUTATE 43u   /* G2: the bell lane re-rolled */
#define ED_CUE_CC_FILL 25u       /* FILL on / off */
#define ED_CUE_CC_SECTION 26u    /* a SONG chain's section playing, 1..4 (A..D); with ED_CUE_NOTE_SECTION as it starts */
#define ED_CUE_CC_SONG 27u       /* THE ARRIVAL's song loaded, 1..13, 0 none (SAVE > ARRIVAL, a project, CLEAR SONG) */
#define ED_CUE_NOTE_SECTION 45u  /* A2: a section starts (a SONG row, a cue's jump) */
#define ED_ACTS 5u               /* EDDA: five parts, five bulbs */

typedef struct {
    /* settings (MENU > EDDA: SHOW CUES, SEQ OUT, RUN, REVEAL kept with the settings, edda_prefs; the key in the
     * project, through ROOT / SCALE; the act from 1 at power-on) */
    uint8_t camelot;             /* 0 OFF, 1..24: 1A 1B 2A 2B .. 12A 12B (edda_camelot_apply) */
    uint8_t cues;                /* show cues on MIDI OUT: 0 OFF, 1 ON */
    uint8_t act;                 /* bulbs lit, 1..ED_ACTS */
    uint8_t run_len;             /* the run: 0 SHORT (1 + 1 + 2 bars), 1 LONG (2 + 2 + 2 bars) */
    uint8_t seq_out;             /* SEQ OUT: the sequencer's notes on USB MIDI OUT: 0 OFF, 1 NOTES, 2 +CLOCK (seq.c
                                  * seq_mo_*: the clock, START and STOP too, on CLK INT) */
    /* runtime */
    uint8_t phase;               /* ED_IDLE.. */
    uint8_t left;                /* beats left in the phase */
    uint8_t armed;               /* a run asked for: it starts on the next one */
    uint8_t stopped;             /* hard stop on (everything muted, the clock runs) */
    uint8_t resume;              /* hard stop: re-enter on the next one */
    uint8_t playing;             /* song.playing as last seen */
    uint8_t beat;                /* beat_n as last seen */
    uint32_t bar;                /* bars since play */
    int16_t lvl[4];              /* the levels before the run / the stop (restored at the drop) */
    uint8_t mute[4];             /* .. and the mutes */
    uint32_t stab_lvl0;          /* the stabs' level the ramp starts from (Q0) */
    uint16_t stab_q8;            /* .. how much of it is left, 256 (all) .. 0 (SEQ OUT's velocities follow it) */
    /* REVEAL (MENU > EDDA): the entrainment / novelty arrangement: every 8th bar the kick rests (the hole), bars
     * 17..24 of every 32 the backbeat moves from SNARE to RIM (the reveal) */
    uint8_t reveal;
    uint8_t fill;                /* FILL on: the fill-only steps (SF_FILL) play, through the end of the next bar */
    uint32_t fill_until;         /* .. the bar count at which it goes off */
    uint8_t mutate;              /* MUTATE asked for: the bell lane re-rolls on the next one */
    uint32_t seed;               /* the mutations' own random state */
    uint8_t song;                /* THE ARRIVAL's song the music is (arrival.c arv_cur), for its cue (ED_RQ_CUE_SONG) */
} edda_t;

static edda_t edda;
static uint8_t edda_lane_mute;   /* bit l: lane l silenced by the run or the stop (seq_step reads it) */
static uint8_t edda_hole_mute;   /* bit l: lane l silenced by REVEAL's hole this bar */
static uint8_t edda_lane_map[8] = {0, 1, 2, 3, 4, 5, 6, 7};   /* the lane a lane's hits play on (REVEAL) */

static void edda_block(uint32_t n);
static void edda_defaults(void);
/* the settings kept with MENU's (ui.c edda_prefs, a byte of the settings): bit 0 SHOW CUES, bits 1..2 SEQ OUT (3: no
 * value, read as OFF), bit 3 RUN LONG, bit 4 REVEAL; 0: every one's default. The key lives on in the project (ROOT /
 * SCALE), the act and the run start afresh */
#define ED_PREFS 0x1Fu
static void edda_prefs_apply(uint32_t b);
static uint32_t edda_prefs_bits(void);
/* SEQ OUT (seq.c seq_mo_on): a step's velocity on track k as the run leaves it (0: nothing goes out) */
static uint32_t edda_out_vel(uint32_t k, uint32_t vel);
/* the UI's EDDA actions (ui_layer.c the GLO keys, menu_items.c MENU > EDDA): queued, run by edda_block at the start of
 * the next audio block, where the sequencer and MIDI OUT live (one context changes the run and sends the cues) */
enum { ED_RQ_RUN = 1, ED_RQ_STOP, ED_RQ_ACT, ED_RQ_FILL, ED_RQ_MUTATE, ED_RQ_KEY_DN, ED_RQ_KEY_UP, ED_RQ_KEY_REL,
       ED_RQ_CUE_KEY, ED_RQ_CUE_ACT, ED_RQ_CUE_SONG };
static void edda_ui(uint32_t rq);
static void edda_reveal_bar(uint32_t b);
static void edda_mutate_now(void);
static void edda_cue_note(uint32_t note);
static void edda_cue_cc(uint32_t cc, uint32_t v);
