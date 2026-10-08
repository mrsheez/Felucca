/* SPDX-License-Identifier: GPL-3.0-only
 * EDDA OS: VIZ, twelve full-screen visualisers over HOME (SLOOP's: a tap of HOME on the tracks screen). Included by
 * ui_draw.c (ui_draw_page draws it, ui_input.c's HOME tap opens and steps it: vz_home_tap).
 *
 * HOME tapped on HOME opens the visualiser shown last (SCOPE at power-on); tapped again: the next one, its number and
 * name at the foot for a moment; past the twelfth: HOME again (SCOPE the next time). A page button leaves for its page;
 * HOME held opens the MENU (closed: the visualiser again). Everything else stays as on HOME: the keys play, KNOB 1..4
 * are HOME's four (the name and value come up at the foot), SELECT the tempo, PRESET the sound, ALGORITHM the track,
 * PLAY, REC, the layers (their map over it while held). The screen stays on while it shows (MENU > SCREEN OFF waits).
 *   SCOPE      the output, triggered, a glowing trace, the last picture as an afterimage
 *   SPECTRUM   40 bands, 43 Hz .. 10.5 kHz (a 512-point FFT of the scope's 23 ms, Hann), gradient bars with
 *              their peaks held and a reflection under the floor
 *   WATERFALL  the spectrum of the last 40 pictures, the newest at the top, on a four-colour ramp
 *   ORBIT      the output against itself a quarter period later: a phase portrait (a pure tone is a circle), the
 *              newest part of the curve the brightest
 *   TUNNEL     a square a beat flies out of the centre, twisting as it comes; the one born on the bar's first beat lit
 *   PULSE      a ring a beat out of the centre over slowly turning spokes, the level of the music its core
 *   STARS      a starfield: the tempo its speed, a rush after every beat, the near ones streak
 *   GRID       the four tracks' 16 steps round their playheads (a trail behind each); what sounds lit; the selected
 *              track underlined
 *   RAIN       the notes sounding, falling and fading: C2 .. B6 across (the octaves marked), the drum tracks' apart
 *   WHEEL      the Camelot wheel: the key (MENU > EDDA > KEY, else the selected track's ROOT / SCALE), the keys it
 *              mixes into, the roots of the notes sounding
 *   BULBS      EDDA's five bulbs (the act) on the beat; the run's phase and its beats, the bar
 *   CLOCK      the tempo in big figures, bar . beat, the beats of the bar
 * Drawing: the screen in two bands of the canvas (gfx.c: 240 x 120 each). A frame draws one band (a forced one both),
 * the two of a picture from one snapshot of the music (vz_snap): never two moments in one picture. The SPI to the
 * panel (12 MHz) bounds it, about 12 pictures a second, the UI's work in between. Colours: the palette's tokens
 * only (GREY and MONO stay gray). Texts lie inside one band. The state lives in the pool, off the audio's RAM. */
#define VZ_COUNT 12u
enum { VZ_SCOPE, VZ_SPECTRUM, VZ_WATERFALL, VZ_ORBIT, VZ_TUNNEL, VZ_PULSE, VZ_STARS, VZ_GRID, VZ_RAIN, VZ_WHEEL,
       VZ_BULBS, VZ_CLOCK };
static const char *const VZ_NAME[VZ_COUNT] = {"SCOPE", "SPECTRUM", "WATERFALL", "ORBIT", "TUNNEL", "PULSE", "STARS",
                                             "GRID", "RAIN", "WHEEL", "BULBS", "CLOCK"};
#define VZ_SPLIT 120                     /* the bands: screen rows 0..119 and 120..239 */
#define VZ_NB 40u                        /* SPECTRUM / WATERFALL: bands */
#define VZ_WF 40u                        /* WATERFALL: rows (6 px) */
#define VZ_NSTAR 64u
#define VZ_RROWS 44u                     /* RAIN: rows (5 px, from y 16: the octave marks above) */
#define VZ_RLO 36                        /* RAIN: C2 (36) .. B6 (95), 4 px a note */
#define VZ_NAME_MS 1600u
/* the bands' FFT bins (43.07 Hz each at the scope's 22.05 kHz): one bin each up to 430 Hz, then log-spaced */
static const uint8_t VZ_EDGE[VZ_NB + 1u] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 17, 18, 20, 23, 25, 28,
                                           31, 34, 38, 42, 47, 52, 57, 64, 71, 78, 87, 96, 107, 118, 131, 146, 161,
                                           179, 198, 220, 244};
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
    uint8_t wf[VZ_WF][VZ_NB / 2u];       /* WATERFALL: 4 bits a band; row wf_top the newest */
    uint8_t wf_top;
    uint8_t after[240];                  /* SCOPE: the last picture's trace, y */
    uint8_t after_ok;
    struct { int16_t x, y, z; } star[VZ_NSTAR];
    uint32_t rnd;
    uint8_t rain[VZ_RROWS][16];           /* RAIN: 2 bits a note from VZ_RLO (1 a synth track, 2 a drum track) */
    uint8_t rain_top;
    uint16_t ramp[16];                   /* WATERFALL, SPECTRUM: BG .. SEL .. THEME .. ACCENT .. TEXT */
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
/* a ring (r0 < r <= r1 from cx, cy; r0 0: a disc), row by row in the band: no gaps between radii */
static void vz_ring(int32_t cx, int32_t cy, int32_t r0, int32_t r1, uint16_t c)
{
    int32_t y, ya = cy - r1, yb = cy + r1;
    if (r1 <= 0)
        return;
    ya = ya < vz_y0 ? vz_y0 : ya;
    yb = yb >= vz_y1 ? vz_y1 - 1 : yb;
    for (y = ya; y <= yb; y++) {
        int32_t dy = y - cy, xo = (int32_t)isqrt32((uint32_t)(r1 * r1 - dy * dy)), xi;
        if (r0 > 0 && dy * dy < r0 * r0) {
            xi = (int32_t)isqrt32((uint32_t)(r0 * r0 - dy * dy));
            cv_rect(cx - xo, y, xo - xi, 1, c);
            cv_rect(cx + xi + 1, y, xo - xi, 1, c);
        } else
            cv_rect(cx - xo, y, 2 * xo + 1, 1, c);
    }
}
/* a square frame centred on the screen, half-size h, t px thick, turned by ang (1024 a turn): t concentric outlines */
static void vz_square(int32_t h, int32_t t, uint32_t ang, uint16_t c)
{
    int32_t cs = SINE[(ang + 256u) & 1023u], sn = SINE[ang & 1023u], k;
    if (h < 1)
        return;
    t = t < 1 ? 1 : t > h ? h : t;
    for (k = 0; k < t; k++) {
        int32_t r = h - k, ax = (r * cs - r * sn) >> 15, ay = (r * sn + r * cs) >> 15;   /* the corner (r, r) turned */
        int32_t bx = (-r * cs - r * sn) >> 15, by = (-r * sn + r * cs) >> 15;            /* .. (-r, r) */
        cv_line(120 + ax, 120 + ay, 120 + bx, 120 + by, c);
        cv_line(120 + bx, 120 + by, 120 - ax, 120 - ay, c);
        cv_line(120 - ax, 120 - ay, 120 - bx, 120 - by, c);
        cv_line(120 - bx, 120 - by, 120 + ax, 120 + ay, c);
    }
}
/* the ramps from the palette (ux_mix is not for inner loops): once a picture */
static void vz_ramps(void)
{
    uint32_t i;
    for (i = 0; i < 16u; i++)
        vz.ramp[i] = i < 5u ? ux_mix(T_BG, T_SEL, (int32_t)(i * 25u)) : i < 10u ? ux_mix(T_SEL, T_THEME, (int32_t)((i - 5u) * 20u)) :
                     i < 14u ? ux_mix(T_THEME, T_ACCENT, (int32_t)((i - 10u) * 25u)) : ux_mix(T_ACCENT, T_TEXT, (int32_t)((i - 14u) * 50u));
    vz.glow[0] = ux_mix(T_BG, T_THEME, 18);
    vz.glow[1] = ux_mix(T_BG, T_THEME, 42);
    vz.glow[2] = T_THEME;
    vz.glow[3] = ux_mix(T_THEME, T_TEXT, 70);
    for (i = 0; i < 8u; i++) {
        vz.trail[i] = ux_mix(T_THEME, T_BG, (int32_t)(i * 11u));
        vz.dtrail[i] = ux_mix(T_ACCENT, T_BG, (int32_t)(i * 11u));
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
        for (i = 0; i < VZ_NB; i += 2u)
            vz.wf[vz.wf_top][i / 2u] = (uint8_t)((now_b[i] >> 4) | (now_b[i + 1u] >> 4) << 4);
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

static void vz_scope(void)
{
    static uint8_t ty[240];
    int32_t x, a = 104, pk = vz.peak > 1500 ? vz.peak : 1500, g;
    static const int8_t GW[4] = {9, 5, 3, 1};                         /* the glow: wide and faint to the thin core */
    cv_rect(0, 120, 240, 1, T_RAISE);
    for (g = 1; g < 4; g++) {
        cv_rect(0, 120 - a * g / 4, 240, 1, T_TINT);
        cv_rect(0, 120 + a * g / 4, 240, 1, T_TINT);
    }
    if (vz.after_ok)                                                  /* the last picture, faint */
        for (x = 1; x < 240; x++)
            cv_line(x - 1, vz.after[x - 1], x, vz.after[x], T_SEL);
    for (x = 0; x < 240; x++) {
        int32_t y = 120 - scope_snap[vz.trig + (uint32_t)x] * a / pk;
        ty[x] = (uint8_t)(y < 6 ? 6 : y > 233 ? 233 : y);
    }
    for (g = 0; g < 4; g++) {
        int32_t w = GW[g];
        for (x = 1; x < 240; x++)
            if (vz_in((ty[x] < ty[x - 1] ? ty[x] : ty[x - 1]) - w / 2, (ty[x] > ty[x - 1] ? ty[x] : ty[x - 1]) - (ty[x] < ty[x - 1] ? ty[x] : ty[x - 1]) + w))
                cv_line_t(x - 1, ty[x - 1] - w / 2, x, ty[x] - w / 2, vz.glow[g], w);
    }
}
static void vz_scope_done(void)                                       /* (after the second band: its afterimage) */
{
    int32_t x, pk = vz.peak > 1500 ? vz.peak : 1500, y;
    for (x = 0; x < 240; x++) {
        y = 120 - scope_snap[vz.trig + (uint32_t)x] * 104 / pk;
        vz.after[x] = (uint8_t)(y < 4 ? 4 : y > 235 ? 235 : y);
    }
    vz.after_ok = 1;
}
#define VZ_FLOOR 196                     /* SPECTRUM: the bars stand on it, their reflection under it */
static void vz_bars(void)
{
    uint32_t b;
    int32_t g;
    for (g = 1; g < 4; g++)                                           /* -15 -30 -45 dB */
        cv_rect(0, VZ_FLOOR - g * 46, 240, 1, T_TINT);
    for (b = 0; b < VZ_NB; b++) {
        int32_t x = (int32_t)b * 6, h = (int32_t)vz.spec[b] * 184 / 255, c = (int32_t)vz.cap[b] * 184 / 255, y;
        for (y = 0; y < h; y += 12) {                                 /* the bar in 12 px steps of the ramp */
            int32_t k = y + 12 > h ? h - y : 12;
            cv_rect(x, VZ_FLOOR - y - k, 5, k, vz.ramp[3u + (uint32_t)y * 12u / 184u]);
        }
        if (c > h + 2)
            cv_rect(x, VZ_FLOOR - c - 2, 5, 2, T_TEXT);
        if (h > 4)                                                    /* the reflection: a quarter, faint */
            cv_rect(x, VZ_FLOOR + 3, 5, h / 4, vz.ramp[2]);
    }
    cv_rect(0, VZ_FLOOR + 1, 240, 1, T_RAISE);
}
static void vz_waterfall(void)
{
    uint32_t r, b;
    for (r = 0; r < VZ_WF; r++) {
        int32_t y = (int32_t)r * 6;
        const uint8_t *row = vz.wf[(vz.wf_top + r) % VZ_WF];
        if (!vz_in(y, 6))
            continue;
        for (b = 0; b < VZ_NB; b++) {
            uint32_t v = (row[b / 2u] >> ((b & 1u) * 4u)) & 15u;
            if (v)
                cv_rect((int32_t)b * 6, y, 6, 6, vz.ramp[v]);
        }
    }
}
static int32_t vz_avg(uint32_t i)                     /* ORBIT: 4 samples from i, averaged (the hiss off the curve) */
{
    return ((int32_t)scope_snap[i] + scope_snap[i + 1u] + scope_snap[i + 2u] + scope_snap[i + 3u]) / 4;
}
static void vz_orbit(void)
{
    int32_t pk = vz.peak > 1500 ? vz.peak : 1500, i, n = (int32_t)(SCOPE_N - vz.tau - 4u), px = 0, py = 0;
    vz_ring(120, 120, 103, 104, T_TINT);
    cv_rect(0, 120, 240, 1, T_TINT);
    cv_rect(120, 0, 1, 240, T_TINT);
    for (i = 0; i < n; i += 2) {
        int32_t x = 120 + vz_avg((uint32_t)i) * 104 / pk, y = 120 - vz_avg((uint32_t)i + vz.tau) * 104 / pk;
        if (i) {
            uint32_t q = (uint32_t)(i * 4 / n);                       /* the oldest quarter faint, the newest bright */
            uint16_t c = vz.glow[q];
            cv_line(px, py, x, y, c);
            if (q >= 2u)
                cv_line(px + 1, py, x + 1, y, c);
            if (q == 3u)
                cv_line(px, py + 1, x, y + 1, c);
        }
        px = x;
        py = y;
    }
}
static void vz_tunnel(void)
{
    int32_t j;
    for (j = 7; j >= 0; j--) {                                        /* far (small) first */
        int32_t a = j * 256 + 255 - (int32_t)vz.phase, h = a * a / 19660;   /* its age, beats * 256: the edge at 6 */
        uint32_t ang = (uint32_t)(a * 3 / 16 + (int32_t)(vz.beats % 64u) * 16) & 1023u;   /* (a twist of 135 degrees */
        uint16_t c = (vz.bn + 8u - (uint32_t)j) % 4u ? vz.ramp[4u + (uint32_t)a * 9u / 2048u] : T_ACCENT;   /* on the way) */
        if (h > 2 && h < 172)
            vz_square(h, 1 + h / 40, ang, c);
    }
    vz_ring(120, 120, 0, 3, T_TEXT);
}
static void vz_pulse(void)
{
    int32_t j;
    uint32_t a0 = (vz.beats * 24u + vz.phase * 24u / 256u) & 1023u;   /* the spokes: a turn in 43 beats */
    for (j = 0; j < 12; j++) {
        uint32_t ang = (a0 + (uint32_t)j * 1024u / 12u) & 1023u;
        int32_t cs = SINE[(ang + 256u) & 1023u], sn = SINE[ang];
        cv_line(120 + (44 * cs >> 15), 120 + (44 * sn >> 15), 120 + (116 * cs >> 15), 120 + (116 * sn >> 15), T_TINT);
    }
    for (j = 4; j >= 0; j--) {                                        /* the oldest (widest) first */
        int32_t a = j * 256 + 255 - (int32_t)vz.phase, r = a * 170 / 1280, t = 7 - j;
        uint16_t c = j ? vz.ramp[11u - (uint32_t)j * 2u] : T_ACCENT;
        if (r > t)
            vz_ring(120, 120, r - t, r, c);
    }
    {   /* the core: the level, its glow */
        int32_t r = 6 + (int32_t)vz.level * 34 / 255;
        vz_ring(120, 120, r, r + 6, vz.glow[0]);
        vz_ring(120, 120, r - 2, r, vz.glow[1]);
        vz_ring(120, 120, 0, r - 2, vz.ramp[6u + vz.level * 9u / 255u]);
    }
}
static void vz_stars(void)
{
    uint32_t i;
    for (i = 0; i < VZ_NSTAR; i++) {
        int32_t z = vz.star[i].z, sx, sy, s;
        if (z < 24)
            continue;
        sx = 120 + vz.star[i].x * 120 / z;
        sy = 120 + vz.star[i].y * 120 / z;
        s = 1 + (1024 - z) / 220;
        if (z < 512) {                                                /* near: a streak back to where it was */
            int32_t z2 = z + 40 + (int32_t)(255u - vz.phase) / 4, tx = 120 + vz.star[i].x * 120 / z2, ty = 120 + vz.star[i].y * 120 / z2;
            cv_line(tx, ty, sx, sy, vz.glow[z < 200 ? 1 : 0]);
        }
        cv_rect(sx - s / 2, sy - s / 2, s, s, z < 200 && vz.phase < 96u ? T_ACCENT : vz.ramp[6u + (uint32_t)(1024 - z) * 9u / 1024u]);
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
    vz_ring(120, 140, 89, 90, T_TINT);
    vz_ring(120, 140, 59, 60, T_TINT);
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
            vz_ring(120 + SN[n % 12u] * 44 / 1000, 140 - SN[(n + 3u) % 12u] * 44 / 1000, 0, 3, T_THEME);
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
static void vz_bulbs(void)
{
    uint32_t i, act = edda.act ? edda.act : 1u, on = 255u - vz.phase;
    char b[16];
    for (i = 0; i < ED_ACTS; i++) {
        int32_t x = 24 + (int32_t)i * 48;
        if (i < act) {
            int32_t g = 21 + (int32_t)(song.playing ? on * on / 6500u : 0u);
            vz_ring(x, 58, 20, g, T_SEL);
            vz_ring(x, 58, 0, 20, T_ACCENT);
            vz_ring(x, 58, 0, 7, ux_mix(T_ACCENT, T_TEXT, 60));
        } else
            vz_ring(x, 58, 18, 20, T_DIM);
    }
    str_cpy(b, "ACT ", sizeof b);
    fmt_int(b + 4, (int32_t)act);
    vz_text(0, 240, 98, &AF_M, b, T_MID, T_BG);
    vz_text(0, 240, 126, &AF_L, vz_phase_name(), edda.stopped ? T_REC : T_TEXT, T_BG);
    if (edda.phase != ED_IDLE) {                                      /* the phase's beats */
        uint32_t n = edda_phase_beats(edda.phase), done = n - edda.left;
        int32_t w = (232 - 4 * ((int32_t)n - 1)) / (int32_t)n;
        for (i = 0; i < n; i++)
            cv_rect(4 + (int32_t)i * (w + 4), 166, w, 12, i < done ? T_THEME : T_RAISE);
    }
    str_cpy(b, "BAR ", sizeof b);
    fmt_int(b + 4, (int32_t)edda.bar + (song.playing ? 1 : 0));
    vz_text(0, 240, 182, &AF_M, b, T_MID, T_BG);
}
/* a figure of seven segments: w x h at x, y, t px thick; d 0..9, else blank */
static void vz_digit(int32_t x, int32_t y, int32_t w, int32_t h, int32_t t, uint32_t d, uint16_t c, uint16_t off)
{
    static const uint8_t SEG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};   /* a b c d e f g */
    uint32_t m = d < 10u ? SEG[d] : 0u, s;
    int32_t hh = (h - t) / 2;
    for (s = 0; s < 7u; s++) {
        uint16_t k = (m >> s) & 1u ? c : off;
        switch (s) {
        case 0: cv_rect(x + t, y, w - 2 * t, t, k); break;                    /* a */
        case 1: cv_rect(x + w - t, y + t, t, hh - t, k); break;               /* b */
        case 2: cv_rect(x + w - t, y + hh + t, t, h - hh - 2 * t, k); break;  /* c */
        case 3: cv_rect(x + t, y + h - t, w - 2 * t, t, k); break;            /* d */
        case 4: cv_rect(x, y + hh + t, t, h - hh - 2 * t, k); break;          /* e */
        case 5: cv_rect(x, y + t, t, hh - t, k); break;                       /* f */
        case 6: cv_rect(x + t, y + hh, w - 2 * t, t, k); break;               /* g */
        }
    }
}
static void vz_clock(void)
{
    uint32_t bpm = (uint32_t)song.g[G_BPM], i, bar = edda.bar + (song.playing ? 1u : 0u), beat = beat_n & 3u;
    uint16_t lit = song.playing ? T_THEME : T_TEXT;
    char b[16];
    if (song.g[G_CLOCK] && midi_beat_samples)                         /* (CLK EXT: the tempo it follows) */
        bpm = (uint32_t)FS * 60u / midi_beat_samples;
    bpm = bpm > 999u ? 999u : bpm;
    vz_digit(32, 14, 48, 84, 9, bpm >= 100u ? bpm / 100u : 10u, lit, T_TINT);
    vz_digit(96, 14, 48, 84, 9, bpm >= 10u ? bpm / 10u % 10u : 10u, lit, T_TINT);
    vz_digit(160, 14, 48, 84, 9, bpm % 10u, lit, T_TINT);
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
        str_cpy(s, ui.msg, sizeof s);
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
