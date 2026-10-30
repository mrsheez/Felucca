# EDDA OS — the Mr. Sheez performance firmware for the M-VAVE FM-1

EDDA OS is a fork of **Felucca 1.7.0** (Hügelton Instruments, GPL-3.0) built for the Mr. Sheez live set:
the thirteen songs of *EDDA: The Arrival* ready to play (section 2.21), a tempo that is the tempo the decks show,
a pitch bend to pull the set into phase by hand, the key of the record on the decks, the EDDA rhythm language,
the transition by subtraction on one key, a hard stop that re-enters on the one, 16-WAV drum kits and the
sequencer out over MIDI with clock. Everything Felucca 1.7.0 does is still there and behaves as Felucca's
documentation says unless a section below says otherwise.

| | |
| --- | --- |
| Version | 1.7.0-EDDA (the splash says `v1.7.0-edda`; the package identity FM-1_917) |
| Base | Felucca 1.7.0 (hugelton/Felucca `main` at 65b21d3, merged), felucca-bsp (MPL-2.0) and Felucca-Engines (MIT) as submodules; EDDA OS's commits on top |
| Licence | GPL-3.0-only, as Felucca |
| Verified | {{CHECKS}} |
| Built | with JieLi's compiler on GitHub: {{IMAGE}} |
| Not yet | flashed onto an FM-1 (section 7) |
| Install | one HTML file, Chrome or Edge, the FM-1 on USB (section 1.1) |

## 0. What 1.7.0-EDDA changes

**Felucca 1.7.0 underneath** (EDDA OS 1.1.5 had Felucca 1.1.5.1): step conditions (1:2 … 4:4, FILL, !FILL) and
SEQ > DETAIL (each step's CHANCE, RATCH, NUDGE, VEL on the knobs); NUDGE (a step up to half a step early or 7/16 late,
played with the track's QUANTIZE off); an INSERT effect per track (SOFT HARD FOLD FUZZ CRUSH PHASR FLANG CHOR, with
MIX); MIDI LEARN; LFO 2 synced to the bars; 128 motion records; SEQ TOOLS' EUCLID, DOUBLE and HALF; a MIDI IN channel
per track; MIDI Program Change; the HALL reverb, PHASE FOLD, humanize (VEL / TIME), the DEEP / TIGHT / PERC kits; FM6's
32-voice bank from the editor; the second core (MENU > SYSTEM > 2ND CORE, beta, off unless you turn it on); the
community fixes to the TRS clock, external-clock drift and TRS input. Projects are Felucca's FUN10.

**New in EDDA OS:**
- **The true tempo** (2.22): the FM-1 plays 44,117.6 samples a second, not the 44,100 every Felucca counts, so its
  tempo ran 0.04 % fast. EDDA OS counts the real rate: 116 BPM on the FM-1 is 116.00 on the deck.
- **TEMPO BEND** (2.23): GLO + **G#4** / **A#4** held slow down / speed up the set by about 4 %, a deck's pitch bend.
- **USR4 moved** to 72 KiB at 0xEA000, clear of the FM-1's own radio calibration and Bluetooth address (2.9).

**Changed:** FILL is Felucca's FILL condition, still on GLO + **A4** (a tap latches it to the end of the next bar, a
hold is FILL while held); micro timing is Felucca's NUDGE (2.13); MIDI LEARN is on GLO + **F5** (Felucca's D4 is
EDDA's RUN); the track filter is FX > **DJ FILTER**; the dotted echoes are Felucca's own; DRUM's KIT runs STD … 77,
DEEP, TIGHT, PERC, then USR1–USR4 and EDDA.

**Not in the device's build** (the app slot is full: Felucca 1.7.0 alone fills 513 KB of its 581,564 bytes): the
twelve visualisers, the USB serial console (a developer's tool) and USB audio's 48 kHz option (the FM-1 still records
to a computer over USB, at 44.1 kHz). The browser emulator has the visualisers; `EDDA_VIZ=1`, `FELUCCA_CDC=1` and
`FELUCCA_UAC_48K=1` build each of them (section 6), not all into the slot together.

**From 1.1.5:** EDDA OS 1.1.5's projects open (their nudges, with the track's QUANTIZE off so they play; their fills
as FILL steps; kits and dotted times renumbered; THE ARRIVAL's song). A sample kept in 1.1.5's USR4 is not: that flash
is Felucca's FM6 voice bank now. Upload it to USR4 again.

---

## 1. Install it

**Not from the iPhone.** Installing and the editor both talk to the FM-1 over Web MIDI with SysEx. No
iPhone browser has Web MIDI (Safari never shipped it, and Chrome on iOS uses Safari's engine), so the install
and the kit uploads happen on the laptop (Windows, macOS or Linux).

### 1.1 The one-file installer (recommended)

`EDDA-OS-Installer-1.7.0.html` is the whole installer with the package inside it: no site, no download, no
account.

1. Connect the FM-1 to the laptop with a USB **data** cable and switch it on. Close every app that uses MIDI
   (DAWs, rekordbox's MIDI settings, an editor tab).
2. Open the file in **Chrome or Edge** (double-click it; if another browser opens, right-click → Open with).
3. Allow MIDI (with SysEx) when the browser asks, and press **Install**. The page takes the FM-1 into its
   update mode, writes EDDA OS and restarts it: about two minutes, the bar to 100 % and the page says **Done**.
   Do not unplug meanwhile. If anything stops it, press Install again: the update mode resumes where it was.
4. The FM-1 starts EDDA OS (`v1.7.0-edda` on the splash). SAVE → turn to **ARRIVAL** for the songs.

The installer writes only the FM-1's app area. It never writes the flash that holds your projects, samples and
presets, or the unit's own radio calibration and Bluetooth address.

### 1.2 Route A — GitHub builds it and hosts the installer

Everything happens in the browser. Use the files from this delivery: `edda-os.bundle` (the git history),
or `edda-os-fork.zip` if you prefer GitHub Desktop.

1. Sign in at github.com. Open **github.com/hugelton/Felucca** and press **Fork**. Keep the name.
2. In your fork, open the **Actions** tab and press *I understand my workflows, go ahead and enable them*
   (forks start with workflows switched off).
3. Press **Code → Codespaces → Create codespace on main**. A Linux machine opens in the browser (the free
   monthly allowance covers this many times over).
4. Drag `edda-os.bundle` into the Codespace's file list on the left. In its terminal, run:

   ```
   git fetch ./edda-os.bundle edda:edda
   git push -u origin edda
   ```

   Close the Codespace (Codespaces → stop) when the push is done.
5. In the fork's **Settings → General → Default branch**, switch to `edda` (the installer page may only be
   published from the default branch). In **Settings → Pages → Build and deployment → Source**, choose
   **GitHub Actions**.
6. **Actions → EDDA OS → Run workflow** (branch `edda`). The run fetches the toolchain, builds the
   package, runs the test suites against it, and publishes the site: about 25 minutes. Its summary
   page lists the package, its size against the FM-1's app slot, its SHA-256, and any check that did not pass.
7. On the laptop, in Chrome or Edge, open `https://<your-user>.github.io/Felucca/webapp/installer/`, connect
   the FM-1 by USB, allow MIDI when asked, and press **Install**. The editor is at `…/webapp/editor/`.

Every later push to `edda` rebuilds and republishes. The package is also kept with each run under
**Artifacts → edda-os-fwsc** for the command-line installer (1.4), and `python3 web/make_site.py --onefile
felucca-1.7.0-EDDA.fwsc 1.7.0-EDDA EDDA-OS-Installer-1.7.0.html` makes the one-file installer from it.

### 1.3 Route B — build on the Windows laptop (WSL2)

WSL2 runs Ubuntu inside Windows 10/11, and JieLi's toolchain runs there natively.

1. PowerShell as administrator: `wsl --install -d Ubuntu`, then restart and finish the Ubuntu setup.
2. In Ubuntu:

   ```
   sudo apt update && sudo apt install -y git python3-pil python3-fonttools python3-numpy curl xz-utils
   git clone --recurse-submodules -b edda https://github.com/mrsheez/Felucca edda-os
   cd edda-os && sh edda-build.sh --release 1.7.0-EDDA   # -> build/felucca-1.7.0-EDDA.fwsc
   python3 web/make_site.py build/felucca-1.7.0-EDDA.fwsc 1.7.0-EDDA /tmp/edda-site
   cd /tmp/edda-site && python3 -m http.server 8000
   ```

   (With the bundle instead: copy it into Ubuntu first, e.g. `cp /mnt/c/Users/<you>/Downloads/edda-os.bundle ~`,
   and `git clone ./edda-os.bundle edda-os -b edda`.)
3. In Windows Chrome or Edge open `http://localhost:8000/webapp/installer/` (Web MIDI is allowed on
   localhost) and install as in Route A.

### 1.4 The command-line installer on Windows

For a package from Route A's artifact or Route B: install Python 3 from python.org, then in PowerShell:

```
py -m pip install mido python-rtmidi
py tools\fm1_install.py felucca-1.7.0-EDDA.fwsc
```

Close every other app that uses MIDI first (DAWs, the editor tab).

### 1.5 Going back

- **To Felucca:** install it from `hugelton.github.io/Felucca`. EDDA OS keeps Felucca's update loader (it
  answers as `FM-1_900`), its release identity (`FM-1_917` for 1.7.x) and its flash layout, so either installs
  over the other. Projects, presets and user samples stay (EDDA's 4th sample slot sits in flash Felucca leaves
  unused); Felucca 1.7 opens EDDA projects (FUN10 is its own format) and reads a value it does not have as its last
  one: KIT USR1–4 and EDDA as PERC, SAMPLE's USR4, EKIT and LOG as USR3. It keeps the settings it does not know.
- **To M-VAVE's firmware:** the installer's **Return to official V15** (with M-VAVE's `FM-1.fwsc`).
- **If the FM-1 does not start:** see Felucca's README, *If the FM-1 does not start* (another cable, close
  MIDI apps, run the installer again; the recovery tool there needs a Mac and a Seeed XIAO RP2040).

Installing firmware is at your own risk.

### 1.6 Your fork (github.com/mrsheez/Felucca)

Steps 1–4 of Route A are done: the fork exists, its workflows run, `edda` and `main` carry EDDA OS (1.7.0
from branch `edda-1.7`). Every push builds the package with JieLi's compiler, runs the suites and pushes the
run's logs and package to the `ci-out` branch. One setting is left, because a workflow's own token may not
turn Pages on: **Settings → Pages → Build and deployment → Source: GitHub Actions**. Then **Actions → EDDA OS →
Run workflow** (branch `main`), and the installer is at `https://mrsheez.github.io/Felucca/webapp/installer/`.

Without Pages, the latest run's package is at
`https://github.com/mrsheez/Felucca/raw/ci-out/felucca-1.7.0-EDDA.fwsc` (its SHA-256 in the run's summary). Install
it with the one-file installer, the command-line installer (1.4), or serve the installer page from the laptop: with
Python 3 on Windows, download the repository (**Code → Download ZIP**, branch `edda`), put the package in its folder,
and run there

```
py web\make_site.py felucca-1.7.0-EDDA.fwsc 1.7.0-EDDA site
cd site
py -m http.server 8000
```

then open `http://localhost:8000/webapp/installer/` in Chrome or Edge (Web MIDI is allowed on localhost).

---

## 2. What EDDA OS adds

### 2.1 KEY — the Camelot lock (MENU > EDDA > KEY)
Set the key of the record on the Omnis-Duo as its Camelot code, **1A–12B**. Every synth track gets that
root and scale (A = minor, B = major), and any track whose key quantiser was OFF is switched to SNAP. The
kit track is never touched. 8A (A minor) is the EDDA home key.

In the GLO layer the top keys walk the Camelot path without the menu: **C5** one step down the wheel,
**D5** one step up, **E5** across to the relative key. With no KEY set, the path starts from the selected
track's own key. The key goes out as show cue CC 24.

### 2.2 The EDDA pattern bank (SEQ > PATTERNS, after Felucca's own)
Melodic lines are written in C and land in the track's root when loaded; drum grids use the DRUM lanes,
ghosts at 60.

| # | Name | Kind | Steps / div | What it is |
| --- | --- | --- | --- | --- |
| E01 | 3STEP | drums | 16 / 1/16 | Kick on 1, the and of 2, 4; dry rims on 2 and 4; straight hats accented on the beats |
| E02 | SGIJA | drums | 16 / 1/16 | Syncopated kick, claps answering, off-beat hats |
| E03 | GQOM | drums | 16 / 1/16 | Broken kick, toms as the melody, no hats |
| E04 | KIZOMBA | drums | 16 / 1/16 | Kick with the push on the and of 2, rim clicks, soft hats |
| E05 | AFROBT | drums | 16 / 1/16 | Sixteenth hats, open hat on the and of 2 and 4, kick on 1 and the and of 3 |
| E06 | OGENE12 | drums | **12 / 8T** | The 12-pulse bell timeline (2 2 1 2 2 2 1) on the BELL lane: one bar of 12/8 |
| E07 | CLAVE32 | drums | 16 / 1/16 | 3-2 son clave on the RIM |
| E08 | SHKR16 | drums | 16 / 1/16 | Straight-sixteenth shakers, the bounce from velocity |
| E09 | LOGDRUM | melodic | 16 / 1/16 | Root, fifth below, flat seventh below, a slide into the one |
| E10 | LOG2 | melodic | 16 / 1/16 | The answer, up to the flat third and back |
| E11 | SUBROLL | melodic | 16 / 1/16 | A held sub, the fifth below on the and of 3 |
| E12 | STABS | melodic | 16 / 1/16 | Minor stabs on the and of 1, the and of 2 and the 4 |
| E13 | BITTERSW | melodic | 16 / 1/16 | i–VI–III–VII as triads, one chord a beat |
| E14 | HILIFE | melodic | 16 / 1/16 | Highlife picking over I–IV–V–I |
| E15 | OJA | melodic | 16 / 1/16 | A pentatonic call: a held note answered by a short figure |
| E16 | LOG2BAR | melodic | **32 / 1/16** | The log drum as a two-bar call and answer |
| E17 | TALKDRM | melodic | 16 / 1/16 | Slides between two pitches, two quick strokes at the end |

Polymeter is free: OGENE12 on the drums against a 16-step bass crosses 12/8 over 4/4.

### 2.3 The run — transition by subtraction (GLO + D4)
The run starts on the next one and moves on the beat clock:

| Phase | SHORT / LONG | What happens |
| --- | --- | --- |
| SHAKERS | 1 / 2 bars | Hats, claps and rims rest; the lead mutes |
| STABS | 1 / 2 bars | The stabs track fades to nothing |
| LOG | 2 bars | The log drum and the kick alone |
| SILENCE | 1 beat | Everything muted |
| DROP | on the one | Levels, mutes and lanes exactly as before; cue note D2 |

MENU > EDDA > RUN picks SHORT or LONG. PLAY ends a run and restores the mix at once. With SEQ OUT on, the
run subtracts on MIDI OUT too: rested lanes and muted tracks send nothing, the stabs fade by velocity.

### 2.4 The hard stop and the re-entry (GLO + E4)
**E4** mutes everything at once while the clock keeps running; **E4** again re-enters on the next one.
Cue notes E2 (stop) and D2 (re-entry).

### 2.5 REVEAL — the arrangement engine (MENU > EDDA > REVEAL)
Every 8th bar the kick rests (the hole); bars 17–24 of every 32 move the SNARE hits to the RIM lane (the
backbeat changes voice, not place). Live remaps: the steps are not touched, OFF is instant.

### 2.6 FILL (GLO + A4)
EDDA's fills are Felucca's FILL condition (SEQ > DETAIL, KNOB 1 past 100 %: 1:2 … 4:4, FILL, !FILL): a FILL step rests
until FILL is on, a !FILL step plays only while it is off. **A4 tapped** latches FILL through the end of the next bar
(tapped again: off early); **A4 held** past a third of a second is FILL while held, as Felucca's key. The bank's drum
grids carry fills on their last steps. Cue CC 25.

### 2.7 MUTATE (GLO + B4)
On the next one, the drums track's BELL lane becomes a Euclidean rhythm E(3, 5 or 7, LEN) with a random
rotation, first onset accented. Every other lane stays as it was. Its own seed: a night can be replayed.

### 2.8 User drum kits — 16 WAVs (DRUM > KIT USR1–USR4)
A user sample slot can hold a 16-pad kit. The DRUM engine's KIT knob (EDIT 1) gains **USR1–USR4**; the
grid, the patterns and the keys then play your WAVs.

| Pad | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Sound | KICK | SNARE | CLAP | HAT CL | HAT OP | TOM | RIM | BELL |
| GM note | 36 | 38 | 39 | 42 | 46 | 45 | 37 | 56 |

| Pad | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Sound | LO TOM | HI TOM | MID TOM | CONGA | CRASH | HI CONGA | RIDE | RIDE BL |
| GM note | 41 | 43 | 47 | 48 | 49 | 50 | 51 | 53 |

DRUM's KIT knob runs STD, 66, 10, 77, 80 (Felucca's), DEEP, TIGHT, PERC (Felucca 1.6's), then **USR1–USR4** and
**EDDA** (THE ARRIVAL's kit, 2.21). Pads 1–8 are the grid's lanes. A note with no pad of its own plays its lane's pad, pitched to it, so every
DRUM pattern plays on every kit, including one with 8 pads. TUNE ±12 semitones; TONE below 64 darkens;
DECY below 64 shortens (gone after 25 ms at 0, 0.8 s at 32, the whole sample at 64 and up); SNAP above 64
skips into the attack (tighter), below 64 fades it in (softer), up to 50 ms; ACC, DRV and the lane LEVELs
as the synth kit. A pad plays one voice at a time; the closed hat chokes the open one.

**Making one in the editor** (laptop, Chrome or Edge): Samples → a slot → tick **16-pad kit** → drop a WAV
on each pad (or click it) → name it → **Upload**. **From a terminal:**
`py tools\fm1_sample_upload.py kit 2 MYKIT kick.wav snare.wav ... ride_bell.wav` (`-` leaves a pad empty;
`kitbuild` writes the files without a device). Any WAV works: 8–32 bit, float, any rate, any channels. Each
pad is trimmed of trailing silence and holds up to 4 s; a slot holds 80 KiB (USR4 72 KiB), and when the kit does
not fit the longest pads drop to 11025 Hz until it does.

### 2.9 A fourth sample slot (USR4)
SAMPLE SET 8, GRAIN SRC 8, SLICE SRC USR4, KIT USR4, the editor's 4th slot, and the backup. 72 KiB (about 6.6 s at
22,050 Hz) at 0xEA000–0xFC000: flash Felucca does not use, above Felucca's FM6 voice bank (0xE7000) and the two
sectors that hold the FM-1's own radio calibration (0xE8000) and Bluetooth address (0xE9000), which EDDA OS never
writes (1.1.5's USR4 started at 0xE7000 and could). The editor and `tools/fm1_sample_upload.py` read each slot's
size from the FM-1 and size a kit for it.

### 2.10 CHORD+ and VOIC LEAD (SCL > CHORD)
With CHRD on and QNT WHITE, the selected track's black keys change the chord while held:

| Key | F#3 | G#3 | A#3 | C#4 | D#4 | F#4 | G#4 | A#4 | C#5 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Change | MIN | 7TH | MAJ7 | SUS4 | 9TH | INV | +BASS | STRUM | OPEN |

They combine (MIN + 7TH = m7; SUS4 + 7TH = 7sus4) and the CHORD page names the result. STRUM plays the
notes 18 ms apart, low to high. **VOIC LEAD** picks, from each chord's inversions, the one that moves least
from the last chord: a keyboard player's voice leading.

### 2.11 The track filter (FX > DJ FILTER)
One knob per track, as a DJ mixer's filter: left of centre a low-pass from 16.5 kHz down to 100 Hz, right a
high-pass from 22 Hz up to 7.8 kHz, 12 dB/octave, off at 0 (bit for bit). The last in the track's chain: after the
SLICER and Felucca's INSERT, before the level and the sends. Its page follows INSERT 2. Automatable and lockable like
any knob.

### 2.12 Dotted echoes (FX > DLY > TIME)
**1/8D, 1/16D, 1/4D** after the straight and triplet times: Felucca's own since 1.5.1 (EDDA's came first; a 1.1.5
project's dotted time is renumbered on load).

### 2.13 Micro timing (SEQ > DETAIL > KNOB 3 NUDGE)
Felucca's NUDGE: a step from half a step early (−8) to 7/16 late (+7), in sixteenths of its own length; live
recording keeps each note's time. Nudges play when the track's **QUANTIZE** is OFF (SEQ > AUTOMATION, its QUANTIZE
row, KNOB 4); ON (the default) plays every step on its start. The grid itself never moves, so nudges never
accumulate. 1.1.5's −3…+3 nudges load as the same sixteenths, their track's QUANTIZE off.

### 2.14 The exact grid
Felucca rounds each step's length on its own, so a 1/16 track and an 8T track drift apart (54 ms after
ten minutes at 120 BPM) and the ARP loses its remainder (93 ms late after 16 bars at 128 BPM). EDDA OS
counts every step, the click, the beat clock, the SLICER, the FX layer, MIDI clock out and the ARP on one fractional
grid: all of them meet on every beat, for as long as the set runs. Checked for ten minutes at 128 BPM.

### 2.15 SEQ OUT — the sequencer on MIDI OUT (MENU > EDDA > SEQ OUT)
- **OFF** (default): as Felucca, only the keys go out.
- **NOTES**: what the sequencer plays goes out on USB MIDI in the block it plays in: each track on its
  channel (1–4, the same as its keys), drum lanes as their GM notes, accents at 127, chords, ties (held),
  slides (legato: the next note on before the last one off) and ratchets. Every note-on gets its note-off:
  at the gate's end, a rest, STOP, a mute, SEQ OUT switched off. Muted tracks (MUTE, the run, the hard
  stop, the FX layer) start nothing.
- **+CLOCK**: NOTES plus MIDI clock (24 a beat, on the exact grid), START with PLAY, STOP. It runs while
  stopped so a DAW or drum machine holds the tempo. Never sent while the FM-1 follows an external clock.

### 2.16 Show cues (MENU > EDDA > SHOW CUES, channel 16)

| Message | Meaning |
| --- | --- |
| CC 20 / CC 22 | bars since play / beat of the bar |
| CC 21 | ACT: bulbs lit 1–5 (MENU > EDDA > ACT, or GLO + G4) |
| CC 23 / CC 24 / CC 25 | the run's phase / the Camelot key 1–24 / FILL |
| CC 26 / CC 27 | the SONG section playing, 1–4 (A–D) / THE ARRIVAL's song loaded, 1–13 (0: none) |
| notes C2 F2 D2 E2 G2 A2 | bar, run start, drop or re-entry, hard stop, bell re-rolled, a section starts |

### 2.17 VIZ — twelve full-screen visualisers (GLO + G5; not in the device's build)
They do not fit the app slot beside Felucca 1.7.0 (11.6 KB): the device's build leaves them out and GLO + G5 says
VIZ NOT BUILT IN. The browser emulator has them, and a build with `EDDA_VIZ=1` (and, to make room, without
something else: section 6) puts them on the FM-1: **GLO + G5** opens one over HOME, again the next, its name at the
foot for a moment; after the twelfth, HOME again. The keys, KNOB 1–4, SELECT, PRESET, ALGORITHM, PLAY, REC and the
layers keep working; a page button leaves; the screen stays on while one shows.

| | | |
| --- | --- | --- |
| **SCOPE** — the output on a phosphor screen: a white-hot trace in its glow, a graticule, the last picture as afterimage | **SPECTRUM** — 80 bands 43 Hz–10.5 kHz in heat colours, held peaks, a reflection | **WATERFALL** — the last 60 spectra, newest on top, blended between bands and rows |
| **ORBIT** — the output against itself a quarter period later on an instrument face; a tone is a circle | **TUNNEL** — a twisting square a beat, the bar's one lit | **PULSE** — rings on the beat over turning spokes, the level as a sun at the core |
| **STARS** — speed from the tempo, a rush after each beat | **GRID** — the four tracks' steps round the playheads | **RAIN** — the notes sounding, falling, C2–B6 across |
| **WHEEL** — the Camelot wheel: the key, its neighbours, the roots sounding | **BULBS** — the act as lit bulbs, their halos on the beat; the run's phase and beats, the bar | **CLOCK** — the tempo in lamp segments, bar.beat, the beat filling |

Drawn from one snapshot of the music per picture, in the palette's colours (GREY and MONO stay gray). Every
edge is anti-aliased: lines, rings and discs by their coverage, positions to a sixteenth of a pixel where a
shape moves. HOME's own oscilloscope draws its trace the same way.

### 2.18 Faster hands
- **SELECT ALL** (MENU > CONTROL > SELECT, after Felucca's BPM PAGE MODW VEL): the SELECT knob turns through every
  page, HOME at either end; GLO + SELECT sets the tempo.
- **SONG quick entry** (SEQ held, or SEQ → SONG): the white keys **F3 G3 A3 B3** append sections A B C D —
  tap A B B C for A, B ×2, C. While a song plays they **cue** instead (2.21).

### 2.19 The voice bank and the palette
FM6 patches **OGENE IRON, OJA FLUTE, HILIFE GTR, TALK DRUM, LOG DRUM, EDDA EP** (presets OGENE, OJA, HILIFE,
TALK DRUM, LOG DRUM, EDDA EP; slots E1–E6), editable and exportable as `.syx` in the editor. The **EDDA** palette (MENU > DISPLAY >
COLOR): deep indigo, golden orange, electric blue; it passes Felucca's contrast lint.

### 2.20 What is kept with the settings
SHOW CUES, SEQ OUT, RUN, REVEAL and SELECT ALL survive power-off (with Felucca's other MENU settings).
KEY lives on in the project (ROOT and SCALE); ACT starts from 1.

### 2.21 THE ARRIVAL — the album's thirteen songs (SAVE > ARRIVAL)
The first thirteen songs of *EDDA: The Arrival* live in the firmware as music the FM-1 plays: every song's
instrumental, arranged start to finish, in its tempo and key, on four tracks. The voice is yours: each song
leaves the room for the Igbo line it opens on.

| # | Song | BPM | Key | Opens on | T1 | T2 | T3 | Length |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 01 | UP NEPA | 116 | 8A | Any moment now. (IHE) | log drum | highlife guitar stabs | tenor sax | 3:26 |
| 02 | WE OUTSIDE | 115 | 6A | One more. (ABALI) | log drum | Rhodes | string drone | 3:28 |
| 03 | OYA COME | 113 | 7A | Oya, come. (UZO) | log drum | grand piano | flute grains | 3:32 |
| 04 | WHERE YOU SLEEP | 112 | 1A | The night still young. (NZUZO) | log drum | organ | flute (the oja) | 3:34 |
| 05 | E GO BE | 116 | 5A | E go be. (NDIDI) | log drum | grand piano | highlife guitar riff | 3:31 |
| 06 | I DEY WAKA | 139 half | 3A | the flute, then Nwayo… nwayo… (NWAYO) | 808 | flute grains | flute (the oja) | 3:06 |
| 07 | BODY KNOW | 112 | 6A | Stop thinking. (EGWU) | log drum | organ stabs | tenor sax | 3:25 |
| 08 | DO AM AGAIN | 125 | 10B | Do am again. (MEE YA OZO) | talking log drum | grand piano | filtered guitar loop | 3:12 |
| 09 | YOU DEY WHINE ME | 124 | 9A | Just ask. (ANYA) | log drum | Rhodes | a sung lead | 3:21 |
| 10 | TURN AM UP | 126 | 12B | Turn am up. (ELU) | log drum | grand piano | surf-picked guitar | 3:18 |
| 11 | ENJOYMENT ONLY | 114 | 5A | Not in this house. (OFUMA) | log drum | grand piano | guitar + ogene loop | 3:30 |
| 12 | RING ME | 116 | 7A | Ring am first. (OGENE) | log drum | the ogene | a choir hum | 3:26 |
| 13 | JAPA AND COME BACK | 115 | 1A | I go come back. (IJE) | log drum | Rhodes | tenor sax | 3:28 |

T4 is THE ARRIVAL's kit in every song. Each song has its sections (INTRO, GROOVE, the RUN, the DROP; DO AM
AGAIN and TURN AM UP: BREAK, BUILD, DROP) and its arrangement as the SONG page's rows.

**Playing one.** SAVE, turn to **ARRIVAL**: KNOB 1 (or PRESETS) picks the song, **OCT+** loads it, **PLAY**
plays it from its first row (on another song than the one loaded, PLAY loads it first); PLAY again stops. A
load stops the transport and sets the four sounds, section A's steps, the rows, the tempo, the delay and the
reverb, and KEY to the song's Camelot code; the top bar says LOADED and then the line the song opens on. The
SONG page names each row's section (the row playing counts its passes down).

**Cueing live.** On the SONG page while a song plays, **F3 G3 A3 B3** are its sections A–D and no longer sound:
the key of the section playing gives it one more pass (as long as the room wants it); another section's key
cues it — NEXT shows on its row, and when the pass playing ends the song goes on from that section's next row
(from a GROOVE straight to the DROP); the same key again drops the cue. A cue on the song's last pass plays on
instead of ending. Every song, and any SONG chain of projects, cues the same way. With SHOW CUES on, each section
sends CC 26 and the note A2 as it starts and a load sends the song (CC 27) and its key (CC 24): lights can follow
the drops.

The tracks hold section A: loop it,
play over it, edit it, save it as a project (the project keeps the song: its rows play the song's sections with
your sounds). **TOOLS > CLEAR SONG** hands the SONG page back to the project slots.

**The rules kept.** Everything on the grid (no swing); the kick and the log drum forward, the high percussion
low; no rolls: a transition is a subtraction (the RUN: the shakers go, the log drum runs alone, a beat of
silence, the drop); four on the floor only in the drops of DO AM AGAIN and TURN AM UP.

**The sounds.** THE ARRIVAL's kit is DRUM KIT **EDDA**, 16 pads: real recordings from the Versilian Community
Sample Library (CC0) — a shaker, a cabasa, claps, a cross-stick, a snare, congas, the agogo as the ogene's two
bells, a log-drum hit as the ekwe, claves — and a designed kick, udu and glass bottle. The log drum is SAMPLE
SET **LOG** (two designed zones a fifth apart, a pitch strike on the attack, its tail left to ring), played
LEGATO so it slides; SET **EKIT** is the kit as a sample set. The Rhodes is FM6 **EDDA EP** (patch E6, TINE
EP with a short release) through the track's drive. `assets/samples-edda/ATTRIBUTION.txt` names every source;
`tools/edda_sounds.py` makes them again.

**The mix.** Rendered on the host through the firmware's own code (the sequencer, the engines, the effects and
the master, block by block) and measured with ITU-R BS.1770's K-weighting: every track sits within about 1.5 LU
of its role's place in the drop (the log drum and the kit forward, the keys under the lead, the pads under the
keys), the drops about 15 LU under full scale at MASTER full, the peaks at the master limiter's threshold
(working on at most a quarter of the time, never more than 7 dB). WE OUTSIDE and OYA COME are the darkest:
their 1–4 kHz is left to the voice. The shared budget of 8 voices never cuts a held note in any song: a
part of notes now gives up its own release tail first, a kit's hits go before a held note, and a kit's lowest
hit — the kick — rings out as a part's bass does.

**Their tempo is the deck's.** With the true tempo (2.22) a song at 116 plays at 116.00: set the deck's tempo to the
song's and they stay together; GLO + G#4 / A#4 (2.23) pull them into phase.

**Changing them.** The music is `tools/arrival_songs.py`, a score in Python (notes by name, drum rows as
strings); `tools/gen_arrival.py` encodes it into 12 KB of `build/gen/edda_arrival.h` at build time.
`build/host/arrival_render build/arrival` renders all thirteen to WAV (`[SONG] [SECONDS] [STEMS]`), and
`tests/arrival_check.py` reports the balance per section and track and the LEVELs that would put each track in
its place.

### 2.22 The true tempo
Every Felucca counts a beat as 44,100 × 60 / BPM samples, but the FM-1's audio runs at 24 MHz / 544 =
**44,117.647 samples a second**, so its tempo plays 0.04 % fast: 112 on the screen is 112.045, about 24 ms a minute
against a record at 112, a sixteenth apart in under six minutes. EDDA OS counts the real rate on its internal clock:
a beat is 45,000,000 / (17 × BPM) samples (exactly a minute of output for 112 beats at 112), every clock on the exact
grid with it (2.14) and MIDI clock out (2.15). The pitch stays as it was (0.7 cent sharp: nobody hears it). On an
external clock the FM-1 follows the clock, as before.

### 2.23 TEMPO BEND (GLO + G#4 / A#4)
A deck's pitch bend for the whole set: **A#4 held** plays every clock (the tracks, the ARP, the click, EDDA's bars,
the SONG, MIDI clock out, the SLICER, the FX layer) at the tempo + (BPM + 12) / 25 (about 4 %: 125 at 120), **G#4
held** the same down; let go, the tempo as set, the set's phase moved: a second of it at 120 BPM is about 40 ms. Both
held: none. The keys stay lit while held, also after GLO is let go. Each clock takes the bend from its next step, so
the gates, the delay and the LFOs keep the tempo as set. On an external clock: BEND: CLK IS EXT, nothing.

---

## 3. Controls at a glance

**GLO layer** (hold GLO): F#3 G#3 A#3 C#4 mute tracks 1–4 · F3–B3 solo while held · C4 all unmuted ·
**D4 RUN · E4 STOP · F4 TAP · G4 ACT+ · A4 FILL (tap: to the next bar's end; hold: while held) · B4 MUTATE ·
C5 / D5 / E5 KEY −1 / +1 / relative · F5 MIDI LEARN · G#4 / A#4 TEMPO BEND − / + (held)** · KNOB 1–4 the levels ·
SELECT the tempo · GLO + PLAY from the top.

**MENU > EDDA** (hold HOME, ALGORITHM to the EDDA tab): KEY · SHOW CUES · SEQ OUT · ACT · RUN · REVEAL.
**MENU > CONTROL**: SELECT (BPM / PAGE / MODW / VEL / ALL).

**New pages and knobs**: SAVE > ARRIVAL (KNOB 1 the song, OCT+ LOAD, PLAY) · FX > DJ FILTER · DRUM EDIT 1 KIT
USR1–4, EDDA · SAMPLE SET 8 (USR4), 9 EKIT, 10 LOG · SCL > CHORD VOIC LEAD · FM6 SLOT E1–E6.

**Gestures**: black keys with CHRD + QNT WHITE = CHORD+ · F3–B3 on SONG = sections A–D (playing: cue the next one,
or one more pass of the one playing).

The web editor sees the new MENU rows, the 4th slot (its 72 KiB) and the kits.

---

## 4. SLOOP 2.4 (7 Oct 2026) feature by feature

| SLOOP 2.4 | EDDA OS |
| --- | --- |
| FM6 with DX7 patches from the web editor | **In** (Felucca's FM6; `.syx` import and export, 14 factory patches) |
| Own drum kits: 16 WAVs on pads | **In** (2.8: KIT USR1–4, the editor's 16 pads, the kit builder) |
| A 4th sample slot | **In** (2.9) |
| A web editor in the FM-1's look | Felucca's editor; EDDA's rows, slots and kits appear in it |
| CHORD+: black keys change the chord, strum, voice leading | **In** (2.10) |
| A filter on every track, one knob | **In** (2.11) |
| 12 full-screen visualisers | Built, not in the device's build (2.17: the app slot); in the browser emulator |
| Parameter locks | **In** (Felucca 1.1) |
| Micro timing | **In** (2.13, Felucca's NUDGE) |
| Fills with one finger | **In** (2.6) |
| Chains: tap A B B C | **In** (2.18, on the SONG page) |
| Patterns out over MIDI | **In** (2.15, with clock) |
| Drums programmed with the keys | **In** (Felucca's grid) |
| Steps up to two bars | **In** (64 steps) |
| Dotted echoes | **In** (2.12) |
| Every key lit in the dark | **In** (MENU > DISPLAY > LEDS) |
| SELECT turns the pages | **In** (2.18, an option) |
| Bigger values, the menu in sections | **In** (Felucca's LARGE and tabbed menu) |

Beyond SLOOP: THE ARRIVAL's thirteen songs with their own kit and log drum, the true tempo and TEMPO BEND, the Camelot
lock and path, the run, the hard stop, REVEAL, MUTATE, the show cues, the exact grid, SEQ OUT's run-aware output, the
West African pattern bank, the voice bank; and Felucca 1.7.0's step conditions, inserts, MIDI learn and second core.

---

## 5. On stage with the Omnis-Duo

1. FM-1 3.5 mm out → stereo-to-1/4" **mono** cable → Omnis-Duo **MIC 2**. Talkover OFF. FM-1 master low,
   bring it up on the mic knob.
2. Set **KEY** to the Camelot of the record playing, or walk it with C5 / D5 / E5 as you mix the one-step
   path. Set the FM-1 to the record's BPM (GLO + SELECT, or tap it on GLO + F4) and the deck to the same: with the
   true tempo they stay together. Start the FM-1 on the one, then hold **G#4 / A#4** (TEMPO BEND) until the kicks
   sit together. With the laptop, rekordbox's Link → a Link-to-MIDI-clock bridge → the FM-1's USB, CLOCK to it.
3. For a song of the album: SAVE > ARRIVAL, the song, OCT+, PLAY (its tempo and key come with it: set the
   decks to them, or sing over it alone). Otherwise track 4 DRUM on your own kit (KIT USR1, or EDDA) with
   3STEP or OGENE12; track 1 LOG DRUM with LOGDRUM; track 2 a pad or STABS; track 3 the lead (OJA, OGENE,
   HILIFE).
4. REVEAL on for long passages; MUTATE when the room needs a new figure; **D4** to leave a record by
   subtraction; **E4** for the hard stop and the re-entry.
5. USB to the stream machine: SEQ OUT +CLOCK drives a DAW or a second synth in time with the FM-1, the show
   cues on channel 16 drive the lights, and the same USB carries the FM-1's audio (44.1 kHz) for recording.

---

## 6. Building and testing from source

```
git submodule update --init         # firmware/bsp (felucca-bsp), firmware/engines (Felucca-Engines)
sh edda-build.sh --release 1.7.0-EDDA   # Linux x86-64 (or WSL2): the toolchain, the SDK files -> build/felucca-1.7.0-EDDA.fwsc
SANITIZE=1 sh tests/run_tests.sh    # every suite against build/felucca.fwsc; tests/run_host_only.sh without a package
```

The device's build (`firmware/src/felucca.c`): `EDDA_VIZ 0`, `FELUCCA_CDC 0`, `FELUCCA_UAC_48K 0`. What each costs in
the app slot (JieLi's linker, `tools/size_matrix.sh`): the visualisers 11.6 KB, the serial console 7.0 KB, USB audio's
48 kHz resampler 8.3 KB, the second core 2.0 KB, USB audio 11.1 KB. With the visualisers in, even without the console,
the second core and USB audio the image is 3.5 KB over. The host suites keep Felucca's nominal tempo
(`-DEDDA_TRUE_TEMPO=0`: their timings are Felucca's, bit for bit); `tests/edda_test.c` checks the true one.

Felucca's `BUILDING.md` covers macOS with Docker and the build options. `.github/workflows/edda-build.yml`
is Route A's build.

---

## 7. What is verified, and what is not

**Verified on the host** (gcc, Felucca's own harness with the real sequencer, UI, MIDI and audio code, under
ASan/UBSan): {{HOSTCHECKS}}. Felucca 1.7.0's suites run against EDDA OS as they are, except where EDDA changes what
they check (FILL on A4, MIDI LEARN on F5, the kits after PERC, FM6's EDDA presets before the bank, SELECT's ALL, the
DJ FILTER page, USR4's 72 KiB, the exact grid's step starts): those checks test EDDA's behaviour instead. They keep
Felucca's nominal tempo (`-DEDDA_TRUE_TEMPO=0`), so their timings stay Felucca's, bit for bit; Felucca's 104 golden
renders are unchanged. EDDA's own (`tests/edda_test.c`, `tests/ui_test.c`) build the device's true tempo and cover
it (a beat of 45,000,000 / (17 × BPM) samples: 112 beats at 112 BPM are exactly a minute of output), TEMPO BEND (the
keys, the LEDs, an external clock, a second of it at 120 BPM moving the set 40 ms ahead and the beats back to 22,059
samples apart after), the Camelot wheel and path, every bank pattern, Euclidean rhythms, the run phase by phase, the
hard stop and re-entry, REVEAL, FILL (tap and hold), MUTATE, the cues, the user kits on a 16-pad kit built by the real
builder (in USR1 and, sized for its 72 KiB, in USR4), CHORD+ and VOIC LEAD, the DJ filter's response, EDDA OS
1.1.5's projects migrating (nudges, fills, kits, dotted times, the song), the exact grid (ten minutes at 128 BPM,
two at 97 and 173, the ARP, MIDI clock out's pulse 6k in step k's block for ten minutes), SEQ OUT, the settings round
trip, the device's build without the visualisers, and THE ARRIVAL: titles, tempi, keys, sections, arrangements,
the load, SAVE > ARRIVAL, the live cues and their show cues; then every song is played through the firmware's audio
(3 to 4 minutes each, sounding, nothing clipped, no held note cut by the voice budget, the master limiter at most
8 dB). Every screen, in every palette, passes Felucca's layout lint. The web editor's protocol (the slots' own sizes
included), the kit builder, the backup and the one-file installer (onto a simulated FM-1: stock → update loader →
EDDA OS, every byte it asks for checked) pass.

**Built with JieLi's compiler** (GitHub Actions, {{RUN}}): {{BUILT}}. Register access in `hal/` and the BSP only. The
package carries JieLi's own AC79NN_SDK_V1.2.1 files, fetched from gitee and checked against `tools/build.py`'s
SHA-256s, so it is put together exactly as Felucca's are. {{CITESTS}}

**Not yet:** a flash onto an FM-1, and listening on its own outputs (the songs have been heard as host renders: the
same code, the same numbers, a computer's DAC). On the device, three checks are worth a minute: the splash says
`v1.7.0-edda`; SAVE > ARRIVAL, a song, OCT+, PLAY sounds; and the true tempo: at 120 BPM against a metronome app or a
DAW at 120.00, the FM-1 should hold the click for ten minutes (Felucca's count gains 0.24 s in that time, half a
beat). `tests/target_budget.py`, a static count of the audio code's loop instructions from JieLi's disassembly,
reports the functions EDDA OS works in beyond Felucca's figures ({{BUDGET}}); the host CPU check, which times the
renders, passes with nothing over budget. After a good test on the FM-1, `BUDGET_UPDATE=1 python3
tests/target_budget.py build/felucca.dis tests/target_budget.txt` re-bases the static figures.

---

## 8. Frontier

Next, in the order they pay on stage (the research behind them: `docs/fm1-landscape-2026-10.md`):

- **Tempo to a hundredth, and a tap that sets the phase.** The tempo in 0.01 BPM steps (shown to two decimals) for
  records off the whole number, and taps that also fix where the one is, so PLAY lands on the record's next downbeat.
- **Mix-in / mix-out macros**: the run's subtraction as a timed handover, phrase-aligned (8 / 16 / 32 bars).
- **Log-drum tools**: the slide and the "tsk" as per-step locks, the log drum's pitch envelope on a knob.
- **The visualisers back** on the device, once 5 KB more of the app slot is found.
- **Ableton Link over the AC791N's Wi-Fi** (the chip has a radio Felucca does not use), BLE MIDI (~45 KB: not beside
  everything else), the chant lane and the set file as before.

---

## 9. Where the FM-1 scene stands (30 October 2026)

Felucca shipped seven releases between 9 and 28 October (1.4 … 1.7.0: the second core, step conditions, inserts,
MIDI learn, felucca-bsp); about a hundred community firmwares now compete on dual cores, eight tracks and live scenes;
M-VAVE answered with stock V16 and an open branch, FM1 Open. None of the firmwares or the grooveboxes reviewed ships
deck-grade tempo and phase tools, mix-in and mix-out macros timed to the phrase, or a log-drum engine: that is where
EDDA OS leads. The whole survey, with its sources: `docs/fm1-landscape-2026-10.md`.

Felucca © 2026 Leo Kuroshita, Hügelton Instruments, GPL-3.0-only. EDDA OS's additions are released under the
same licence.
