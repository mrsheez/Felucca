/* SPDX-License-Identifier: GPL-3.0-only
 * EDDA OS: an IMA ADPCM encoder for the sound packs (tools/arrival_pack.py), bit-exact with the firmware's decoder
 * (firmware/src/eng_sample.c smp_decode: 4 bits a sample, the low nibble first, from predictor 0 / index 0), that
 * looks ahead instead of taking each sample's nearest code (tools/sampleio.py ima_encode, the editor's):
 *
 *   a beam of the K best code paths so far (distinct decoder states), each extended by all 16 codes; the cost of a
 *   path is the energy of its error through a first-order weighting, e[n] - c[n] e[n - 1], with c[n] following the
 *   sound's own spectral tilt (given per sample by the caller), so the noise left sits under the sound's spectrum
 *   (voiced: below, where the voice is loud; unvoiced: flat) rather than flat across the band
 *
 *   adpcm_xq K [DECODED.raw] < in.bin > out.bin
 *   in:  int32 n, then n x int16 samples, then n x float32 c (0: plain squared error)
 *   out: (n + 1) / 2 bytes of codes; the SNR in dB on stderr; DECODED.raw: what the firmware will play (int16)
 *
 * Built by tools/arrival_pack.py with the host's cc. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const int16_t STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
    4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767};
static const int8_t IDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

typedef struct {
    double cost;
    float e;                     /* the last error (decoded - wanted) */
    int32_t pred;
    int16_t idx;
    int16_t from;                /* the node of the beam before */
    uint8_t code;
} node_t;

static int by_cost(const void *a, const void *b)
{
    double d = ((const node_t *)a)->cost - ((const node_t *)b)->cost;
    return d < 0 ? -1 : d > 0;
}

int main(int argc, char **argv)
{
    int K = argc > 1 ? atoi(argv[1]) : 16, n, i, j, c, nb = 1, m;
    int16_t *x;
    float *w;
    uint8_t *code, *from, *out;
    node_t *beam, *cand;
    double se = 0, sx = 0;
    if (K < 1 || K > 64 || fread(&n, 4, 1, stdin) != 1 || n < 0 || n > (1 << 26))
        return 2;
    x = malloc(2 * (size_t)n + 2);
    w = malloc(4 * (size_t)n + 4);
    code = malloc((size_t)n * K + 1);
    from = malloc((size_t)n * K + 1);
    out = calloc((size_t)(n + 1) / 2 + 1, 1);
    beam = malloc(sizeof *beam * K);
    cand = malloc(sizeof *cand * K * 16);
    if (!x || !w || !code || !from || !out || !beam || !cand ||
        fread(x, 2, (size_t)n, stdin) != (size_t)n || fread(w, 4, (size_t)n, stdin) != (size_t)n)
        return 2;
    beam[0] = (node_t){0, 0, 0, 0, 0, 0};
    for (i = 0; i < n; i++) {
        int nc = 0;
        for (j = 0; j < nb; j++) {
            int step = STEP[beam[j].idx];
            for (c = 0; c < 16; c++) {
                int vd = step >> 3, p, ix;
                double e, s;
                if (c & 4) vd += step;
                if (c & 2) vd += step >> 1;
                if (c & 1) vd += step >> 2;
                p = beam[j].pred + ((c & 8) ? -vd : vd);
                p = p > 32767 ? 32767 : p < -32768 ? -32768 : p;
                ix = beam[j].idx + IDX[c & 7];
                ix = ix < 0 ? 0 : ix > 88 ? 88 : ix;
                e = (double)p - x[i];
                s = e - (double)w[i] * beam[j].e;
                cand[nc++] = (node_t){beam[j].cost + s * s, (float)e, p, (int16_t)ix, (int16_t)j, (uint8_t)c};
            }
        }
        qsort(cand, (size_t)nc, sizeof *cand, by_cost);
        for (nb = 0, j = 0; j < nc && nb < K; j++) {       /* the K best with distinct decoder states */
            for (m = 0; m < nb; m++)
                if (beam[m].pred == cand[j].pred && beam[m].idx == cand[j].idx)
                    break;
            if (m < nb)
                continue;
            beam[nb] = cand[j];
            code[(size_t)i * K + nb] = cand[j].code;
            from[(size_t)i * K + nb] = (uint8_t)cand[j].from;
            nb++;
        }
    }
    for (j = 0, i = n - 1; i >= 0; i--) {                  /* the best path, back from its end (beam[0]) */
        c = code[(size_t)i * K + j];
        out[i >> 1] |= (uint8_t)(c << ((i & 1) * 4));
        j = from[(size_t)i * K + j];
    }
    {   /* what the firmware will play: decode it, measure it */
        int pred = 0, idx = 0;
        FILE *df = argc > 2 ? fopen(argv[2], "wb") : 0;
        for (i = 0; i < n; i++) {
            int step = STEP[idx], vd = step >> 3;
            c = (out[i >> 1] >> ((i & 1) * 4)) & 15;
            if (c & 4) vd += step;
            if (c & 2) vd += step >> 1;
            if (c & 1) vd += step >> 2;
            pred += (c & 8) ? -vd : vd;
            pred = pred > 32767 ? 32767 : pred < -32768 ? -32768 : pred;
            idx += IDX[c & 7];
            idx = idx < 0 ? 0 : idx > 88 ? 88 : idx;
            se += ((double)pred - x[i]) * ((double)pred - x[i]);
            sx += (double)x[i] * x[i];
            if (df) {
                int16_t v = (int16_t)pred;
                fwrite(&v, 2, 1, df);
            }
        }
        if (df)
            fclose(df);
    }
    fwrite(out, 1, (size_t)(n + 1) / 2, stdout);
    fprintf(stderr, "%.2f\n", se > 0 ? 10 * log10(sx / se) : 99.0);
    return 0;
}
