/* SPDX-License-Identifier: GPL-3.0-only
 * EDDA OS: VIZ, twelve full-screen visualisers over HOME (SLOOP's: a tap of HOME on the tracks screen). Included by
 * ui_draw.c (ui_draw_page draws it, ui_input.c's HOME tap opens and steps it: vz_home_tap).
 *
 * HOME tapped on HOME opens the visualiser shown last (SCOPE at power-on); tapped again: the next one, its number and
 * name at the foot for a moment; past the twelfth: HOME again (SCOPE the next time). A page button leaves for its page;
 * HOME held opens the MENU (closed: the visualiser again). Everything else stays as on HOME: the keys play, KNOB 1..4
 * are HOME's four (the name and value come up at the foot), SELECT the tempo, PRESET the sound, ALGORITHM the track,
 * PLAY, REC, the layers (their map over it while held). The screen stays on while it shows (MENU > SCREEN OFF waits).
 *   SCOPE      the output, triggered, on a phosphor screen: a white-hot curve in its glow, the area to the zero line
 *              lit, the last picture an afterimage, a graticule
 *   SPECTRUM   80 bands, 43 Hz .. 10.5 kHz (a 512-point FFT of the scope's 23 ms, Hann), bars in heat colours (the
 *              loud reach the hot white), their peaks held, a reflection under the floor
 *   WATERFALL  the spectrum of the last 60 pictures, the newest at the top, in heat colours blended between the bands
 *              and down the rows
 *   ORBIT      the output against itself a quarter period later: a phase portrait (a pure tone is a circle) on an
 *              instrument's face, the newest part of the curve the brightest, its head a dot
 *   TUNNEL     a square a beat flies out of the centre, twisting as it comes; the one born on the bar's first beat lit
 *   PULSE      a ring a beat out of the centre over slowly turning spokes, the level of the music its core (a sun)
 *   STARS      a starfield: the tempo its speed, a rush after every beat, the near ones streak
 *   GRID       the four tracks' 16 steps round their playheads (a trail behind each); what sounds lit; the selected
 *              track underlined
 *   RAIN       the notes sounding, falling and fading: C2 .. B6 across (the octaves marked), the drum tracks' apart
 *   WHEEL      the Camelot wheel: the key (MENU > EDDA > KEY, else the selected track's ROOT / SCALE), the keys it
 *              mixes into, the roots of the notes sounding
 *   BULBS      EDDA's five bulbs (the act), their halos breathing with the beat; the run's phase and its beats, the bar
 *   CLOCK      the tempo in a lamp's figures (pointed segments), bar . beat, the beats of the bar
 * Drawing: the screen in two bands of the canvas (gfx.c: 240 x 120 each). A frame draws one band (a forced one both),
 * the two of a picture from one snapshot of the music (vz_snap): never two moments in one picture. The SPI to the
 * panel (12 MHz) bounds it, about 12 pictures a second, the UI's work in between. Every edge anti-aliased: lines
 * Wu's way, rings and dots by their coverage, in sixteenths of a pixel where a shape moves. Colours: the palette's
 * tokens and blends of them only (GREY and MONO stay gray). Texts lie inside one band. The state lives in the pool,
 * off the audio's RAM. */
#define VZ_COUNT 12u
enum { VZ_SCOPE, VZ_SPECTRUM, VZ_WATERFALL, VZ_ORBIT, VZ_TUNNEL, VZ_PULSE, VZ_STARS, VZ_GRID, VZ_RAIN, VZ_WHEEL,
       VZ_BULBS, VZ_CLOCK };
static const char *const VZ_NAME[VZ_COUNT] = {"SCOPE", "SPECTRUM", "WATERFALL", "ORBIT", "TUNNEL", "PULSE", "STARS",
                                             "GRID", "RAIN", "WHEEL", "BULBS", "CLOCK"};
#define VZ_SPLIT 120                     /* the bands: screen rows 0..119 and 120..239 */
#define VZ_NB 80u                        /* SPECTRUM: bands (3 px each) */
#define VZ_WB 40u                        /* WATERFALL: bands (two of SPECTRUM's each, 6 px) */
#define VZ_WF 60u                        /* WATERFALL: rows (4 px) */
#define VZ_NSTAR 64u
#define VZ_RROWS 44u                     /* RAIN: rows (5 px, from y 16: the octave marks above) */
#define VZ_RLO 36                        /* RAIN: C2 (36) .. B6 (95), 4 px a note */
#define VZ_NAME_MS 1600u
#define VZ_AXIS (120 * 16)               /* SCOPE: the zero line (Q4) */
/* the bands' FFT bins (43.07 Hz each at the scope's 22.05 kHz): one bin each up to 860 Hz, then log-spaced to 10.5 kHz */
static const uint8_t VZ_EDGE[VZ_NB + 1u] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    32, 33, 34, 36, 37, 39, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 61, 63, 66, 69, 72, 75, 78, 81, 84, 88, 91, 95,
    99, 103, 108, 112, 117, 122, 127, 132, 138, 143, 149, 156, 162, 169, 176, 183, 191, 199, 207, 216, 225, 234, 244};
static struct {
    uint8_t n, last;                     /* the one shown; the one HOME opens */
    uint8_t band;                        /* the band the next frame draws */
    uint8_t fresh;                       /* histories cleared on the next snapshot */
    uint32_t name_ms;                    /* fm1_ms | 1 until which its name shows, 0 none */
    uint32_t t_ms;                       /* fm1_ms of the last snapshot */
    int32_t peak;                        /* the snapshot's peak (auto-scale) */
    uint32_t level;                      /* its level, 0..255 (60 dB) */
    uint32_t trig;                       /* SCOPE: the rising zero crossing the trace starts at */
    uint32_t tau;                        /* ORBIT: a quarter of the period, in scope samples */
    uint32_t beats, beat_seen;           /* beats counted (the beat clock's turns) */
    uint32_t phase;                      /* the beat's phase, 0..255 */
    uint32_t bn;                         /* the beat of the bar (beat_n), 0 the first */
    uint8_t spec[VZ_NB], cap[VZ_NB];     /* SPECTRUM: 0..255, smoothed; the held peaks */
    uint8_t wf[VZ_WF][VZ_WB / 2u];       /* WATERFALL: 4 bits a band; row wf_top the newest */
    uint8_t wf_top;
    uint8_t after[240];                  /* SCOPE: the last picture's curve, rows */
    int16_t trace[240];                  /* .. this one's, Q4 */
    uint8_t after_ok;
    struct { int16_t x, y, z; } star[VZ_NSTAR];
    uint32_t rnd;
    uint8_t rain[VZ_RROWS][16];           /* RAIN: 2 bits a note from VZ_RLO (1 a synth track, 2 a drum track) */
    uint8_t rain_top;
    uint16_t ramp[16];                   /* heat in 16 steps (colours) */
    uint16_t heat[64], lum[64], hot[17]; /* (vz_ramps: swapped, a pixel a store) */
    uint16_t glow[4];                    /* a trace's glow: TINT-ish .. the core */
    uint16_t trail[8];                   /* RAIN: THEME fading to the background by age */
    uint16_t dtrail[8];                  /* .. the drum tracks' ACCENT */
} vz __attribute__((section(".pool")));
static int32_t vz_y0, vz_y1;             /* the band being drawn: screen rows vz_y0 .. vz_y1 - 1 */

static uint32_t vz_rand(void)
{
    uint32_t x = vz.rnd ? vz.rnd : 0x56495A31u;   /* "VIZ1" */
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    vz.rnd = x;
    return x;
}
static uint32_t vz_lg(uint32_t v)                /* 16 log2(v) (4 fraction bits, linear between powers of 2); 0: 0 */
{
    uint32_t e = 0;
    if (!v)
        return 0;
    while (e < 31u && v >> (e + 1u))
        e++;
    return e * 16u + (e >= 4u ? v >> (e - 4u) : v << (4u - e)) % 16u;
}
static int vz_in(int32_t y, int32_t h) { return y < vz_y1 && y + h > vz_y0; }   /* rows y .. y + h - 1 meet the band */
/* a text centred across x0 .. x0 + w, its line box at y; drawn in the band it lies in (never placed across two) */
static void vz_text(int32_t x0, int32_t w, int32_t y, const aafont_t *f, const char *s, uint16_t fg, uint16_t bg)
{
    if (vz_in(y, f->h)) {
        GFX_HOOK_ALIGN(x0, 0, x0 + w, 0, AL_H, "viz text centred");
        cv_text_in(x0, y, w, f, s, fg, bg);
    }
}
/* ---- anti-aliased drawing (the panel is 240 px across: every edge counts). Where a shape moves, its coordinates
 * are in sixteenths of a pixel (Q4), a pixel's centre at its whole value. A pixel of the band's canvas blended with c
 * by cov (0..16): over the background through the ramp (gfx.c ramp: the text's own blending), over ink mixed */
static uint16_t vz_n(uint16_t c) { return ux.bw ? ux_gray(c >> 11) : c; }   /* (MONO: blends of blends stay neutral) */
static void vz_px(int32_t x, int32_t y, uint16_t c, const uint16_t *rv, uint32_t cov)
{
    uint16_t *p;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || (uint32_t)y >= cv_h || !cov)
        return;
    p = cv_px + (uint32_t)y * cv_w + (uint32_t)x;
    if (cov >= 16u)
        *p = swap16(c);
    else if (*p == rv[0])
        *p = rv[cov];
    else
        *p = swap16(vz_n(ux_mix(swap16(*p), c, (int32_t)(cov * 100u / 16u))));
}
/* a line between Q4 points, Wu's way: along its long axis each whole pixel from the first end up to (not at) the
 * other (a curve's segments meet without a pixel drawn twice), the two pixels either side of the ideal line sharing
 * its coverage; wide: twice, half a pixel either side (2 px across) */
static void vz_aalq(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, int wide)
{
    const uint16_t *rv;
    int32_t dx = x1 - x0, dy = y1 - y0, steep = (dy < 0 ? -dy : dy) > (dx < 0 ? -dx : dx), t, i, ie, f, step, w;
    t = (y0 < y1 ? y0 : y1) >> 4;
    c = vz_n(c);
    if (t - 2 >= vz_y1 || ((y0 > y1 ? y0 : y1) >> 4) + 2 < vz_y0 || c == cv_bg)
        return;                                          /* (not in the band, or nothing to see) */
    rv = ramp(c, cv_bg);
    if (steep) {
        t = x0; x0 = y0; y0 = t;
        t = x1; x1 = y1; y1 = t;
    }
    if (x0 > x1) {
        t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
    }
    dx = x1 - x0;
    dy = y1 - y0;
    for (w = wide ? -8 : 0; w <= (wide ? 8 : 0); w += 16) {
        i = (x0 + 15) >> 4;                              /* the first whole pixel at or after x0 .. */
        ie = (x1 + 15) >> 4;                             /* .. to the last before x1 */
        step = dx ? dy * 256 / dx : 0;                   /* (Q8 a pixel) */
        f = (y0 + w) * 16 + (dx ? (i * 16 - x0) * dy * 16 / dx : 0);
        for (; i < ie; i++, f += step) {
            int32_t yi = f >> 8;
            uint32_t fr = (uint32_t)(f & 255) >> 4;
            if (steep) {
                vz_px(yi, i, c, rv, 16u - fr);
                vz_px(yi + 1, i, c, rv, fr);
            } else {
                vz_px(i, yi, c, rv, 16u - fr);
                vz_px(i, yi + 1, c, rv, fr);
            }
        }
    }
}
/* the coverage (0..16) of a pixel d2 (its distance squared) from the centre by the disc of radius r near its edge: the
 * edge locally straight, the signed distance (r^2 - d^2) / 2r (inv: 8 << 16 / r), half a pixel either side */
static int32_t vz_dcov(int32_t d2, int32_t r, int32_t inv)
{
    int32_t v;
    if (r <= 0)
        return 0;
    v = (((r * r - d2) * inv) >> 16) + 8;
    return v < 0 ? 0 : v > 16 ? 16 : v;
}
static int32_t vz_ext(int32_t r, int32_t ady)        /* a circle's half-width on the row ady from its centre, -1 past it */
{
    return r < 0 || ady > r ? -1 : (int32_t)isqrt32((uint32_t)(r * r - ady * ady));
}
/* a ring (r0 < d <= r1 from cx, cy; r0 0: a disc), anti-aliased: on each row the pixels a pixel or more inside it
 * filled whole, the ones within a pixel of an edge by their coverage. Rows in the band only */
static void vz_aaring(int32_t cx, int32_t cy, int32_t r0, int32_t r1, uint16_t c)
{
    const uint16_t *rv;
    int32_t y, ya = cy - r1 - 1, yb = cy + r1 + 1, i0, i1;
    c = vz_n(c);
    if (r1 <= 0 || c == cv_bg)
        return;
    r0 = r0 < 0 ? 0 : r0;
    ya = ya < vz_y0 ? vz_y0 : ya;
    yb = yb >= vz_y1 ? vz_y1 - 1 : yb;
    if (ya > yb)
        return;
    rv = ramp(c, cv_bg);
    i1 = (8 << 16) / r1;
    i0 = r0 ? (8 << 16) / r0 : 0;
    for (y = ya; y <= yb; y++) {
        int32_t dy = y - cy, ady = dy < 0 ? -dy : dy, x, k, e[4];
        int32_t oo = vz_ext(r1 + 1, ady), oi = vz_ext(r1 - 1, ady);           /* the outer edge: (oi, oo] */
        int32_t io = r0 ? vz_ext(r0 + 1, ady) : -1, ii = r0 ? vz_ext(r0 - 1, ady) : -1;   /* the inner: (ii, io] */
        if (oo < 0)
            continue;
        if (io >= oi) {                                  /* (a thin ring: the two edges one run) */
            e[0] = ii; e[1] = oo; e[2] = e[3] = 0;
        } else {
            int32_t a = io + 1, b = oi;                  /* whole: a .. b either side */
            if (a <= b) {
                cv_rect(cx + a, y, b - a + 1, 1, c);
                cv_rect(cx - b, y, a ? b - a + 1 : b, 1, c);
            }
            e[0] = ii; e[1] = io; e[2] = oi; e[3] = oo;
        }
        for (k = 0; k < 4; k += 2)
            for (x = e[k] + 1; x <= e[k + 1]; x++) {
                int32_t d2 = x * x + dy * dy, cov = vz_dcov(d2, r1, i1) - vz_dcov(d2, r0, i0);
                if (cov <= 0)
                    continue;
                vz_px(cx + x, y, c, rv, (uint32_t)cov);
                if (x)
                    vz_px(cx - x, y, c, rv, (uint32_t)cov);
            }
    }
}
/* a small disc, its centre and radius in Q4 (stars, the orbit's head, dots): every pixel of its box by 4 x 4 samples */
static void vz_dot(int32_t xq, int32_t yq, int32_t rq, uint16_t c)
{
    const uint16_t *rv;
    int32_t x, y, xa = (xq - rq) >> 4, xb = (xq + rq + 15) >> 4, ya = (yq - rq) >> 4, yb = (yq + rq + 15) >> 4, r2 = rq * rq;
    ya = ya < vz_y0 ? vz_y0 : ya;
    yb = yb >= vz_y1 ? vz_y1 - 1 : yb;
    c = vz_n(c);
    if (rq <= 0 || ya > yb || c == cv_bg)
        return;
    rv = ramp(c, cv_bg);
    for (y = ya; y <= yb; y++)
        for (x = xa; x <= xb; x++) {
            int32_t sx, sy, n = 0;
            for (sy = 0; sy < 4; sy++) {
                int32_t py = y * 16 - 6 + sy * 4 - yq;  /* (the samples at -6 -2 2 6 sixteenths) */
                for (sx = 0; sx < 4; sx++) {
                    int32_t px = x * 16 - 6 + sx * 4 - xq;
                    n += px * px + py * py <= r2;
                }
            }
            vz_px(x, y, c, rv, (uint32_t)n);
        }
}
/* a lit sphere: a disc of radius r whose colour runs from rv[0] at its rim to rv[16] at its heart (rv: 17 swapped
 * colours), the light falling off with the square of the distance; the rim anti-aliased against what lies under it */
static void vz_orb(int32_t cx, int32_t cy, int32_t r, const uint16_t *rv)
{
    int32_t y, ya = cy - r - 1, yb = cy + r + 1, r2 = r * r, inv, i1;
    uint16_t rim;
    if (r <= 0)
        return;
    ya = ya < vz_y0 ? vz_y0 : ya;
    yb = yb >= vz_y1 ? vz_y1 - 1 : yb;
    inv = (16 << 16) / r2;
    i1 = (8 << 16) / r;
    rim = swap16(rv[0]);
    for (y = ya; y <= yb; y++) {
        int32_t dy = y - cy, ext = vz_ext(r + 1, dy < 0 ? -dy : dy), x;
        uint16_t *row = cv_px + (uint32_t)(y - vz_y0) * cv_w;
        for (x = -ext; x <= ext; x++) {
            int32_t px = cx + x, d2 = x * x + dy * dy, cov;
            if ((uint32_t)px >= cv_w)
                continue;
            cov = vz_dcov(d2, r, i1);
            if (cov >= 16) {
                int32_t k = 16 - (d2 * inv >> 16);
                row[px] = rv[k < 0 ? 0 : k];
            } else if (cov > 0)
                vz_px(px, y, rim, ramp(rim, cv_bg), (uint32_t)cov);
        }
    }
}
/* a square frame centred on the screen, half-size hq (Q4), t px thick, turned by ang (1024 a turn): t outlines */
static void vz_square(int32_t hq, int32_t t, uint32_t ang, uint16_t c)
{
    int32_t cs = SINE[(ang + 256u) & 1023u], sn = SINE[ang & 1023u], k;
    if (hq < 16)
        return;
    t = t < 1 ? 1 : t > hq / 16 ? hq / 16 : t;
    for (k = 0; k < t; k++) {
        int32_t r = hq - k * 16, ax = (r * cs - r * sn) >> 15, ay = (r * sn + r * cs) >> 15;   /* the corner (r, r) turned */
        int32_t bx = (-r * cs - r * sn) >> 15, by = (-r * sn + r * cs) >> 15;            /* .. (-r, r) */
        vz_aalq(1920 + ax, 1920 + ay, 1920 + bx, 1920 + by, c, 0);
        vz_aalq(1920 + bx, 1920 + by, 1920 - ax, 1920 - ay, c, 0);
        vz_aalq(1920 - ax, 1920 - ay, 1920 - bx, 1920 - by, c, 0);
        vz_aalq(1920 - bx, 1920 - by, 1920 + ax, 1920 + ay, c, 0);
    }
}
static uint16_t vz_c(uint16_t v) { return swap16(v); }   /* a colour of the swapped tables (lum, heat, hot) */
/* the ramps from the palette (ux_mix is not for inner loops): once a picture. heat: BG .. SEL .. THEME .. the hot
 * white of THEME .. TEXT (SPECTRUM, WATERFALL, the ramps of the rest); lum: THEME's light over the background, in
 * 64ths; hot: THEME to its hot white, in 16ths (the swapped panel values: a pixel is a store) */
static void vz_ramps(void)
{
    uint32_t i;
    uint16_t hot = ux_mix(T_THEME, T_TEXT, 72);
    for (i = 0; i < 64u; i++) {
        vz.lum[i] = swap16(ux_mix(T_BG, T_THEME, (int32_t)(i * 100u / 63u)));
        vz.heat[i] = swap16(i < 20u ? ux_mix(T_BG, T_SEL, (int32_t)(i * 5u)) :
                            i < 44u ? ux_mix(T_SEL, T_THEME, (int32_t)((i - 20u) * 100u / 24u)) :
                            i < 58u ? ux_mix(T_THEME, hot, (int32_t)((i - 44u) * 100u / 14u)) :
                                      ux_mix(hot, T_TEXT, (int32_t)((i - 58u) * 20u)));
    }
    for (i = 0; i <= 16u; i++)
        vz.hot[i] = swap16(ux_mix(T_THEME, hot, (int32_t)(i * 100u / 16u)));
    for (i = 0; i < 16u; i++)
        vz.ramp[i] = swap16(vz.heat[i * 63u / 15u]);
    vz.glow[0] = ux_mix(T_BG, T_THEME, 18);
    vz.glow[1] = ux_mix(T_BG, T_THEME, 42);
    vz.glow[2] = T_THEME;
    vz.glow[3] = hot;
    for (i = 0; i < 8u; i++) {
        vz.trail[i] = ux_mix(T_THEME, T_BG, (int32_t)(i * 11u));
        vz.dtrail[i] = ux_mix(T_ACCENT, T_BG, (int32_t)(i * 11u));
    }
    if (ux.bw) {                                         /* (MONO: every one neutral, its blends too) */
        for (i = 0; i < 64u; i++) {
            vz.lum[i] = swap16(vz_n(swap16(vz.lum[i])));
            vz.heat[i] = swap16(vz_n(swap16(vz.heat[i])));
        }
        for (i = 0; i <= 16u; i++)
            vz.hot[i] = swap16(vz_n(swap16(vz.hot[i])));
        for (i = 0; i < 16u; i++)
            vz.ramp[i] = vz_n(vz.ramp[i]);
        for (i = 0; i < 8u; i++) {
            vz.trail[i] = vz_n(vz.trail[i]);
            vz.dtrail[i] = vz_n(vz.dtrail[i]);
        }
        for (i = 0; i < 4u; i++)
            vz.glow[i] = vz_n(vz.glow[i]);
    }
}

/* ---------------------------------------------------------- the FFT --- */
/* 256 complex points in place, interleaved (re, im): the DFT / 256 (each stage halves: no overflow from a
 * half-scale input). Twiddles from SINE (1024 a turn) */
static void vz_fft(int16_t *z)
{
    uint32_t i, j = 0, k, len;
    for (i = 1; i < 256u; i++) {                /* bit reversal */
        uint32_t bit = 128u;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            int16_t a = z[2u * i], b = z[2u * i + 1u];
            z[2u * i] = z[2u * j]; z[2u * i + 1u] = z[2u * j + 1u];
            z[2u * j] = a; z[2u * j + 1u] = b;
        }
    }
    for (len = 2; len <= 256u; len <<= 1) {
        uint32_t half = len >> 1, step = 1024u / len;
        for (i = 0; i < 256u; i += len)
            for (k = 0; k < half; k++) {
                int32_t wr = SINE[(k * step + 256u) & 1023u], wi = -SINE[(k * step) & 1023u];   /* e^-i2pik/len */
                int16_t *a = z + 2u * (i + k), *b = z + 2u * (i + k + half);
                int32_t tr = (b[0] * wr - b[1] * wi) >> 15, ti = (b[0] * wi + b[1] * wr) >> 15, ar = a[0], ai = a[1];
                a[0] = (int16_t)((ar + tr) >> 1); a[1] = (int16_t)((ai + ti) >> 1);
                b[0] = (int16_t)((ar - tr) >> 1); b[1] = (int16_t)((ai - ti) >> 1);
            }
    }
}
/* the spectrum of scope_snap (destroyed: its 512 samples are the FFT's 256 complex points) into 0..255 per band
 * (60 dB): Hann (half scale), the complex FFT, then each real bin from Z[k] and Z[256 - k] */
static void vz_spectrum(uint8_t *out)
{
    int16_t *z = scope_snap;
    uint32_t i, b;
    for (i = 0; i < SCOPE_N; i++) {
        int32_t w = (32767 - SINE[(2u * i + 256u) & 1023u]) >> 1;    /* Hann, Q15 */
        z[i] = (int16_t)((z[i] * w) >> 16);                           /* (x w / 2) */
    }
    vz_fft(z);
    for (b = 0; b < VZ_NB; b++) {
        uint32_t m = 0, k;
        for (k = VZ_EDGE[b]; k < VZ_EDGE[b + 1u]; k++) {
            uint32_t q = 256u - k;
            int32_t fr = z[2u * k], fi = z[2u * k + 1u], gr = z[2u * q], gi = -z[2u * q + 1u];   /* F, conj Z[256 - k] */
            int32_t er = (fr + gr) >> 1, ei = (fi + gi) >> 1, or_ = (fr - gr) >> 1, oi = (fi - gi) >> 1;
            int32_t s = SINE[(2u * k) & 1023u], c = SINE[(2u * k + 256u) & 1023u];   /* phi = 2 pi k / 512 */
            int32_t xr = er + ((-or_ * s + oi * c) >> 15), xi = ei + ((-or_ * c - oi * s) >> 15);   /* E + O (-sin - i cos) */
            uint32_t p = (uint32_t)(xr * xr + xi * xi);
            m = p > m ? p : m;
        }
        k = vz_lg(m);                                                 /* 16 log2 |X|^2: 3 dB a 16 */
        out[b] = (uint8_t)(k <= 96u ? 0u : k >= 416u ? 255u : (k - 96u) * 255u / 320u);
    }
}

/* ------------------------------------------------------- the picture --- */
/* what a picture shows, once for its two bands: the scope's samples, the level, the beat, the analyses */
static void vz_snap(void)
{
    uint32_t w = scope_w, i, dt, now = fm1_ms, bl = beat_len ? beat_len : beat_samples(), bn = beat_n;
    int32_t peak = 1;
    if (vz.fresh) {
        memset(vz.spec, 0, sizeof vz.spec);
        memset(vz.cap, 0, sizeof vz.cap);
        memset(vz.wf, 0, sizeof vz.wf);
        memset(vz.rain, 0, sizeof vz.rain);
        memset(vz.star, 0, sizeof vz.star);
        vz.after_ok = 0;
        vz.beat_seen = bn;
        vz.t_ms = now;
        vz.fresh = 0;
    }
    dt = now - vz.t_ms;
    dt = dt > 250u ? 250u : dt;
    vz.t_ms = now;
    for (i = 0; i < SCOPE_N; i++) {
        int32_t s = scope_buf[(w + i) & (SCOPE_N - 1u)];
        scope_snap[i] = (int16_t)s;
        s = s < 0 ? -s : s;
        peak = s > peak ? s : peak;
    }
    vz.peak = peak;
    i = vz_lg((uint32_t)peak);                                        /* 16 log2: 6 dB a 16; 32767 -> 240 */
    vz.level = i <= 80u ? 0u : (i - 80u) * 255u / 160u;              /* (-60 dB .. 0) */
    vz.beats += (bn - vz.beat_seen) & 3u;                            /* (a picture is shorter than a beat) */
    vz.beat_seen = bn;
    vz.bn = bn & 3u;
    vz.phase = bl ? (beat_pos < bl ? beat_pos : bl - 1u) * 256u / bl : 0u;
    for (vz.trig = 0, i = 1; i < SCOPE_N - 240u; i++)                /* SCOPE: a rising zero crossing */
        if (scope_snap[i - 1u] < 0 && scope_snap[i] >= 0) {
            vz.trig = i;
            break;
        }
    if (vz.n == VZ_SCOPE) {                                           /* SCOPE: the curve (Q4 rows, +-104 px) */
        int32_t pk = peak > 1500 ? peak : 1500;
        for (i = 0; i < 240u; i++) {
            int32_t y = VZ_AXIS - scope_snap[vz.trig + i] * 1664 / pk;
            vz.trace[i] = (int16_t)(y < 96 ? 96 : y > 3728 ? 3728 : y);
        }
    }
    {   /* ORBIT: the period from the rising crossings; a quarter of it the lag */
        uint32_t first = 0, last = 0, n = 0;
        for (i = 1; i < SCOPE_N; i++)
            if (scope_snap[i - 1u] < 0 && scope_snap[i] >= 0) {
                if (!n++)
                    first = i;
                last = i;
            }
        i = n > 1u ? (last - first) / (n - 1u) / 4u : 12u;
        vz.tau = i < 2u ? 2u : i > 120u ? 120u : i;
    }
    if (vz.n == VZ_SPECTRUM || vz.n == VZ_WATERFALL) {
        uint8_t now_b[VZ_NB];
        vz_spectrum(now_b);                                           /* (scope_snap is the FFT's now) */
        for (i = 0; i < VZ_NB; i++) {
            uint32_t d = dt * 3u / 8u + 1u;                             /* (~0.7 s from full to nothing) */
            vz.spec[i] = (uint8_t)(now_b[i] > vz.spec[i] ? now_b[i] : vz.spec[i] > d ? vz.spec[i] - d : 0u);
            d = dt / 10u + 1u;
            vz.cap[i] = (uint8_t)(vz.spec[i] > vz.cap[i] ? vz.spec[i] : vz.cap[i] > d ? vz.cap[i] - d : 0u);
        }
        vz.wf_top = (uint8_t)((vz.wf_top + VZ_WF - 1u) % VZ_WF);
        for (i = 0; i < VZ_WB; i++) {                                 /* (a WATERFALL band: the louder of its two) */
            uint32_t v = now_b[2u * i] > now_b[2u * i + 1u] ? now_b[2u * i] : now_b[2u * i + 1u];
            if (i & 1u)
                vz.wf[vz.wf_top][i / 2u] |= (uint8_t)((v >> 4) << 4);
            else
                vz.wf[vz.wf_top][i / 2u] = (uint8_t)(v >> 4);
        }
    }
    if (vz.n == VZ_RAIN) {                                            /* RAIN: this picture's row */
        uint8_t *row;
        uint32_t k, v;
        vz.rain_top = (uint8_t)((vz.rain_top + VZ_RROWS - 1u) % VZ_RROWS);
        row = vz.rain[vz.rain_top];
        memset(row, 0, 16);
        for (k = 0; k < NTRK; k++)
            for (v = 0; v < NVOICE; v++) {
                const voice_t *vo = &trk[k].v[v];
                int32_t x = (int32_t)vo->note - VZ_RLO;
                if (vo->active && vo->gate && x >= 0 && x < 60)
                    row[x >> 2] |= (uint8_t)((drum_track(&trk[k]) ? 2u : 1u) << ((x & 3) * 2));
            }
    }
    if (vz.n == VZ_STARS) {                                           /* STARS: on by the tempo, a rush after the beat */
        uint32_t rush = 255u - vz.phase, sp = (uint32_t)song.g[G_BPM] * (64u + rush * rush / 256u) / 120u;
        for (i = 0; i < VZ_NSTAR; i++) {
            int32_t z = vz.star[i].z - (int32_t)(dt * sp / 128u);   /* (at 120 BPM: 2 s deep to near, 5 x after a beat) */
            if (z < 24 || !vz.star[i].z) {
                vz.star[i].x = (int16_t)((int32_t)(vz_rand() % 1024u) - 512);
                vz.star[i].y = (int16_t)((int32_t)(vz_rand() % 1024u) - 512);
                z = 1024 - (int32_t)(vz_rand() % (vz.star[i].z ? 64u : 1000u));
            }
            vz.star[i].z = (int16_t)z;
        }
    }
    vz_ramps();
}

/* SCOPE's curve through column s: the rows (Q4) from half-way to the column before to half-way to the one after */
static void vz_span(int32_t s, int32_t *lo, int32_t *hi)
{
    const int16_t *q = vz.trace;
    int32_t m = q[s], a = s > 0 ? (q[s - 1] + m) >> 1 : m, b = s < 239 ? (q[s + 1] + m) >> 1 : m;
    *lo = m < a ? (m < b ? m : b) : (a < b ? a : b);
    *hi = m > a ? (m > b ? m : b) : (a > b ? a : b);
}
#define VZ_GLOW2 6400                    /* SCOPE: the glow's reach, squared (Q4: 5 px) */
/* SCOPE as a phosphor screen, a column at a time: the light of the graticule, of the fill between the curve and the
 * zero line, of the last picture's curve and of the glow (64ths of THEME over the background: vz.lum), then the curve:
 * a THEME body 2.5 px across round a white-hot core (vz.hot), by each pixel's distance from it (ui_graph.c
 * trace_dist: smooth where it is steep). The glow's distance: from the columns' spans within reach (alpha max beta
 * min, no square roots) */
static void vz_scope(void)
{
    static const uint8_t REACH[5] = {80, 78, 73, 64, 48};       /* the glow's reach (Q4 rows) 0..4 columns away */
    static const uint8_t DIV[8] = {16, 42, 68, 94, 146, 172, 198, 224};   /* the divisions, 26 px apart */
    int16_t lo[240], hi[240];
    uint16_t kq[240];
    uint8_t gl[VZ_SPLIT], bd[VZ_SPLIT], hc[VZ_SPLIT], al[VZ_SPLIT];   /* this column by row: glow, body, core, light */
    const int16_t *tq = vz.trace;
    int32_t X, r, j;
    for (X = 0; X < 240; X++) {
        int32_t a, b;
        vz_span(X, &a, &b);
        lo[X] = (int16_t)a;
        hi[X] = (int16_t)b;
    }
    trace_prep(tq, 240, kq);
    for (X = 0; X < 240; X++) {
        int32_t s, yq = tq[X], fa = (lo[X] + 15) >> 4, fb = hi[X] >> 4, ra, rb;   /* (on its own span: the glow at full) */
        uint16_t *col = cv_px + X;
        memset(gl, 0, sizeof gl);
        memset(bd, 0, sizeof bd);
        memset(hc, 0, sizeof hc);
        memset(al, 0, sizeof al);
        for (r = fa < vz_y0 ? vz_y0 : fa; r <= fb && r < vz_y1; r++)
            gl[r - vz_y0] = 26;
        for (s = X - 4; s <= X + 4; s++) {                       /* the glow */
            int32_t m = X > s ? X - s : s - X, k2 = 256 * m * m, l, h;
            if (s < 0 || s > 239)
                continue;
            l = lo[s];
            h = hi[s];
            ra = (l - REACH[m] + 15) >> 4;
            rb = (h + REACH[m]) >> 4;
            ra = ra < vz_y0 ? vz_y0 : ra;
            rb = rb >= vz_y1 ? vz_y1 - 1 : rb;
            for (r = ra; r <= rb; r++) {
                int32_t rq = r * 16, dv = rq < l ? l - rq : rq > h ? rq - h : 0, d2 = k2 + dv * dv, t, g;
                if (d2 >= VZ_GLOW2 || (r >= fa && r <= fb))
                    continue;
                j = r - vz_y0;
                t = (VZ_GLOW2 - d2) >> 7;                        /* 0..50 */
                g = t * t * 43 >> 12;                            /* (a smooth bump: 26 at the curve) */
                if (g > gl[j])
                    gl[j] = (uint8_t)g;
            }
        }
        ra = yq;                                                 /* the body and the core: within 2.5 px */
        rb = yq;
        if (X) {
            ra = tq[X - 1] < ra ? tq[X - 1] : ra;
            rb = tq[X - 1] > rb ? tq[X - 1] : rb;
        }
        if (X < 239) {
            ra = tq[X + 1] < ra ? tq[X + 1] : ra;
            rb = tq[X + 1] > rb ? tq[X + 1] : rb;
        }
        ra = (ra - 40 + 15) >> 4;
        rb = (rb + 40) >> 4;
        ra = ra < vz_y0 ? vz_y0 : ra;
        rb = rb >= vz_y1 ? vz_y1 - 1 : rb;
        for (r = ra; r <= rb; r++) {
            int32_t d = trace_dist(tq, 240, kq, X, r * 16), b = 28 - d, c = 20 - d;   /* (the body whole to 0.75 px, the
                                                                                         * core to 0.25, a pixel of edge) */
            j = r - vz_y0;
            bd[j] = (uint8_t)(b < 0 ? 0 : b > 16 ? 16 : b);
            hc[j] = (uint8_t)(c < 0 ? 0 : c > 16 ? 16 : c);
        }
        if (yq < VZ_AXIS - 16 || yq > VZ_AXIS + 16) {           /* the fill: 12/64 at the curve, nothing at the zero line */
            int32_t fi = 12 * 16 * 256 / (yq - VZ_AXIS);
            ra = yq < VZ_AXIS ? (yq >> 4) + 1 : 121;
            rb = yq < VZ_AXIS ? 119 : (yq - 1) >> 4;
            ra = ra < vz_y0 ? vz_y0 : ra;
            rb = rb >= vz_y1 ? vz_y1 - 1 : rb;
            for (r = ra; r <= rb; r++)
                al[r - vz_y0] = (uint8_t)((r - 120) * fi >> 8);
        }
        if (120 >= vz_y0 && 120 < vz_y1)                         /* the graticule: the zero line, ticks on it */
            al[120 - vz_y0] += 10;
        if (!(X & 7))
            for (r = 118; r <= 122; r++)
                if (r != 120 && r >= vz_y0 && r < vz_y1)
                    al[r - vz_y0] += 7;
        if (!(X & 3))                                            /* (the divisions, dotted) */
            for (j = 0; j < 8; j++)
                if (DIV[j] >= vz_y0 && DIV[j] < vz_y1)
                    al[DIV[j] - vz_y0] += 7;
        if (X && !(X % 40))
            for (r = vz_y0; r < vz_y1; r += 4)
                al[r - vz_y0] += 7;
        if (vz.after_ok) {                                       /* the last picture's curve: its rows in this column */
            int32_t m = vz.after[X], p = X ? (vz.after[X - 1] + m) >> 1 : m, n = X < 239 ? (vz.after[X + 1] + m) >> 1 : m;
            int32_t a0 = m < p ? (m < n ? m : n) : (p < n ? p : n), a1 = m > p ? (m > n ? m : n) : (p > n ? p : n);
            for (r = a0 - 1; r <= a1 + 1; r++)
                if (r >= vz_y0 && r < vz_y1)
                    al[r - vz_y0] += r < a0 || r > a1 ? 3 : 9;
        }
        for (j = 0; j < VZ_SPLIT; j++) {
            int32_t a = al[j] + gl[j], b = bd[j];
            if (!a && !b)
                continue;                                        /* (the background: cv_begin's) */
            a = a > 63 ? 63 : a;
            if (b)
                a += (63 - a) * b >> 4;
            col[(uint32_t)j * cv_w] = hc[j] ? vz.hot[hc[j]] : vz.lum[a];
        }
    }
}
static void vz_scope_done(void)                                       /* (after the second band: its afterimage) */
{
    uint32_t x;
    for (x = 0; x < 240u; x++)
        vz.after[x] = (uint8_t)((vz.trace[x] + 8) >> 4);
    vz.after_ok = 1;
}
#define VZ_FLOOR 196                     /* SPECTRUM: the bars stand on it, their reflection under it */
#define VZ_BARH 184                      /* .. 0 dB */
/* SPECTRUM: 80 bars of 2 px, each row its height's colour (vz.heat, so the loud reach the hot white), the top row by
 * its fraction; the held peaks; the reflection fading under the floor; -15 -30 -45 dB dotted in the gaps */
static void vz_bars(void)
{
    uint32_t b;
    int32_t g, x, y;
    for (g = 1; g < 4; g++)
        if (vz_in(VZ_FLOOR - g * 46, 1))
            for (x = 2; x < 240; x += 3)
                cv_rect(x, VZ_FLOOR - g * 46, 1, 1, T_TINT);
    cv_rect(0, VZ_FLOOR, 240, 1, T_RAISE);
    for (b = 0; b < VZ_NB; b++) {
        int32_t hq = (int32_t)vz.spec[b] * (VZ_BARH * 16) / 255, h = hq >> 4, c = (int32_t)vz.cap[b] * VZ_BARH / 255;
        int32_t ya = VZ_FLOOR - h, yb = VZ_FLOOR, L = h / 4;
        uint16_t *p;
        x = (int32_t)b * 3;
        ya = ya < vz_y0 ? vz_y0 : ya;                    /* the bar: rows VZ_FLOOR - h .. VZ_FLOOR - 1 */
        yb = yb > vz_y1 ? vz_y1 : yb;
        for (y = ya; y < yb; y++) {
            p = cv_px + (uint32_t)(y - vz_y0) * cv_w + (uint32_t)x;
            p[0] = p[1] = vz.heat[12 + (VZ_FLOOR - 1 - y) * 51 / VZ_BARH];
        }
        y = VZ_FLOOR - 1 - h;                            /* (its top: the fraction, darker down the ramp) */
        if ((hq & 15) && y >= vz_y0 && y < vz_y1) {
            p = cv_px + (uint32_t)(y - vz_y0) * cv_w + (uint32_t)x;
            p[0] = p[1] = vz.heat[(12 + h * 51 / VZ_BARH) * (hq & 15) / 16];
        }
        if (c > h + 2) {                                 /* the peak held: a hot line, a softer one under it */
            cv_rect(x, VZ_FLOOR - c - 2, 2, 1, T_TEXT);
            cv_rect(x, VZ_FLOOR - c - 1, 2, 1, vz_c(vz.heat[52]));
        }
        for (y = 0; y < L; y++) {                        /* the reflection: a quarter, fading */
            int32_t row = VZ_FLOOR + 2 + y;
            if (row >= vz_y0 && row < vz_y1) {
                p = cv_px + (uint32_t)(row - vz_y0) * cv_w + (uint32_t)x;
                p[0] = p[1] = vz.heat[18 * (L - y) / L];
            }
        }
    }
}
/* WATERFALL: a row of the history (r: 0 the newest) as heat, pixel by pixel across: between two bands' centres
 * (6 px apart) the one blended into the other */
static void vz_wfline(uint32_t r, uint8_t *out)
{
    const uint8_t *row = vz.wf[(vz.wf_top + r) % VZ_WF];
    uint32_t b, f;
    int32_t x;
    for (b = 0; b < VZ_WB; b++) {
        uint32_t v0 = (row[b / 2u] >> ((b & 1u) * 4u)) & 15u, n = b + 1u < VZ_WB ? b + 1u : b;
        uint32_t v1 = (row[n / 2u] >> ((n & 1u) * 4u)) & 15u;
        if (!b)
            out[0] = out[1] = out[2] = (uint8_t)(v0 * 6u * 45u >> 6);
        for (f = 0; f < 6u; f++) {
            x = (int32_t)(b * 6u + 3u + f);
            if (x < 240)
                out[x] = (uint8_t)((v0 * (6u - f) + v1 * f) * 45u >> 6);   /* (0..63) */
        }
    }
}
static void vz_waterfall(void)
{
    uint8_t va[240], vb[240];                            /* a row's heat; the next (older) one's */
    uint32_t r, k, x;
    for (r = 0; r < VZ_WF; r++) {
        int32_t y = (int32_t)r * 4;
        if (!vz_in(y, 4))
            continue;
        vz_wfline(r, va);
        vz_wfline(r + 1u < VZ_WF ? r + 1u : r, vb);
        for (k = 0; k < 4u; k++) {                       /* (down the row: into the older one) */
            uint16_t *p;
            if (y + (int32_t)k < vz_y0 || y + (int32_t)k >= vz_y1)
                continue;
            p = cv_px + (uint32_t)(y + (int32_t)k - vz_y0) * cv_w;
            for (x = 0; x < 240u; x++)
                p[x] = vz.heat[(va[x] * (4u - k) + vb[x] * k) >> 2];
        }
    }
}
static int32_t vz_avg(uint32_t i)                     /* ORBIT: 4 samples from i, averaged (the hiss off the curve) */
{
    return ((int32_t)scope_snap[i] + scope_snap[i + 1u] + scope_snap[i + 2u] + scope_snap[i + 3u]) / 4;
}
/* ORBIT: an instrument's face (a ring, 24 ticks, the cross dotted) and the curve over it, anti-aliased, the oldest
 * first: its colour by age (faint THEME .. THEME .. hot white), the newest quarter 2 px, its head a dot in a halo */
static void vz_orbit(void)
{
    int32_t pk = vz.peak > 1500 ? vz.peak : 1500, i, n = (int32_t)(SCOPE_N - vz.tau - 4u), px = 0, py = 0;
    uint32_t k;
    vz_aaring(120, 120, 103, 104, T_TINT);
    for (k = 0; k < 24u; k++) {
        uint32_t ang = k * 1024u / 24u;
        int32_t cs = SINE[(ang + 256u) & 1023u], sn = SINE[ang & 1023u], r0 = k % 6u ? 98 : 92;
        vz_aalq(1920 + (r0 * cs >> 11), 1920 + (r0 * sn >> 11), 1920 + (103 * cs >> 11), 1920 + (103 * sn >> 11),
                k % 6u ? T_TINT : T_DIM, 0);
    }
    for (i = 6; i < 236; i += 4) {
        cv_rect(i, 120, 1, 1, T_TINT);
        cv_rect(120, i, 1, 1, T_TINT);
    }
    for (i = 0; i < n; i += 2) {
        int32_t x = 1920 + vz_avg((uint32_t)i) * 1664 / pk, y = 1920 - vz_avg((uint32_t)i + vz.tau) * 1664 / pk;
        if (i) {
            uint32_t q = (uint32_t)(i * 8 / n);          /* 0 .. 7, the newest last */
            vz_aalq(px, py, x, y, q < 6u ? vz_c(vz.lum[18u + q * 9u]) : q == 6u ? T_THEME : vz.glow[3], q >= 6u);
        }
        px = x;
        py = y;
    }
    vz_dot(px, py, 72, vz_c(vz.lum[20]));
    vz_dot(px, py, 40, T_THEME);
    vz_dot(px, py, 24, T_TEXT);
}
/* TUNNEL: a square a beat flies out of the centre, twisting as it comes, its size in sixteenths (no steps); the one
 * born on the bar's first beat in ACCENT with a halo */
static void vz_tunnel(void)
{
    int32_t j;
    for (j = 7; j >= 0; j--) {                                        /* far (small) first */
        int32_t a = j * 256 + 255 - (int32_t)vz.phase, hq = a * a * 16 / 19660, h = hq >> 4;   /* its age, beats * 256 */
        uint32_t ang = (uint32_t)(a * 3 / 16 + (int32_t)(vz.beats % 64u) * 16) & 1023u;   /* (a twist of 135 degrees */
        int acc = !((vz.bn + 8u - (uint32_t)j) % 4u);                                      /* on the way) */
        if (h <= 2 || h >= 172)
            continue;
        if (acc) {
            vz_square(hq + 40, 1, ang, ux_mix(T_BG, T_ACCENT, 22));
            vz_square(hq + 24, 1, ang, ux_mix(T_BG, T_ACCENT, 45));
        }
        vz_square(hq, 1 + h / 40, ang, acc ? T_ACCENT : vz.ramp[4u + (uint32_t)a * 9u / 2048u]);
    }
    vz_dot(1920, 1920, 40, T_TEXT);
}
/* PULSE: a ring a beat out of the centre over slowly turning spokes (fading outward), the level of the music its
 * core: a sun in a halo, hot white at its heart */
static void vz_pulse(void)
{
    int32_t j, s;
    uint32_t a0 = (vz.beats * 24u + vz.phase * 24u / 256u) & 1023u;   /* the spokes: a turn in 43 beats */
    for (j = 0; j < 12; j++) {
        uint32_t ang = (a0 + (uint32_t)j * 1024u / 12u) & 1023u;
        int32_t cs = SINE[(ang + 256u) & 1023u], sn = SINE[ang];
        for (s = 0; s < 3; s++) {
            int32_t ra = 44 + s * 24, rb = ra + 24;
            vz_aalq(1920 + (ra * cs >> 11), 1920 + (ra * sn >> 11), 1920 + (rb * cs >> 11), 1920 + (rb * sn >> 11),
                    vz_c(vz.lum[14 - s * 4]), 0);
        }
    }
    for (j = 4; j >= 0; j--) {                                        /* the oldest (widest) first */
        int32_t a = j * 256 + 255 - (int32_t)vz.phase, r = a * 170 / 1280, t = 7 - j;
        uint16_t c = j ? vz.ramp[11u - (uint32_t)j * 2u] : T_ACCENT;
        if (r > t)
            vz_aaring(120, 120, r - t, r, c);
    }
    {   /* the core: the level, a sun in a narrow halo */
        int32_t r = 6 + (int32_t)vz.level * 34 / 255;
        vz_aaring(120, 120, r - 1, r + 5, vz_c(vz.lum[14]));
        vz_aaring(120, 120, r - 1, r + 3, vz_c(vz.lum[28]));
        vz_aaring(120, 120, r - 1, r + 1, vz_c(vz.lum[44]));
        vz_orb(120, 120, r, vz.hot);
    }
}
/* STARS: in sixteenths of a pixel (they drift, never step), discs growing as they come; the near ones streak */
static void vz_stars(void)
{
    uint32_t i;
    for (i = 0; i < VZ_NSTAR; i++) {
        int32_t z = vz.star[i].z, sx, sy, rq;
        uint16_t c;
        if (z < 24)
            continue;
        sx = 1920 + vz.star[i].x * 1920 / z;
        sy = 1920 + vz.star[i].y * 1920 / z;
        if (sx < -64 || sx > 3904 || sy < -64 || sy > 3904)            /* (gone past the edge) */
            continue;
        rq = 7 + (1024 - z) * 34 / 1024;                              /* (0.4 .. 2.6 px) */
        c = z < 200 && vz.phase < 96u ? T_ACCENT : vz.ramp[6u + (uint32_t)(1024 - z) * 9u / 1024u];
        if (z < 512) {                                                /* near: a streak back to where it was */
            int32_t z2 = z + 40 + (int32_t)(255u - vz.phase) / 4, tx = 1920 + vz.star[i].x * 1920 / z2, ty = 1920 + vz.star[i].y * 1920 / z2;
            vz_aalq(tx, ty, (tx + sx) / 2, (ty + sy) / 2, vz.glow[0], 0);
            vz_aalq((tx + sx) / 2, (ty + sy) / 2, sx, sy, vz.glow[z < 200 ? 2 : 1], 0);
        }
        if (z < 200)                                                  /* (the nearest: a halo) */
            vz_dot(sx, sy, rq * 2, vz.glow[0]);
        vz_dot(sx, sy, rq, c);
    }
}
static void vz_grid(void)
{
    uint32_t k, i;
    for (k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        const step_t *st = seq_steps(t);
        uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, at = t->seq_idx % len, base = at / 16u * 16u;
        int32_t y = (int32_t)k * 60 + 4, playing = song.playing && t->seq_pos < 0x7FFFFFFFu;
        int muted = t->p[P_MUTE] != 0;
        if (!vz_in(y, 52))
            continue;
        for (i = 0; i < 16u; i++) {
            uint32_t s = base + i;
            int32_t x = (int32_t)i * 15 + 1;
            uint16_t c;
            if (s >= len) {
                cv_rect(x + 5, y + 24, 3, 3, T_DIM);
                continue;
            }
            if (st[s].time == ST_NOTE && (st[s].n || st[s].hit))
                c = muted ? T_DIM : (st[s].flags & SF_ACCENT) || st[s].acc ? T_THEME : ux_mix(T_RAISE, T_THEME, 55);
            else if (st[s].time == ST_TIE)
                c = muted ? T_DIM : ux_mix(T_RAISE, T_THEME, 28);
            else
                c = T_RAISE;
            if (playing && s == at)
                c = t->seq_n && !muted ? T_ACCENT : c == T_RAISE ? T_MID : c;
            else if (playing && s + 1u == at)                       /* the trail */
                c = ux_mix(c, T_ACCENT, 35);
            else if (playing && s + 2u == at)
                c = ux_mix(c, T_ACCENT, 15);
            cv_rrect(x, y, 13, 52, 3, c, T_BG);
            if (playing && s == at && c != T_ACCENT)                 /* (the playhead on a rest: a bar under it) */
                cv_rect(x, y + 48, 13, 4, T_ACCENT);
        }
        if (k == song.sel)                                            /* the selected track */
            cv_rect(1, y + 54, 238, 2, T_THEME);
    }
}
static void vz_rain(void)
{
    static const char *const OCT[5] = {"C2", "C3", "C4", "C5", "C6"};
    uint32_t r, i;
    for (i = 0; i < 5u; i++) {                                        /* the octaves */
        cv_rect((int32_t)i * 48, 16, 1, 224, T_TINT);
        if (vz_in(0, 15))
            cv_text((int32_t)i * 48 + 3, 0, &AF_S, OCT[i], T_DIM);
    }
    for (r = 0; r < VZ_RROWS; r++) {
        int32_t y = 16 + (int32_t)r * 5;
        const uint8_t *row = vz.rain[(vz.rain_top + r) % VZ_RROWS];
        uint32_t age = r * 8u / VZ_RROWS;
        if (!vz_in(y, 5))
            continue;
        for (i = 0; i < 60u; i++) {
            uint32_t v = (row[i >> 2] >> ((i & 3u) * 2u)) & 3u;
            if (v)
                cv_rect((int32_t)i * 4, y, 3, 4, !r ? T_TEXT : v == 1u ? vz.trail[age] : v == 2u ? vz.dtrail[age] : vz.glow[3]);
        }
    }
}
/* WHEEL: Camelot n (1..12) at n * 30 degrees from the top, B (major) on the outer ring, A (minor) inside */
static void vz_wheel(void)
{
    static const int16_t SN[12] = {0, 500, 866, 1000, 866, 500, 0, -500, -866, -1000, -866, -500};   /* sin n*30 */
    uint32_t key = edda.camelot, nb[3] = {0, 0, 0}, pcs = 0, k, v, i;
    char b[8];
    if (!key)
        key = edda_cam_of((uint32_t)TSEL->p[P_ROOT], (uint32_t)TSEL->p[P_SCALE]);
    if (key)
        edda_cam_neighbours(key, nb);
    for (k = 0; k < NTRK; k++)                                        /* the pitch classes sounding (not the kits) */
        if (!drum_track(&trk[k]))
            for (v = 0; v < NVOICE; v++)
                if (trk[k].v[v].active && trk[k].v[v].gate)
                    pcs |= 1u << (trk[k].v[v].note % 12u);
    vz_aaring(120, 140, 89, 90, T_TINT);
    vz_aaring(120, 140, 59, 60, T_TINT);
    for (v = 1; v <= 24u; v++) {
        uint32_t n = (v - 1u) / 2u + 1u, maj = (v - 1u) & 1u, r = maj ? 90u : 60u;
        int32_t cx = 120 + SN[n % 12u] * (int32_t)r / 1000, cy = 140 - SN[(n + 3u) % 12u] * (int32_t)r / 1000;
        uint16_t fill = T_SURF, ink = T_DIM;
        if (v == key) {
            fill = T_ACCENT; ink = T_INK;
        } else if ((pcs >> edda_cam_root(v)) & 1u) {
            fill = T_THEME; ink = T_INK;
        } else if (v == nb[0] || v == nb[1] || v == nb[2]) {
            fill = T_RAISE; ink = T_TEXT;
        }
        if (!vz_in(cy - 9, 18))
            continue;
        cv_rrect(cx - 12, cy - 9, 24, 18, 5, fill, T_BG);
        edda_cam_name(v, b);
        GFX_HOOK_ALIGN(cx - 12, cy - 9, cx + 12, cy + 9, AL_HV, "wheel chip centred");
        cv_text_in(cx - 12, cy - 9 + CAP_IN(S, 18), 24, &AF_S, b, ink, fill);
    }
    if (key) {
        static const char *const NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        edda_cam_name(key, b);
        vz_text(60, 120, 124, &AF_L, b, T_TEXT, T_BG);
        str_cpy(b, NOTE[edda_cam_root(key)], sizeof b);
        str_cpy(b + str_len(b), edda_cam_major(key) ? " MAJ" : " MIN", 5);
        vz_text(80, 80, 104, &AF_S, b, T_MID, T_BG);
    } else
        vz_text(60, 120, 124, &AF_L, "--", T_DIM, T_BG);
    for (i = 0; i < 12u; i++)                                         /* (the sounding roots, round the hub) */
        if ((pcs >> i) & 1u) {
            uint32_t n = edda_cam_num(edda_cam_of(i, ED_SC_MAJ));
            vz_dot(1920 + SN[n % 12u] * 704 / 1000, 2240 - SN[(n + 3u) % 12u] * 704 / 1000, 52, T_THEME);
        }
}
static const char *vz_phase_name(void)
{
    static const char *const PH[] = {"PLAYING", "SHAKERS", "STABS", "LOG", "SILENCE", "DROP"};
    if (edda.stopped)
        return edda.resume ? "BACK ON 1" : "STOP";
    if (edda.armed)
        return "RUN ON 1";
    if (!song.playing)
        return "READY";
    return PH[edda.phase < 6u ? edda.phase : 0u];
}
/* BULBS: EDDA's five bulbs (the act): lit, a glass of ACCENT round a white-hot filament, a highlight on the glass and a
 * halo breathing with the beat (the halos first: a neighbour's never covers a glass); out, a dim rim */
static void vz_bulbs(void)
{
    uint32_t i, act = edda.act ? edda.act : 1u, on = 255u - vz.phase;
    int32_t g = 21 + (int32_t)(song.playing ? on * on / 6500u : 0u);
    uint16_t glass[17], lite = ux.light ? T_BG : T_TEXT;              /* (the light: white on the dark palettes, the paper's */
    char b[16];                                                       /* own on the light ones) */
    for (i = 0; i <= 16u; i++)                                        /* (ACCENT at the rim, the light in the middle) */
        glass[i] = swap16(vz_n(ux_mix(T_ACCENT, lite, (int32_t)(i * i * 85u / 256u))));
    for (i = 0; i < act && i < ED_ACTS; i++) {
        int32_t x = 24 + (int32_t)i * 48;
        vz_aaring(x, 58, 19, g, ux_mix(T_BG, T_ACCENT, 12));
        vz_aaring(x, 58, 19, 19 + (g - 19) * 2 / 3, ux_mix(T_BG, T_ACCENT, 24));
        vz_aaring(x, 58, 19, 19 + (g - 19) / 3, ux_mix(T_BG, T_ACCENT, 40));
    }
    for (i = 0; i < ED_ACTS; i++) {
        int32_t x = 24 + (int32_t)i * 48;
        if (i < act) {
            vz_orb(x, 58, 20, glass);
            vz_dot((x - 8) * 16, 50 * 16, 40, ux_mix(T_ACCENT, lite, 70));     /* (the light on the glass) */
        } else {
            vz_aaring(x, 58, 0, 18, ux_mix(T_BG, T_DIM, 12));
            vz_aaring(x, 58, 18, 20, T_DIM);
        }
    }
    str_cpy(b, "ACT ", sizeof b);
    fmt_int(b + 4, (int32_t)act);
    vz_text(0, 240, 98, &AF_M, b, T_MID, T_BG);
    vz_text(0, 240, 126, &AF_L, vz_phase_name(), edda.stopped ? T_REC : T_TEXT, T_BG);
    if (edda.phase != ED_IDLE && edda_phase_beats(edda.phase)) {      /* the phase's beats */
        uint32_t n = edda_phase_beats(edda.phase), done = n > edda.left ? n - edda.left : 0u;
        int32_t w = (232 - 4 * ((int32_t)n - 1)) / (int32_t)n;
        for (i = 0; i < n; i++)
            cv_rrect(4 + (int32_t)i * (w + 4), 166, w, 12, 4, i < done ? T_THEME : T_RAISE, T_BG);
    }
    str_cpy(b, "BAR ", sizeof b);
    fmt_int(b + 4, (int32_t)edda.bar + (song.playing ? 1 : 0));
    vz_text(0, 240, 182, &AF_M, b, T_MID, T_BG);
}
/* CLOCK's figures as a lamp's segments: hexagons, their ends pointed at 45 degrees (the slanted edges' pixels half
 * covered), apart by a gap; from (x0, y0) to (x1, y1) along a row or a column, t px thick (odd) */
static void vz_seg(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t t, uint16_t c)
{
    const uint16_t *rv;
    int32_t h = t / 2, k;
    c = vz_n(c);
    if (c == cv_bg)
        return;
    rv = ramp(c, cv_bg);
    for (k = -h; k <= h; k++) {
        int32_t ak = k < 0 ? -k : k;
        if (y0 == y1) {                                  /* across */
            int32_t a = x0 + ak, b = x1 - ak, y = y0 + k;
            if (b < a || !vz_in(y, 1))
                continue;
            vz_px(a, y, c, rv, 8u);
            vz_px(b, y, c, rv, 8u);
            if (b - a > 1)
                cv_rect(a + 1, y, b - a - 1, 1, c);
        } else {                                         /* up and down */
            int32_t a = y0 + ak, b = y1 - ak, x = x0 + k;
            if (b < a)
                continue;
            vz_px(x, a, c, rv, 8u);
            vz_px(x, b, c, rv, 8u);
            if (b - a > 1)
                cv_rect(x, a + 1, 1, b - a - 1, c);
        }
    }
}
/* a figure of seven segments: w x h at x, y, t px thick (odd); d 0..9, else blank (the segments unlit: off) */
static void vz_digit(int32_t x, int32_t y, int32_t w, int32_t h, int32_t t, uint32_t d, uint16_t c, uint16_t off)
{
    static const uint8_t SEG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};   /* a b c d e f g */
    uint32_t m = d < 10u ? SEG[d] : 0u;
    int32_t L = x + t / 2, R = x + w - 1 - t / 2, T = y + t / 2, B = y + h - 1 - t / 2, M = (T + B) / 2, g = 2;
    vz_seg(L + g, T, R - g, T, t, m & 0x01u ? c : off);  /* a */
    vz_seg(R, T + g, R, M - g, t, m & 0x02u ? c : off);  /* b */
    vz_seg(R, M + g, R, B - g, t, m & 0x04u ? c : off);  /* c */
    vz_seg(L + g, B, R - g, B, t, m & 0x08u ? c : off);  /* d */
    vz_seg(L, M + g, L, B - g, t, m & 0x10u ? c : off);  /* e */
    vz_seg(L, T + g, L, M - g, t, m & 0x20u ? c : off);  /* f */
    vz_seg(L + g, M, R - g, M, t, m & 0x40u ? c : off);  /* g */
}
static void vz_clock(void)
{
    uint32_t bpm = (uint32_t)song.g[G_BPM], i, bar = edda.bar + (song.playing ? 1u : 0u), beat = beat_n & 3u;
    uint16_t lit = song.playing ? T_THEME : T_TEXT, ghost = ux.bw ? T_BG : ux_mix(T_BG, T_RAISE, 50);   /* (the unlit: a lamp's ghost) */
    char b[16];
    if (song.g[G_CLOCK] && midi_beat_samples)                         /* (CLK EXT: the tempo it follows) */
        bpm = (uint32_t)FS * 60u / midi_beat_samples;
    bpm = bpm > 999u ? 999u : bpm;
    vz_digit(32, 14, 48, 84, 9, bpm >= 100u ? bpm / 100u : 10u, lit, ghost);
    vz_digit(96, 14, 48, 84, 9, bpm >= 10u ? bpm / 10u % 10u : 10u, lit, ghost);
    vz_digit(160, 14, 48, 84, 9, bpm % 10u, lit, ghost);
    vz_text(0, 240, 102, &AF_S, song.g[G_CLOCK] ? "BPM  CLK EXT" : "BPM", T_MID, T_BG);
    fmt_int(b, (int32_t)bar);
    str_cpy(b + str_len(b), ".", 4);
    fmt_int(b + str_len(b), song.playing ? (int32_t)beat + 1 : 0);
    vz_text(0, 240, 128, &AF_L, b, T_TEXT, T_BG);
    for (i = 0; i < 4u; i++) {                                        /* (stopped: none lit); the beat fills as it goes */
        int now = song.playing && i == beat;
        int32_t x = 8 + (int32_t)i * 58;
        cv_rrect(x, 174, 50, 24, 4, now ? (i ? T_SEL : ux_mix(T_SEL, T_ACCENT, 40)) : T_RAISE, T_BG);
        if (now)
            cv_rect(x, 174, (int32_t)vz.phase * 50 / 255, 24, i ? T_THEME : T_ACCENT);
    }
}
/* the foot's note: a message, the visualiser's name for a while, the knob just turned, the tempo just set */
static void vz_toast(void)
{
    char s[40];
    const char *unit = "";
    s[0] = 0;
    if (ui.msg_t && ui.msg[0])
        str_cpy(s, ui.msg + ((uint8_t)ui.msg[0] < 32u), sizeof s);   /* (a message's icon byte: its words) */
    else if (vz.name_ms && (int32_t)(vz.name_ms - fm1_ms) > 0) {
        fmt_int(s, (int32_t)vz.n + 1);
        str_cpy(s + str_len(s), "/12 ", 6);
        str_cpy(s + str_len(s), VZ_NAME[vz.n], sizeof s - 8u);
    } else if (ui.hot_t && ui.hot_col < 4u) {
        int16_t *vp;
        const param_desc_t *d = home_param(ui.hot_col, &vp);
        char v[12];
        param_format(d, *vp, v, &unit);
        str_cpy(s, d->label, 12);
        str_cpy(s + str_len(s), " ", 2);
        str_cpy(s + str_len(s), v, 12);
        if (unit && unit[0]) {
            str_cpy(s + str_len(s), " ", 2);
            str_cpy(s + str_len(s), unit, 8);
        }
    } else if (ui.bpm_t) {
        fmt_int(s, song.g[G_BPM]);
        str_cpy(s + str_len(s), " BPM", 5);
    }
    if (!s[0] || !vz_in(206, 28))
        return;
    cv_rrect(20, 206, 200, 28, 8, T_SURF, T_BG);
    if (text_w(&AF_M, s) <= 184) {
        GFX_HOOK_ALIGN(20, 206, 220, 234, AL_HV, "viz note centred");
        cv_text_in(28, 206 + CAP_IN(M, 28), 184, &AF_M, s, T_TEXT, T_SURF);
    } else
        cv_free_text(28, 206 + CAP_IN(M, 28), &AF_M, s, T_TEXT, T_SURF, 184);
}
static void vz_band(uint32_t band)
{
    vz_y0 = band ? VZ_SPLIT : 0;
    vz_y1 = band ? 240 : VZ_SPLIT;
    cv_begin(240, (uint32_t)(vz_y1 - vz_y0), T_BG);
    cv_oy = -vz_y0;
    switch (vz.n) {
    case VZ_SCOPE: vz_scope(); break;
    case VZ_SPECTRUM: vz_bars(); break;
    case VZ_WATERFALL: vz_waterfall(); break;
    case VZ_ORBIT: vz_orbit(); break;
    case VZ_TUNNEL: vz_tunnel(); break;
    case VZ_PULSE: vz_pulse(); break;
    case VZ_STARS: vz_stars(); break;
    case VZ_GRID: vz_grid(); break;
    case VZ_RAIN: vz_rain(); break;
    case VZ_WHEEL: vz_wheel(); break;
    case VZ_BULBS: vz_bulbs(); break;
    case VZ_CLOCK: vz_clock(); break;
    }
    vz_toast();
    cv_oy = 0;
    cv_blit(0, (uint32_t)vz_y0);
    if (band && vz.n == VZ_SCOPE)
        vz_scope_done();
}
/* ui_draw_page: the visualiser over HOME; 0: not shown (leaving HOME closes it) */
static int vz_page(void)
{
    if (!ui.viz)
        return 0;
    if (!ui.home) {
        ui.viz = 0;
        return 0;
    }
    if (ui.force) {                                                   /* (a whole picture) */
        vz_snap();
        vz_band(0);
        vz_band(1);
        vz.band = 0;
        ui.head_sig = ui.foot_sig = ui.graph_sig = ~0u;               /* (HOME redraws whole after it) */
    } else {
        if (!vz.band)
            vz_snap();
        vz_band(vz.band);
        vz.band ^= 1u;
    }
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {                      /* (as the page: the second message) */
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (ui.bpm_t)
        ui.bpm_t--;
    if (ui.hot_t)
        ui.hot_t--;
    ui.force = 0;
    return 1;
}
/* HOME tapped (ui_input.c): on HOME the visualiser; in it the next one, past the last HOME */
static void vz_home_tap(void)
{
    int clean = ui.home && !ui.viz && !ui.plk_src && !ui.entry_open && !song.seq_mode && !ui.layer;
    if (ui.viz && ui.home) {
        if (vz.n + 1u >= VZ_COUNT) {
            ui.viz = 0;
            vz.last = 0;
            go_home();
            return;
        }
        vz.n++;
        vz.last = vz.n;
        vz.name_ms = (fm1_ms + VZ_NAME_MS) | 1u;
        vz.fresh = 1;
        ui.force = 1;
        return;
    }
    go_home();
    if (clean) {
        ui.viz = 1;
        vz.n = vz.last < VZ_COUNT ? vz.last : 0u;
        vz.name_ms = (fm1_ms + VZ_NAME_MS) | 1u;
        vz.fresh = 1;
        vz.band = 0;
    }
}
