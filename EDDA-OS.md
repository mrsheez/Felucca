# EDDA OS — the Mr. Sheez performance firmware for the M-VAVE FM-1

EDDA OS is a fork of **Felucca 1.1.5.1** (Hügelton Instruments, GPL-3.0) built for the Mr. Sheez live set:
the thirteen songs of *EDDA: The Arrival* ready to play (section 2.21), the key of the record on the decks,
the EDDA rhythm language, the transition by subtraction on one key, a hard stop that re-enters on the one,
16-WAV drum kits, the sequencer out over MIDI with clock, and twelve full-screen visualisers. Everything Felucca does is still there and behaves as before unless a
section below says otherwise.

| | |
| --- | --- |
| Version | 1.1.5-EDDA (the splash says `v1.1.5-edda`) |
| Base | Felucca 1.1.5.1 (hugelton/Felucca `main`, merged); EDDA OS's commits sit on top (`git log main..edda`) |
| Licence | GPL-3.0-only, as Felucca |
| Verified | 3,517 host checks pass (Felucca's suites and `tests/edda_test.c`, under ASan/UBSan); the whole firmware compiles for a 32-bit target |
| Not yet | flashed onto an FM-1 (section 7); the package builds with JieLi's compiler on GitHub and its suites pass there |
| Install | from a Windows laptop with Chrome or Edge; no Mac or Linux needed (section 1) |

---

## 1. Install it without a Mac or a Linux computer

The FM-1's firmware is compiled with JieLi's toolchain, which runs only on Linux x86-64. You do not need
that computer: GitHub runs the build on its own Linux machines and hosts the installer page. What you
need is the Windows laptop, Chrome or Edge, the FM-1's USB cable, and a free GitHub account.

**Not from the iPhone.** Installing and the editor both talk to the FM-1 over Web MIDI with SysEx. No
iPhone browser has Web MIDI (Safari never shipped it, and Chrome on iOS uses Safari's engine), so the
iPhone can read this file and watch the build, but the install and the kit uploads happen on the laptop.

### 1.1 Route A — GitHub builds it and hosts the installer (recommended)

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
   package, runs the test suites against it, and publishes the site: about 10 to 15 minutes. Its summary
   page lists the package, its size against the FM-1's app slot, its SHA-256, and any check that did not pass.
7. On the laptop, in Chrome or Edge, open `https://<your-user>.github.io/Felucca/webapp/installer/`, connect
   the FM-1 by USB, allow MIDI when asked, and press **Install**. The editor is at `…/webapp/editor/`.

Every later push to `edda` rebuilds and republishes. The package is also kept with each run under
**Artifacts → edda-os-fwsc** for the command-line installer (1.3).

If the whole image does not fit the FM-1's app slot (the build checks this), the run ships the same
build without the visualisers (`EDDA_VIZ=0`) and says so in its summary (section 7).

### 1.2 Route B — build on the Windows laptop (WSL2)

WSL2 runs Ubuntu inside Windows 10/11, and JieLi's toolchain runs there natively.

1. PowerShell as administrator: `wsl --install -d Ubuntu`, then restart and finish the Ubuntu setup.
2. In Ubuntu:

   ```
   sudo apt update && sudo apt install -y git python3-pil python3-fonttools python3-numpy curl xz-utils
   git clone -b edda https://github.com/mrsheez/Felucca edda-os   # (or from edda-os.bundle / edda-os-fork.zip)
   cd edda-os && sh edda-build.sh                     # -> build/felucca.fwsc
   python3 web/make_site.py build/felucca.fwsc 1.1.5-EDDA /tmp/edda-site
   cd /tmp/edda-site && python3 -m http.server 8000
   ```

   (With the bundle instead: copy it into Ubuntu first, e.g. `cp /mnt/c/Users/<you>/Downloads/edda-os.bundle ~`,
   and `git clone ./edda-os.bundle edda-os -b edda`.)
3. In Windows Chrome or Edge open `http://localhost:8000/webapp/installer/` (Web MIDI is allowed on
   localhost) and install as in Route A.

### 1.3 The command-line installer on Windows

For a package from Route A's artifact or Route B: install Python 3 from python.org, then in PowerShell:

```
py -m pip install mido python-rtmidi
py tools\fm1_install.py felucca-1.1.5-EDDA.fwsc
```

Close every other app that uses MIDI first (DAWs, the editor tab).

### 1.4 Going back

- **To Felucca:** install it from `hugelton.github.io/Felucca`. EDDA OS keeps Felucca's update loader (it
  answers as `FM-1_900`), its release identity (`FM-1_911` for 1.1.x) and its flash layout, so either installs
  over the other. Projects, presets and user
  samples stay (EDDA's 4th sample slot sits in flash Felucca leaves unused); Felucca opens EDDA projects
  without their nudges and fill flags, and settings Felucca does not know are kept as they are.
- **To M-VAVE's firmware:** the installer's **Return to official V15** (with M-VAVE's `FM-1.fwsc`).
- **If the FM-1 does not start:** see Felucca's README, *If the FM-1 does not start* (another cable, close
  MIDI apps, run the installer again; the recovery tool there needs a Mac and a Seeed XIAO RP2040).

Installing firmware is at your own risk.

### 1.5 Your fork (github.com/mrsheez/Felucca)

Steps 1–4 of Route A are done: the fork exists, its workflows run, `edda` is pushed and `main` carries the
same commits (a fast-forward, so Pages publishes from the default branch without changing it). Every push
builds the package with JieLi's compiler, runs the suites and pushes the run's logs and package to the
`ci-out` branch. One setting is left, because a workflow's own token may not turn Pages on: **Settings →
Pages → Build and deployment → Source: GitHub Actions**. Then **Actions → EDDA OS → Run workflow** (branch
`main`), and the installer is at `https://mrsheez.github.io/Felucca/webapp/installer/`.

Without Pages, the latest run's package is at
`https://github.com/mrsheez/Felucca/raw/ci-out/felucca-1.1.5-EDDA.fwsc` (its SHA-256 in the run's summary). Install
it with the command-line installer (1.3), or serve the installer page from the laptop: with Python 3 on Windows,
download the repository (**Code → Download ZIP**, branch `edda`), put the package in its folder, and run there

```
py web\make_site.py felucca-1.1.5-EDDA.fwsc 1.1.5-EDDA site
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
Steps flagged fill-only rest until FILL is on; **A4** plays them through the end of the next bar (again:
off early). The bank's drum grids carry fills on their last steps. Cue CC 25.

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

Pads 1–8 are the grid's lanes. A note with no pad of its own plays its lane's pad, pitched to it, so every
DRUM pattern plays on every kit, including one with 8 pads. TUNE ±12 semitones; TONE below 64 darkens;
DECY below 64 shortens (gone after 25 ms at 0, 0.8 s at 32, the whole sample at 64 and up); SNAP above 64
skips into the attack (tighter), below 64 fades it in (softer), up to 50 ms; ACC, DRV and the lane LEVELs
as the synth kit. A pad plays one voice at a time; the closed hat chokes the open one.

**Making one in the editor** (laptop, Chrome or Edge): Samples → a slot → tick **16-pad kit** → drop a WAV
on each pad (or click it) → name it → **Upload**. **From a terminal:**
`py tools\fm1_sample_upload.py kit 2 MYKIT kick.wav snare.wav ... ride_bell.wav` (`-` leaves a pad empty;
`kitbuild` writes the files without a device). Any WAV works: 8–32 bit, float, any rate, any channels. Each
pad is trimmed of trailing silence and holds up to 4 s; a slot holds 80 KiB, and when the kit does not fit
the longest pads drop to 11025 Hz until it does.

### 2.9 A fourth sample slot (USR4)
SAMPLE SET 8, GRAIN SRC 8, SLICE SRC USR4, KIT USR4, the editor's 4th slot, and the backup. It sits in flash
Felucca does not use (0xE7000–0xFB000), so going back to Felucca leaves the other three slots as they are.

### 2.10 CHORD+ and VOIC LEAD (SCL > CHORD)
With CHRD on and QNT WHITE, the selected track's black keys change the chord while held:

| Key | F#3 | G#3 | A#3 | C#4 | D#4 | F#4 | G#4 | A#4 | C#5 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Change | MIN | 7TH | MAJ7 | SUS4 | 9TH | INV | +BASS | STRUM | OPEN |

They combine (MIN + 7TH = m7; SUS4 + 7TH = 7sus4) and the CHORD page names the result. STRUM plays the
notes 18 ms apart, low to high. **VOIC LEAD** picks, from each chord's inversions, the one that moves least
from the last chord: a keyboard player's voice leading.

### 2.11 The track filter (FX > FILTER)
One knob per track, as a DJ mixer's filter: left of centre a low-pass from 16.5 kHz down to 100 Hz, right a
high-pass from 22 Hz up to 7.8 kHz, 12 dB/octave, off at 0 (bit for bit). After the SLICER, before the level
and the sends. Automatable and lockable like any knob.

### 2.12 Dotted echoes (FX > DLY > TIME)
**1/4D, 1/8D, 1/16D** after the straight and triplet times.

### 2.13 Micro timing (SEQ > CHANCE > KNOB 4 NUDGE)
Each step can sit up to three sixteenths of its own length early or late (−3…+3); the grid itself never
moves, so nudges never accumulate. Nothing plays before PLAY: an early first step starts at once, as far into
the step as it was early. Nudges hold through song chains and are saved in projects.

### 2.14 The exact grid
Felucca rounded each step's length on its own, so a 1/16 track and an 8T track drifted apart (54 ms after
ten minutes at 120 BPM) and the ARP lost its remainder (93 ms late after 16 bars at 128 BPM). EDDA OS
counts every step, the click, the beat clock, the SLICER, the FX layer and the ARP on one fractional grid:
all of them meet on every beat, for as long as the set runs. Checked for ten minutes at 128 BPM.

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

### 2.17 VIZ — twelve full-screen visualisers (tap HOME on HOME)
Tap **HOME** on the HOME screen; tap again for the next one, its name at the foot for a moment; after the
twelfth, HOME again. The keys, KNOB 1–4 (their value at the foot), SELECT, PRESET, ALGORITHM, PLAY, REC and
the layers keep working; a page button leaves; the screen stays on while one shows.

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
- **SELECT PAGES** (MENU > CONTROL > SELECT): the SELECT knob turns through every page, HOME at either end;
  GLO + SELECT sets the tempo.
- **SONG quick entry** (SEQ held, or SEQ → SONG): the white keys **F3 G3 A3 B3** append sections A B C D —
  tap A B B C for A, B ×2, C. While a song plays they **cue** instead (2.21).

### 2.19 The voice bank and the palette
FM6 patches **OGENE IRON, OJA FLUTE, HILIFE GTR, TALK DRUM, LOG DRUM, EDDA EP** (presets OGENE, OJA, HILIFE,
TALK DRUM, LOG DRUM, EDDA EP; slots E1–E6), editable and exportable as `.syx` in the editor. The **EDDA** palette (MENU > DISPLAY >
COLOR): deep indigo, golden orange, electric blue; it passes Felucca's contrast lint.

### 2.20 What is kept with the settings
SHOW CUES, SEQ OUT, RUN, REVEAL and SELECT PAGES survive power-off (with Felucca's other MENU settings).
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

**Changing them.** The music is `tools/arrival_songs.py`, a score in Python (notes by name, drum rows as
strings); `tools/gen_arrival.py` encodes it into 12 KB of `build/gen/edda_arrival.h` at build time.
`build/host/arrival_render build/arrival` renders all thirteen to WAV (`[SONG] [SECONDS] [STEMS]`), and
`tests/arrival_check.py` reports the balance per section and track and the LEVELs that would put each track in
its place.

---

## 3. Controls at a glance

**GLO layer** (hold GLO): F#3 G#3 A#3 C#4 mute tracks 1–4 · F3–B3 solo while held · C4 all unmuted ·
**D4 RUN · E4 STOP · F4 TAP · G4 ACT+ · A4 FILL · B4 MUTATE · C5 / D5 / E5 KEY −1 / +1 / relative** ·
SELECT the tempo.

**MENU > EDDA** (hold HOME, ALGORITHM to the EDDA tab): KEY · SHOW CUES · SEQ OUT · ACT · RUN · REVEAL.
**MENU > CONTROL**: SELECT (TEMPO / PAGES).

**New pages and knobs**: SAVE > ARRIVAL (KNOB 1 the song, OCT+ LOAD, PLAY) · FX > FILTER (FILT) · FX > DLY >
TIME 1/4D 1/8D 1/16D · SEQ > CHANCE KNOB 4 NUDGE · DRUM EDIT 1 KIT USR1–4, EDDA · SAMPLE SET 8 (USR4), 9 EKIT,
10 LOG · SCL > CHORD VOIC LEAD.

**Gestures**: HOME on HOME = VIZ (again: next) · black keys with CHRD + QNT WHITE = CHORD+ · F3–B3 on SONG =
sections A–D (playing: cue the next one, or one more pass of the one playing).

The web editor sees the new MENU rows (protocol ids 18–24), the 4th slot and the kits.

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
| 12 full-screen visualisers | **In** (2.17) |
| Parameter locks | **In** (Felucca 1.1) |
| Micro timing | **In** (2.13) |
| Fills with one finger | **In** (2.6) |
| Chains: tap A B B C | **In** (2.18, on the SONG page) |
| Patterns out over MIDI | **In** (2.15, with clock) |
| Drums programmed with the keys | **In** (Felucca's grid) |
| Steps up to two bars | **In** (64 steps) |
| Dotted echoes | **In** (2.12) |
| Every key lit in the dark | **In** (MENU > DISPLAY > LEDS) |
| SELECT turns the pages | **In** (2.18, an option) |
| Bigger values, the menu in sections | **In** (Felucca's LARGE and tabbed menu) |

Beyond SLOOP: THE ARRIVAL's thirteen songs with their own kit and log drum, the Camelot lock and path, the run,
the hard stop, REVEAL, MUTATE, the show cues, the exact grid, SEQ OUT's run-aware output, the West African
pattern bank, the voice bank.

---

## 5. On stage with the Omnis-Duo

1. FM-1 3.5 mm out → stereo-to-1/4" **mono** cable → Omnis-Duo **MIC 2**. Talkover OFF. FM-1 master low,
   bring it up on the mic knob.
2. Set **KEY** to the Camelot of the record playing, or walk it with C5 / D5 / E5 as you mix the one-step
   path. Tap the tempo on GLO + F4, or send MIDI clock from the laptop and set CLOCK to it.
3. For a song of the album: SAVE > ARRIVAL, the song, OCT+, PLAY (its tempo and key come with it: set the
   decks to them, or sing over it alone). Otherwise track 4 DRUM on your own kit (KIT USR1, or EDDA) with
   3STEP or OGENE12; track 1 LOG DRUM with LOGDRUM; track 2 a pad or STABS; track 3 the lead (OJA, OGENE,
   HILIFE).
4. REVEAL on for long passages; MUTATE when the room needs a new figure; **D4** to leave a record by
   subtraction; **E4** for the hard stop and the re-entry.
5. USB to the stream machine: SEQ OUT +CLOCK drives a DAW or a second synth in time with the FM-1, the show
   cues on channel 16 drive the lights, and the same USB carries Felucca's 44.1/48 kHz audio for recording.
6. Between records, tap HOME on HOME: CLOCK or BULBS for you, PULSE or TUNNEL for the front row.

---

## 6. Building and testing from source

```
sh edda-build.sh                   # Linux x86-64 (or WSL2): the toolchain, the vendored SDK files -> build/felucca.fwsc
SANITIZE=1 sh tests/run_tests.sh   # every suite; tests/run_host_only.sh without a package
EDDA_VIZ=0 ./build.sh              # the same firmware without the visualisers (~17 KB less code)
```

Felucca's `BUILDING.md` covers macOS with Docker and the build options. `.github/workflows/edda-build.yml`
is Route A's build.

---

## 7. What is verified, and what is not

**Verified on the host** (gcc and clang, Felucca's own harness with the real sequencer, UI and MIDI code,
under ASan/UBSan): 3,517 checks pass. EDDA's own (`tests/edda_test.c`, `tests/ui_test.c`) cover the Camelot
wheel and path, every bank pattern, Euclidean rhythms, the run phase by phase at 120 BPM, the hard stop and
re-entry, REVEAL, FILL, MUTATE, the cues, the user kits on a 16-pad kit built by the real builder (pads, the
GM fall-back, TUNE / DECY / SNAP, the choke, levels, an empty kit, USR4), CHORD+ and VOIC LEAD, the filter's
response, dotted echo lengths, micro timing at the pattern's start and in chains, the exact grid (ten minutes
at 128 BPM, two at 97 and 173, the ARP), SEQ OUT (pairing through chords, ties, slides, ratchets, lanes,
mutes, a full queue, OFF; the clock's pulse 6k in step k's block for ten minutes; START / STOP; none on an
external clock), the settings round trip, VIZ navigation and the spectrum's bands, SELECT PAGES and the SONG
keys. Every screen, in every palette, passes Felucca's layout lint (0 findings over 163 screens × 11 palettes).
The web editor's protocol and kit builder tests pass; the browser emulator's source builds and runs natively.

**Built with JieLi's compiler** (GitHub Actions, runs 37803676056 and 37805034320, every visualiser in): the
image is 472,580 bytes of the 581,564-byte app slot, RAM (.data + .bss) 93,688 of 98,304 bytes, the pool
334,788 of 344,064 bytes (9,276 spare; the build requires 8,192), register access in `hal/` only. The package
carries JieLi's own AC79NN_SDK_V1.2.1 files, fetched from gitee and checked against `tools/build.py`'s SHA-256s,
so it is put together exactly as Felucca's are. On the same runner the whole suite passes against the real
package, sanitizers and fuzzers included, and the browser emulator builds to WebAssembly and plays (2.1 % of real
time for a heavy song), except the static cost check below.

**Not yet:** a flash onto an FM-1 and listening tests of the voices and kits. `tests/target_budget.py`, a static
count of the instructions in the audio code's loops from JieLi's disassembly, reports four functions over
Felucca's figures, all EDDA's intended work: `drum_render` (the user kits' sample voice now inside it; its
per-sample loop has no divides), the audio interrupt `fm1_alnk0_irq` (+19 %: SEQ OUT, the run, the cues),
`slicer_track` (+59 %) and `perf_begin` (+29 %: both the exact grid's clocks). The host CPU check, which times
the renders, passes with nothing over budget. After a good test on the FM-1, `BUDGET_UPDATE=1 python3
tests/target_budget.py build/felucca.dis tests/target_budget.txt` re-bases the static figures.

---

## 8. Frontier

- **Ableton Link over the AC7911's Wi-Fi.** The chip has a radio Felucca does not use yet.
- **The chant lane**: the room's "Yaa" in a user slot, a key as the call.
- **YAA counter in**: the stream kit's count as MIDI CC into MUTATE's density.
- **Set file**: the crate's keys and tempos as a Camelot path for C5 / D5.

---

## 9. Where the FM-1 scene stood (7 October 2026)

| Firmware | Author | What it is | Licence |
| --- | --- | --- | --- |
| FM-1+VA | Baud Girl | Menu rebuild, fixes to the stock FM engine, a supersaw VA engine | free, open source promised |
| Felucca | Hügelton / Leo Kuroshita | 13 engines, 4 tracks, 64 steps, chords, mod matrix, FX layer, web installer and editor, browser build, host tests | GPL-3.0 |
| Groove OS | Peter Gombos | 8-track groovebox | paid |
| SLOOP | 3dSam | 4-track groovebox on Felucca's engines | free, open source |
| X0X / OMNI | Charles Vestal | ReBirth-style groovebox / Omnichord-style instrument | open source |

Sources: github.com/hugelton/Felucca; synthanatomy.com (Sep 28–Oct 6 2026); sonicstate.com 29 Sep 2026;
pianoandsynth.com 28 Sep 2026; matrixsynth.com 4 Oct 2026.

Felucca © 2026 Leo Kuroshita, Hügelton Instruments, GPL-3.0-only. EDDA OS's additions are released under the
same licence.
