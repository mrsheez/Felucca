/* SPDX-License-Identifier: GPL-3.0-only */
/* EDDA OS: "Powered by COSMIC YOU" at power-on, in the place of Felucca's splash, until a key or a button is pressed
 * (main.c's loop runs under it; web/emu felucca_web.c the same): "Press any key to continue" comes under it 1.5 s
 * after it began. Cosmic You's lockup as its pages and its playlist cards set it: "Powered by" (Inter 500, white at 42 %), COSMIC YOU
 * (Cinzel 900, 0.22 em apart, #5AA9FF, glowing 0 0 8px rgba(90,169,255,.45) over 0 0 18px rgba(0,240,255,.18)) and
 * an orange rule under it (transparent, #F97316, transparent), as Chrome draws them at 1.3 device pixels a CSS pixel
 * (tools/gen_powered.py: powered_data.h; tests/powered_check.py holds this drawing to Chrome's). A pixel is
 * composited in 8 bits a channel, the layers in CSS's order, and rounded to RGB565 once. Its colours are its own in
 * every palette, over the palette's BG ("Powered by": TEXT's on a light palette); GREY and MONO stay gray, as every
 * screen of theirs (tests/ui_render.c): the colours as their lumas and no glow (in 32 grays it bands into a smudge). */
#ifndef EDDA_POWERED
#define EDDA_POWERED 1
#endif
#if EDDA_POWERED
#include "powered_data.h"

#define PW_FADE_MS 400u        /* it fades in over this (MENU > ANIM OFF: whole at once) */
#define PW_PROMPT_MS 1500u     /* "Press any key to continue" from this long after it began, faded in the same way .. */
#define PW_BREATH_MS 2400u     /* .. then breathing, 100 % to 50 % of its own and back in this (ANIM OFF: still) */
#define PW_PROMPT_Y 194u       /* its line's top (AF_S), between the lockup and the screen's foot */
#define PW_N (240u * PW_H)     /* the band's pixels: composed once, kept in the canvas's other half (CV_MAX: two) */
#if PW_N * 2u > CV_MAX
#error "powered_data.h: the band and its copy do not fit the canvas"
#endif

static uint32_t pw_nib(const uint8_t *d, uint32_t i) { return (uint32_t)(d[i >> 1] >> ((i & 1u) ? 0 : 4)) & 15u; }

/* a layer's coverages, row after row, from its packets (powered_data.h): w of them into row */
typedef struct { const uint8_t *p; uint8_t z, n, hi; } pw_rd_t;
static void pw_row(pw_rd_t *r, uint8_t *row, uint32_t w)
{
    while (w) {
        if (r->z) {
            r->z--;
            *row = 0;
        } else if (r->n) {
            r->n--;
            if (r->hi) {
                *row = (uint8_t)(*r->p >> 4);
                r->hi = 0;
                if (!r->n)
                    r->p++;                     /* (an odd count's padding) */
            } else {
                *row = (uint8_t)(*r->p++ & 15u);
                r->hi = 1;
            }
        } else {
            r->z = (uint8_t)(*r->p >> 4);       /* the next packet */
            r->n = (uint8_t)(*r->p++ & 15u);
            r->hi = 1;
            continue;
        }
        row++;
        w--;
    }
}

/* a glow's alpha (/4096) at band (x, y): its samples s px apart from (gx, gy), gw a row, bilinear between */
static __attribute__((noinline)) uint32_t pw_glow(const uint8_t *d, int32_t x, int32_t y, int32_t gx, int32_t gy,
                                                  uint32_t gw, uint32_t s, uint32_t pw, uint32_t ph, uint32_t a12)
{
    uint32_t fx = (uint32_t)(x - gx), fy = (uint32_t)(y - gy), i, j, rx, ry, k, acc;
    if (fx >= pw || fy >= ph)
        return 0;
    i = fx / s;
    j = fy / s;
    rx = fx - i * s;
    ry = fy - j * s;
    k = j * gw + i;
    acc = (pw_nib(d, k) * (s - rx) + pw_nib(d, k + 1u) * rx) * (s - ry)
        + (pw_nib(d, k + gw) * (s - rx) + pw_nib(d, k + gw + 1u) * rx) * ry;
    return (acc * a12 + 15u * s * s / 2u) / (15u * s * s);
}

/* c (R G B) toward rgb (0xRRGGBB) by a12 / 4096 */
static __attribute__((noinline)) void pw_over(int32_t *c, uint32_t rgb, uint32_t a12)
{
    uint32_t k;
    for (k = 0; k < 3u; k++)
        c[k] += ((((int32_t)(rgb >> (16u - 8u * k)) & 255) - c[k]) * (int32_t)a12 + 2048) >> 12;
}

static uint32_t pw_rgb(uint32_t c)            /* RGB565 -> 0xRRGGBB, the bits repeated */
{
    uint32_t r = c >> 11, g = (c >> 5) & 63u, b = c & 31u;
    return ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
}

static uint32_t pw_col(uint32_t rgb, int gray)  /* GREY, MONO: its luma, R = G = B */
{
    uint32_t l = ((rgb >> 16) * 54u + ((rgb >> 8) & 255u) * 183u + (rgb & 255u) * 19u + 128u) >> 8;
    return gray ? (l << 16 | l << 8 | l) : rgb;
}

/* the whole lockup into the canvas's band (cv_begin'd) and its copy */
static void pw_compose(void)
{
    static const uint32_t LAYER_RGB[5] = {0xFFFFFFu, 0x00F0FFu, 0x5AA9FFu, 0x5AA9FFu, 0xF97316u};   /* CSS's order */
    uint8_t trow[PW_T_W], prow[PW_P_W];
    pw_rd_t tr = {PW_T, 0, 0, 0}, pr = {PW_P, 0, 0, 0};
    int gray = ux.mono || ux.bw;
    uint32_t x, y, k, col[5], bg = pw_rgb(T_BG);
    for (k = 0; k < 5u; k++)
        col[k] = pw_col(k || !ux.light ? LAYER_RGB[k] : pw_rgb(T_TEXT), gray);
    for (y = 0; y < PW_H; y++) {
        int32_t yy = (int32_t)y;
        int in_t = yy >= PW_T_Y && yy < PW_T_Y + (int32_t)PW_T_H, in_p = yy >= PW_P_Y && yy < PW_P_Y + (int32_t)PW_P_H;
        if (in_t)
            pw_row(&tr, trow, PW_T_W);
        if (in_p)
            pw_row(&pr, prow, PW_P_W);
        for (x = 0; x < 240u; x++) {
            int32_t xx = (int32_t)x, c[3];
            uint32_t a[5] = {0, 0, 0, 0, 0}, any = 0, v;
            if (in_p && xx >= PW_P_X && xx < PW_P_X + (int32_t)PW_P_W)
                a[0] = (prow[xx - PW_P_X] * PW_P_A12 + 7u) / 15u;
            if (!gray) {
                a[1] = pw_glow(PW_G2, xx, yy, PW_G2_X, PW_G2_Y, PW_G2_W, PW_G2_S, PW_G2_PW, PW_G2_PH, PW_G2_A12);
                a[2] = pw_glow(PW_G1, xx, yy, PW_G1_X, PW_G1_Y, PW_G1_W, PW_G1_S, PW_G1_PW, PW_G1_PH, PW_G1_A12);
            }
            if (in_t && xx >= PW_T_X && xx < PW_T_X + (int32_t)PW_T_W && (v = trow[xx - PW_T_X]) != 0)
                a[3] = (v * 4096u + 7u) / 15u;
            if (yy >= PW_R_Y && yy < PW_R_Y + (int32_t)PW_R_H && xx >= PW_R_X && xx < PW_R_X + (int32_t)PW_R_W)
                a[4] = ((uint32_t)PW_R_ROWS[yy - PW_R_Y] * PW_R_COLS[xx - PW_R_X] * 4096u + 32512u) / 65025u;
            for (k = 0; k < 5u; k++)
                any |= a[k];
            if (!any)
                continue;
            c[0] = (int32_t)(bg >> 16);
            c[1] = (int32_t)((bg >> 8) & 255u);
            c[2] = (int32_t)(bg & 255u);
            for (k = 0; k < 5u; k++)
                if (a[k])
                    pw_over(c, col[k], a[k]);
            k = ((uint32_t)c[0] * 31u * 257u + 32768u) >> 16;
            cv_px[y * 240u + x] = swap16(gray ? ux_gray(k)     /* (G = 2 R = 2 B in 565, as ux_gray) */
                                              : k << 11 | (((uint32_t)c[1] * 63u * 257u + 32768u) >> 16) << 5
                                                | (((uint32_t)c[2] * 31u * 257u + 32768u) >> 16));
        }
    }
    memcpy(cv_px + PW_N, cv_px, PW_N * sizeof cv_px[0]);
}

/* the screen at g / 256 of the lockup (0: BG only); whole: from scratch (the lockup composed, then the rows above and
 * below its band), else a frame of the fade, from BG to the kept lockup channel by channel */
static void draw_powered(uint32_t g, int whole)
{
    uint32_t i, k, b = T_BG;
    if (whole) {                                  /* (composed first: the splash stays whole meanwhile) */
        cv_begin(240, PW_H, T_BG);
        pw_compose();
        lcd_fill(0, 0, 240, PW_Y0, T_BG);
        lcd_fill(0, PW_Y0 + PW_H, 240, 240u - PW_Y0 - PW_H, T_BG);
    }
    cv_begin(240, PW_H, T_BG);
    for (i = 0; g && i < PW_N; i++) {
        uint32_t p = swap16(cv_px[PW_N + i]), o = 0;
        if (g >= 256u || p == b) {
            cv_px[i] = cv_px[PW_N + i];
            continue;
        }
        for (k = 0; k < 16u; k += 5u + (k == 5u)) {   /* (B, G, R: bits 0, 5, 11; 5, 6, 5 wide) */
            uint32_t m = k == 5u ? 63u : 31u;
            int32_t q = (int32_t)((b >> k) & m);
            o |= (uint32_t)(q + ((((int32_t)((p >> k) & m) - q) * (int32_t)g + 128) >> 8)) << k;
        }
        cv_px[i] = swap16(ux.mono || ux.bw ? ux_gray(o >> 11) : o);   /* (GREY, MONO: gray kept) */
    }
    GFX_HOOK_PIXELS(PW_N);
    cv_blit(0, PW_Y0);
}

/* the lockup's alpha (/256) t ms after it began: eased in over PW_FADE_MS; MENU > ANIM OFF: whole at once */
static uint32_t powered_alpha(uint32_t t)
{
    uint32_t u;
    if ((ui_prefs & PREF_ANIM_OFF) || t >= PW_FADE_MS)
        return 256u;
    u = t * 256u / PW_FADE_MS;
    return u * u * (768u - 2u * u) >> 16;
}

/* the prompt's strength t ms after the lockup began, in % of white at 42 % ("Powered by"'s): none before PW_PROMPT_MS,
 * then eased in as the lockup was, then breathing down to half and back */
static uint32_t powered_prompt(uint32_t t)
{
    uint32_t u;
    if (t < PW_PROMPT_MS)
        return 0;
    t -= PW_PROMPT_MS;
    if (ui_prefs & PREF_ANIM_OFF)
        return 100u;
    if (t < PW_FADE_MS)
        return powered_alpha(t) * 100u >> 8;
    u = (t - PW_FADE_MS) % PW_BREATH_MS;
    u = u < PW_BREATH_MS / 2u ? u : PW_BREATH_MS - u;
    return 100u - u * 100u / PW_BREATH_MS;
}

/* "Press any key to continue" at pct % (the UI's small face, centred; on a light palette in TEXT's ink) */
static void draw_prompt(uint32_t pct)
{
    uint16_t ink = ux.light ? T_TEXT : 0xFFFFu, c;
    int32_t p = (int32_t)(42u * pct / 100u);
    if (ux.mono || ux.bw)                         /* (gray kept, as ux_gray) */
        c = ux_gray((uint32_t)(((int32_t)(T_BG >> 11) * (100 - p) + (int32_t)(ink >> 11) * p + 50) / 100));
    else
        c = ux_mix(T_BG, ink, p);
    draw_text_box(0, PW_PROMPT_Y, 240, &AF_S, "Press any key to continue", c, 1);
}

/* the screen's state: 0 not up (or over), 1 up, 2 ended by a press (the UI the frame after, so the audio has seen a
 * key under kb_boot_hold first: silent until pressed again); pw_shown: it came up this power-on */
static uint8_t pw_state, pw_shown;
static uint32_t pw_t0, pw_held_b, pw_held_n, pw_a, pw_p;

static void powered_begin(void)                   /* at power-on (main.c): dark until it fades in */
{
    pw_state = pw_shown = 1;
    pw_t0 = fm1_ms;
    pw_held_b = fm1_in.buttons;                   /* (held from before: a press once let go) */
    pw_held_n = fm1_in.notes;
    pw_a = pw_p = 0;
    draw_powered(0, 1);
}

/* a frame of it (main.c's loop, every 15 ms; USB, the editor and the updater run on): 1 when the UI takes over, its
 * screen cleared. The press that ends it does nothing else: the UI's input is swallowed until everything is let go
 * and the knobs are at rest, as after the screen's wake (ui.c scr_input; the turns made under it go too), so no
 * button's release taps (HOME: MIXER, SAVE: its page) or holds (HOME: the menu). OCT- + OCT+ held from power-on
 * end it at UBOOT's countdown (main.c), which the UI shows */
static int powered_frame(void)
{
    uint32_t t = fm1_ms - pw_t0, v;
    if (pw_state == 2) {
        pw_state = 0;
        kb_boot_hold = 0;
        scrn.eat = 1;
        lcd_fill(0, 0, 240, 240, T_BG);
        return 1;
    }
    pw_held_b &= fm1_in.buttons;
    pw_held_n &= fm1_in.notes;
    if ((fm1_in.buttons & ~pw_held_b) || (fm1_in.notes & ~pw_held_n) || ui.uboot) {
        (void)fm1_input_edges(0);
        (void)fm1_input_note_edges();
        pw_state = 2;
        return 0;
    }
    if ((v = powered_alpha(t)) != pw_a)
        draw_powered(pw_a = v, 0);
    if ((v = powered_prompt(t)) != pw_p)
        draw_prompt(pw_p = v);
    return 0;
}
#endif
