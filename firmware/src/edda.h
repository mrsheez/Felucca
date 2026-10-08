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
#define ED_ACTS 5u               /* EDDA: five parts, five bulbs */

typedef struct {
    /* settings (MENU > EDDA; runtime, a project keeps the key through ROOT / SCL) */
    uint8_t camelot;             /* 0 OFF, 1..24: 1A 1B 2A 2B .. 12A 12B (edda_camelot_apply) */
    uint8_t cues;                /* show cues on MIDI OUT: 0 OFF, 1 ON */
    uint8_t act;                 /* bulbs lit, 1..ED_ACTS */
    uint8_t run_len;             /* the run: 0 SHORT (1 + 1 + 2 bars), 1 LONG (2 + 2 + 2 bars) */
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
    /* REVEAL (MENU > EDDA): the entrainment / novelty arrangement: every 8th bar the kick rests (the hole), bars
     * 17..24 of every 32 the backbeat moves from SNARE to RIM (the reveal) */
    uint8_t reveal;
    uint8_t fill;                /* FILL on: the fill-only steps (SF_FILL) play, through the end of the next bar */
    uint32_t fill_until;         /* .. the bar count at which it goes off */
    uint8_t mutate;              /* MUTATE asked for: the bell lane re-rolls on the next one */
    uint32_t seed;               /* the mutations' own random state */
} edda_t;

static edda_t edda;
static uint8_t edda_lane_mute;   /* bit l: lane l silenced by the run or the stop (seq_step reads it) */
static uint8_t edda_hole_mute;   /* bit l: lane l silenced by REVEAL's hole this bar */
static uint8_t edda_lane_map[8] = {0, 1, 2, 3, 4, 5, 6, 7};   /* the lane a lane's hits play on (REVEAL) */

static void edda_block(uint32_t n);
static void edda_defaults(void);
static void edda_reveal_bar(uint32_t b);
static void edda_mutate_now(void);
static void edda_cue_note(uint32_t note);
static void edda_cue_cc(uint32_t cc, uint32_t v);
