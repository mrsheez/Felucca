/* SPDX-License-Identifier: GPL-3.0-only */
/* EDDA OS: THE ARRIVAL rendered on the host through the firmware's own code (firmware/src/arrival.c loads the song
 * as SAVE > ARRIVAL's LOAD does, PLAY plays its arrangement: the sequencer, the engines, the effects and the master,
 * block by block as the audio ISR runs them).
 *   build/host/arrival_render OUTDIR [SONG] [SECONDS] [STEMS]
 * writes OUTDIR/NN_NAME.wav (44.1 kHz stereo, 16-bit: the master as the DAC gets it, full scale = the DAC's -6 dBFS
 * ceiling) for every song, or song SONG (1..13, 0 all), the whole arrangement (SECONDS: at most that long), with
 * STEMS 1 each track alone too (NN_NAME_Tk.wav); OUTDIR "-": no files. Prints per song: the length, the peak, the
 * loudness (RMS, dBFS), the voices given up (the shared budget of 8: by track, the audible ones, the held notes
 * cut), the master limiter's work, the voices sounding. MASTER_Q12 (env): the MASTER (4096 full; 512 for
 * tests/arrival_check.py's stems, under the limiter). ARRIVAL_CHECK=1 (env, tests/run_tests.sh): each song must
 * play 3 to 4 minutes and sound, clip nowhere, cut no held note and ask the limiter for at most 8 dB; exit 1 if not. */
#include <stdio.h>
#include <stdint.h>
/* which voices the shared budget gives up: per track, and how loud they still were (env_out, Q15) */
static uint32_t kill_trk[4], kill_loud[4], kill_held[4];
struct voice_kill_probe;
#define VOICE_KILL_HOOK(v) arv_kill_probe(v)
static void arv_kill_probe(const void *v);
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
/* a sampled hit's peak still to come against its own peak (dB): how much of it the take cuts off */
static double kit_left_db(const voice_t *v)
{
    const smp_zone_t *z;
    uint32_t pos = 0;
    int32_t st[2] = {0, 0}, pk = 1, pk2 = 0;
    if ((v->s[6] != 1 && v->s[6] != 3) || (uint32_t)v->s[5] >= 0x8000u)
        return -120.0;
    z = smp_zone((uint32_t)v->s[5]);
    while (pos < z->n) {
        int32_t a = smp_decode(z, &pos, st, 0);
        a = a < 0 ? -a : a;
        pk = a > pk ? a : pk;
        if (pos > v->ph[0] && a > pk2)
            pk2 = a;
    }
    return 20.0 * log10((pk2 + 1.0) / pk);
}
static uint32_t kill_note[128], kill_note_loud[128];
static void arv_kill_probe(const void *pv)
{
    const voice_t *v = (const voice_t *)pv;
    uint32_t k;
    for (k = 0; k < NTRK; k++)
        if (v >= trk[k].v && v < trk[k].v + NVOICE) {
            int loud = trk[k].engine == ENGI_DRUM ? kit_left_db(v) > -20.0 : v->env_out > 3277;
            kill_trk[k]++;
            kill_loud[k] += loud;                       /* (above -20 dB: an audible cut; a sampled hit: of its peak
                                                         * still to come) */
            if (trk[k].engine == ENGI_DRUM) {
                kill_note[v->note & 127]++;
                kill_note_loud[v->note & 127] += loud;
            }
            kill_held[k] += v->gate != 0;               /* (a held note cut, not a release tail) */
        }
}

static void wav16(const char *path, const int16_t *x, uint32_t frames)
{
    FILE *f = fopen(path, "wb");
    uint32_t v;
    uint16_t a = 1, ch = 2, ba = 4, bits = 16;
    uint32_t sr = FS, br = FS * 4u;
    if (!f) {
        perror(path);
        exit(1);
    }
    fwrite("RIFF", 1, 4, f); v = 36u + frames * 4u; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); v = frames * 4u; fwrite(&v, 4, 1, f);
    fwrite(x, 4, frames, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/arrival";
    uint32_t only = argc > 2 ? (uint32_t)atoi(argv[2]) : 0u, maxs = argc > 3 ? (uint32_t)atoi(argv[3]) : 420u;
    uint32_t stems = argc > 4 ? (uint32_t)atoi(argv[4]) : 0u, pass;
    uint32_t n, bad = 0, checked = 0;
    int nowav = !strcmp(dir, "-"), check = getenv("ARRIVAL_CHECK") && atoi(getenv("ARRIVAL_CHECK"));
    for (pass = 0; pass < (stems ? 5u : 1u); pass++)
    for (n = 0; n < arv_count(); n++) {
        uint32_t max = maxs * FS, frames = 0, started = 0, clipped = 0, kills0, rc, i, tail = 0, limited = 0;
        uint32_t vmax[NTRK] = {0}, full = 0, blocks = 0;
        int32_t lim_max = 0;
        int16_t *buf;
        double sum = 0;
        int32_t peak = 0;
        char path[512], nm[32];
        if (only && n + 1u != only)
            continue;
        buf = malloc((size_t)max * 4u + 16u);
        ui_power_on();
        song.master_q12 = getenv("MASTER_Q12") ? (uint32_t)atoi(getenv("MASTER_Q12")) : 4096u;   /* (MASTER: full,
                                                         * the pot at the top; lower: the levels before the limiter) */
        arv_load(n);
        if (pass)                                       /* a stem: track pass alone */
            for (i = 0; i < NTRK; i++)
                trk[i].p[P_MUTE] = i + 1u != pass;
        rc = chain_prepare();
        if (rc) {
            printf("arrival: %s: chain_prepare %u\n", arv_song(n)->name, rc);
            bad++;
            free(buf);
            continue;
        }
        kills0 = voice_kills;
        memset(kill_trk, 0, sizeof kill_trk);
        memset(kill_loud, 0, sizeof kill_loud);
        memset(kill_held, 0, sizeof kill_held);
        memset(kill_note, 0, sizeof kill_note);
        memset(kill_note_loud, 0, sizeof kill_note_loud);
        while (frames + CTL <= max) {
            int32_t o[2 * CTL];
            mix_block(o, CTL);
            if (song.playing || chain.running)
                started = 1;
            limited += lim_env > LIM_T + 64;
            {   /* the voices sounding: each track's most, and how often all 8 are taken */
                uint32_t k2, j, tot = 0;
                for (k2 = 0; k2 < NTRK; k2++) {
                    uint32_t c = 0;
                    for (j = 0; j < NVOICE; j++)
                        c += trk[k2].v[j].active && trk[k2].v[j].stage != 4u;
                    vmax[k2] = c > vmax[k2] ? c : vmax[k2];
                    tot += c;
                }
                full += tot >= NVOICE;
                blocks++;
            }
            lim_max = lim_env > lim_max ? lim_env : lim_max;
            for (i = 0; i < CTL; i++) {
                int32_t l = o[2 * i], r = o[2 * i + 1];
                int32_t al = l < 0 ? -l : l, ar = r < 0 ? -r : r;
                clipped += al > 32767 || ar > 32767;
                peak = al > peak ? al : peak;
                peak = ar > peak ? ar : peak;
                sum += (double)l * l + (double)r * r;
                buf[2u * (frames + i)] = (int16_t)clamp(l, -32768, 32767);
                buf[2u * (frames + i) + 1u] = (int16_t)clamp(r, -32768, 32767);
            }
            frames += CTL;
            if (started && !song.playing && !chain.running && ++tail > (uint32_t)(2u * FS / CTL))
                break;                                  /* the arrangement ended: 2 s of its tails, then stop */
        }
        str_cpy(nm, arv_song(n)->name, sizeof nm);
        for (i = 0; nm[i]; i++)
            nm[i] = nm[i] == ' ' ? '_' : nm[i];
        if (pass)
            snprintf(path, sizeof path, "%s/%02u_%s_T%u.wav", dir, n + 1u, nm, pass);
        else
            snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, n + 1u, nm);
        if (!nowav)
            wav16(path, buf, frames);
        if (check && !pass) {                           /* the song as the album needs it */
            double secs = frames / (double)FS - 2.0, gr = lim_max > LIM_T ? 20.0 * log10((double)lim_max / LIM_T) : 0.0;
            uint32_t held = kill_held[0] + kill_held[1] + kill_held[2] + kill_held[3];
            int ok = secs >= 180.0 && secs <= 240.0 && peak > 8192 && !clipped && !held && gr <= 8.0;
            printf("arrival: %02u %-18s %5.1f s, peak %5.1f dBFS, limiter at most %.1f dB, %u held notes cut, %u clipped: %s\n",
                   n + 1u, arv_song(n)->name, secs, 20.0 * log10(peak / 32768.0), gr, held, clipped, ok ? "ok" : "FAIL");
            bad += !ok;
            checked++;
        }
        printf("arrival: %02u %-14s %3u BPM %5.1f s  peak %6.1f dBFS  rms %6.1f dBFS  voices given up %u  clipped %u\n",
               n + 1u, arv_song(n)->name, arv_song(n)->bpm, frames / (double)FS,
               peak ? 20.0 * log10(peak / 32768.0) : -120.0, sum > 0 ? 10.0 * log10(sum / (2.0 * frames) / (32768.0 * 32768.0)) : -120.0,
               voice_kills - kills0, clipped);
        printf("arrival:    given up by track %u %u %u %u (above -20 dB: %u %u %u %u, held: %u %u %u %u); limiter on %.1f %% "
               "of blocks, at most %.1f dB%s\n", kill_trk[0], kill_trk[1], kill_trk[2], kill_trk[3], kill_loud[0], kill_loud[1],
               kill_loud[2], kill_loud[3], kill_held[0], kill_held[1], kill_held[2], kill_held[3], 100.0 * limited / (frames / CTL), lim_max > LIM_T ? 20.0 * log10((double)lim_max / LIM_T) : 0.0,
               pass ? " (a stem)" : "");
        if (!pass) {
            uint32_t q;
            printf("arrival:    hits given up (note: all / loud):");
            for (q = 0; q < 128u; q++)
                if (kill_note[q])
                    printf(" %u:%u/%u", q, kill_note[q], kill_note_loud[q]);
            printf("\n");
        }
        if (!pass)
            printf("arrival:    voices: most per track %u %u %u %u, all 8 taken %.1f %% of the time\n", vmax[0], vmax[1],
                   vmax[2], vmax[3], 100.0 * full / (blocks ? blocks : 1u));
        free(buf);
    }
    if (check)
        printf(bad ? "arrival: %u FAILED\n" : "arrival: %u songs ok\n", bad ? bad : checked);
    return bad != 0;
}
