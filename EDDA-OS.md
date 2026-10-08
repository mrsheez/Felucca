# EDDA OS — the Mr. Sheez performance firmware for the M-VAVE FM-1

A fork of **Felucca 1.0.5.2** (Hügelton Instruments, GPL-3.0) that turns the FM-1 into the
Envoy's stage instrument: the key of the record on the decks, the EDDA rhythm language,
the transition-by-subtraction as one key, a hard stop that re-enters on the one, an
arrangement engine built on the entrainment/novelty rule, show cues for the visuals rig,
and the "future version" voice bank (ogene, oja, highlife guitar, talking drum, log drum).

Everything in this fork is covered by host tests that run Felucca's real sequencer, UI and
MIDI code (`tests/edda_test.c`, 40 checks) and the whole upstream suite still passes.
What has **not** happened yet is a flash onto a real FM-1: the JieLi toolchain could not be
fetched in the environment this was written in. The build-and-install steps are at the end
and take about an hour on a Mac or Linux box with Docker.

---

## 1. Where the FM-1 custom-firmware scene stands (7 October 2026)

The FM-1 runs a JieLi AC7911B8. Its firmware updates over USB through M-VAVE's M-UPGRADE
app, which Baud Girl reverse-engineered; the first 16 KB of flash is the boot code and every
installer refuses to touch it. That opened the floodgates in a fortnight:

| Firmware | Author | What it is | Licence |
| --- | --- | --- | --- |
| **FM-1+VA** (28 Sep) | Baud Girl | Performance-first menu rebuild, fixes to the stock FM engine, a 303-style supersaw VA engine, a better sequencer | free, open source promised |
| **Felucca 1.0.5.2** (updated daily) | Hügelton / Leo Kuroshita | 13 engines (Dexed 6-op FM, VA, phase distortion, chip, samples, formant, 3-osc, tonewheel, granular, physical models, noise, slicer, synthesised drums), 4 tracks, 64-step sequencer with chords / chance / ratchets, chord keys, 16 scales, mod matrix, FX layer, web installer + editor, a browser build, a full host test suite | GPL-3.0 |
| **Groove OS** (4 Oct) | Peter Gombos | 8-track groovebox, four engines, try-in-browser | paid |
| **SLOOP** (5 Oct) | 3dSam | 4-track groovebox on Felucca's engines | free, open source |
| **X0X** (5 Oct) | Charles Vestal | ReBirth-style groovebox | open source |
| **OMNI** (6 Oct) | Charles Vestal | Omnichord-style instrument | open source |

Nearly all of it is AI-coded. Felucca is the platform of record: documented HAL, build,
tests, and a web installer anyone can host. EDDA OS is built on it and keeps its licence.

Sources: github.com/hugelton/Felucca; synthanatomy.com (Baud Girl, Felucca 1.0, Groove OS,
SLOOP, X0X, OMNI articles, Sep 28–Oct 6 2026); sonicstate.com 29 Sep 2026;
pianoandsynth.com 28 Sep 2026 (the AC7911B8 and the updater); matrixsynth.com 4 Oct 2026.

---

## 2. What EDDA OS adds

### 2.1 KEY — the Camelot lock (MENU > EDDA > KEY)
Set the key of the record on the Omnis-Duo as its Camelot code, **1A–12B**. Every synth
track gets that root and scale (A = minor, B = major), and any track whose key quantiser was
OFF is switched to SNAP. Nothing you play under the mask can land out of key. The kit track
is never touched. 8A (A minor) is the EDDA home key.

In the GLO layer the top keys walk the Camelot path of the set without the menu:
**C5** one step down the wheel (a fifth down), **D5** one step up, **E5** across to the
relative key. With no KEY set, the path starts from the selected track's own key.
The key goes out as show cue CC 24.

### 2.2 The EDDA pattern bank (SEQ > PATTERNS, E01–E16)
Listed after Felucca's thirteen factory patterns (E01–E17; patterns run to 32 steps). Melodic lines are written in C and land in
the track's **root** when loaded (the chord stabs build their triads from the track's scale).
Drum grids use the DRUM lanes; velocities carry the "human levels" rule (ghosts at 60).

| # | Name | Kind | Steps / div | What it is |
| --- | --- | --- | --- | --- |
| E01 | 3STEP | drums | 16 / 1/16 | The family identity: kick on 1, the and of 2, 4; dry rims on 2 and 4; straight hats accented on the beats |
| E02 | SGIJA | drums | 16 / 1/16 | Pretoria bacardi bounce: syncopated kick, claps answering, off-beat hats |
| E03 | GQOM | drums | 16 / 1/16 | Broken kick, toms as the melody, no hats |
| E04 | KIZOMBA | drums | 16 / 1/16 | Kick with the push on the and of 2, rim clicks, soft hats (the 92–94 BPM lane) |
| E05 | AFROBT | drums | 16 / 1/16 | The Africa 70 feel: sixteenth hats, open hat on the and of 2 and 4, kick on 1 and the and of 3 |
| E06 | OGENE12 | drums | **12 / 8T** | The 12-pulse ogene/gankogui bell timeline (2 2 1 2 2 2 1) on the BELL lane, kicks on pulses 1 and 7: one bar of 12/8 |
| E07 | CLAVE32 | drums | 16 / 1/16 | 3-2 son clave on the RIM (CLAVE in the 77 kit) |
| E08 | SHKR16 | drums | 16 / 1/16 | Straight-sixteenth shakers: accents on the beats and ands, ghosts between — bounce from velocity, not timing |
| E09 | LOGDRUM | melodic | 16 / 1/16 | The log drum conversing with a sparse kick: root, fifth below, flat seventh below, a slide into the one |
| E10 | LOG2 | melodic | 16 / 1/16 | The answer: a longer phrase up to the flat third and back |
| E11 | SUBROLL | melodic | 16 / 1/16 | The deep rolling sub, held, the fifth below on the and of 3 |
| E12 | STABS | melodic | 16 / 1/16 | Skeletal minor stabs on the and of 1, the and of 2 and the 4 |
| E13 | BITTERSW | melodic | 16 / 1/16 | i–VI–III–VII as triads, one chord a beat, each on the and |
| E14 | HILIFE | melodic | 16 / 1/16 | Highlife guitar, palm-wine picking over I–IV–V–I |
| E15 | OJA | melodic | 16 / 1/16 | A pentatonic flute call: a held note answered by a short figure |
| E16 | LOG2BAR | melodic | **32 / 1/16** | The log drum as a two-bar phrase: the call, then the answer with the flat third and a long slide home |
| E17 | TALKDRM | melodic | 16 / 1/16 | Talking drum: slides between two pitches, two quick strokes at the end of the bar |

Polymeter is free: put OGENE12 on the drums and a 16-step line on the bass and the 12/8
bell crosses the 4/4 — the West African cross-rhythm inside a club groove.

### 2.3 The run — transition by subtraction (GLO + D4)
Your rule, as a state machine on the beat clock. Press **D4** while holding GLO; the run
starts on the next one:

| Phase | Length (SHORT / LONG) | What happens |
| --- | --- | --- |
| SHAKERS | 1 / 2 bars | Hats, claps and rims rest (lane mask); the lead mutes |
| STABS | 1 / 2 bars | The stabs track's level ramps to nothing over the phase |
| LOG | 2 bars | The log drum and the kick alone |
| SILENCE | 1 beat | Everything muted |
| DROP | on the one | Levels, mutes and lanes exactly as before; cue note D2 |

MENU > EDDA > RUN picks SHORT or LONG. PLAY (a stop) ends a run and restores the mix at
once. The phase boundary is applied **before** the one's own step plays, so the first step
of each phase is already in the new state. The GLO map shows RUN lit while it runs.

### 2.4 The hard stop and the re-entry (GLO + E4)
**E4** mutes everything at once while the clock keeps running; **E4** again re-enters on
the next one. No tempo loss, no restart — the hard stop → re-entry of the EDDA set, on the
grid every time. Cue notes E2 (stop) and D2 (re-entry). STOP is lit on the map while stopped.

### 2.5 REVEAL — the arrangement engine (MENU > EDDA > REVEAL)
The entrainment/novelty psychology from the crate, running by itself:
- **the hole**: on every 8th bar the kick rests — the missing hit as the strongest event,
  and the return on bar 1 is the drop you didn't have to play;
- **the reveal**: in bars 17–24 of every 32 the backbeat changes voice, not place —
  SNARE hits move to the RIM lane and come back.
Nothing in the steps is changed; both are live remaps, so OFF is instant and exact.

### 2.6 FILL — fill-only steps (GLO + A4)
Steps flagged **fill-only** (`SF_FILL`, a new step flag) rest until FILL is on; press **A4** in the
GLO layer and they play from now through the end of the next bar, then rest again (press again to
stop early). The drum grids of the bank carry fills on their last steps: 3STEP a tom run with a snare
pickup on 14–16, SGIJA toms on 15–16, AFROBT snare pickups on 14 and 16, KIZOMBA a rim on 16. Cue
CC 25 carries the state. (SLOOP 2.4's "fill with one finger", with the bar-end semantics of its key 10.)

### 2.7 MUTATE — Euclidean ear-candy (GLO + B4)
On the next one, the drums track's BELL lane is re-rolled as a **Euclidean rhythm**
E(k, LEN) with k from the bell family {3, 5, 7} and a random rotation, first onset accented.
Every other lane — kick, backbeat, shakers — is left exactly as it was: constant placement
for entrainment, a changing figure for novelty. (The 12-pulse ogene timeline *is* E(7, 12);
the test suite proves it.) The generator has its own seed, so a night can be replayed.
Stopped, MUTATE applies at once.

### 2.8 Show cues (MENU > EDDA > SHOW CUES, USB MIDI OUT channel 16)
For TouchDesigner / Resolume / OBS on the stream machine:

| Message | Meaning |
| --- | --- |
| CC 20 | bars since play (mod 128) |
| CC 22 | beat of the bar, 0–3 |
| CC 21 | ACT: bulbs lit, 1–5 (MENU > EDDA > ACT, or GLO + **G4** for the next bulb) |
| CC 23 | the run's phase, 0 idle … 4 silence |
| CC 24 | the Camelot key, 1–24 |
| CC 25 | FILL on / off |
| note C2 (36) | every bar, on the one |
| note F2 (41) | the run begins |
| note D2 (38) | the drop / the re-entry |
| note E2 (40) | a hard stop |
| note G2 (43) | the bell lane re-rolled |

### 2.9 The voice bank (FM6 slots E1–E5)
The vF "future version" instruments as 6-operator FM patches, selectable as FM6 presets
OGENE, OJA, HILIFE, TALK DRUM, LOG DRUM, measured on the host renderer (note C4):

| Patch | What was measured | Reads as |
| --- | --- | --- |
| **OGENE IRON** | partials at 1.00 / 1.42 / 3.44 / 5.45 × f0, instant strike, −20 dB at 0.9 s, centroid 780 Hz (GLASS BELL: 1640 Hz) | the dark-iron bell; the keys or the pattern supply its two pitches |
| **OJA FLUTE** | fundamental + octave, sustaining, triangle vibrato, a chiff on the attack | the notched flute |
| **HILIFE GTR** | 10 ms attack, −20 dB at 0.41 s, centroid 830 Hz (brighter than NYLON PICK) | the clean highlife line |
| **TALK DRUM** | pitch falls from 323 to 258 Hz through the note (DX7 pitch envelope), −20 dB at 0.25 s | the hourglass drum; a SLIDE between steps bends it |
| **LOG DRUM** | fundamental at 129 Hz (ratio 0.5), a knock partial, −20 dB at 0.52 s, MONO with slides | the amapiano log drum |

Every patch is editable in Felucca's web editor (operators, envelopes, algorithm) and
exports as `.syx`.

### 2.10 The EDDA palette (MENU > DISPLAY > COLOR)
Deep indigo ground, royal-blue surfaces, the golden-orange of the Blue Golden Star ankara
as the theme colour, electric blue (the Blue Hand) as the accent. Passes Felucca's WCAG
contrast lint (text 17:1, labels 8.7:1, values 9.8:1).

### 2.11 Menu and layer summary
- **MENU > EDDA** (hold HOME, ALGORITHM to the EDDA tab): KEY · SHOW CUES · ACT · RUN · REVEAL.
  These are runtime settings (they reset at power-on; the key lives on in the project's
  ROOT / SCALE). The web editor sees them too (protocol ids 12–16).
- **GLO layer** (hold GLO): the stock map plus **D4 RUN · E4 STOP · G4 ACT+ · A4 FILL · B4 MUTATE ·
  C5 / D5 / E5 KEY −1 / +1 / relative**. Layer keys are silent, as in Felucca.
- The tab bar uses the small face in LARGE so five tabs fit (icon cushions 28 px, gap 6).

### 2.12 SLOOP 2.4 (7 Oct 2026) feature by feature
SLOOP 2.4 shipped today with 1,000 subscribers behind it. Where each of its headline features stands in
EDDA OS, honestly:

| SLOOP 2.4 | EDDA OS |
| --- | --- |
| FM6 with DX7 patches from the web editor | **In** (Felucca's FM6 is the engine SLOOP borrowed; `.syx` import, 13 factory patches here) |
| Own drum kits: 16 WAVs on pads | **Not in.** Felucca has three user sample slots (SAMPLE / SLICE) and synthesised kits; a 16-pad kit needs a flash-layout change we won't ship untested |
| A 4th sample slot | **Not in** (same reason) |
| New web editor in the FM-1's look | Felucca's editor + the separate Felucca-WebApp; EDDA's menu rows and bank appear in both |
| CHORD+: black keys change the chord type, strum, voice leading | **Partly.** Felucca's chord keys (TRIAD / 7TH / 9TH / SUS4 / POWER, STRUM, VLEAD) are there under SCL; the black-key shortcut is not |
| A filter on every track, one knob | **Not in** as a global mixer stage; Felucca's engines carry their own filters and the FX layer sweeps |
| 12 full-screen visualisers | Felucca's HOME oscilloscope only |
| Parameter locks | **Not in** (a step-format change) |
| Micro timing (nudge) | **Not in** (same); EDDA's timing rule is "on the grid" anyway |
| Fills with one finger | **In**: FILL, GLO + A4 (2.6), with fills written into the bank's grids |
| Chains: hold SAVE, tap A B B C | **In** (Felucca's song chain) |
| Patterns out over MIDI | **In** (Felucca sends the sequencer and keys over USB MIDI), plus EDDA's show cues |
| Drums programmed with the keys | **In** (Felucca's grid) |
| Steps up to two bars long | **In** (Felucca runs 64 steps; LOG2BAR is a 32-step phrase) |
| Dotted echoes | **Not in** |
| Every key lit in the dark | **In** (MENU > DISPLAY > LEDS) |
| Bigger values, pages, the menu in sections | **In** in Felucca's own form (LARGE, the tabbed menu) |

What EDDA OS has that SLOOP does not: the Camelot key lock and key path, the run, the hard stop with
re-entry, REVEAL, MUTATE, the show-cue MIDI map, the West African pattern bank, the voice bank.

---

## 3. On stage with the Omnis-Duo

1. FM-1 3.5 mm out → 3.5 mm stereo-to-1/4" **mono** cable → Omnis-Duo **MIC 2**. Talkover
   OFF. FM-1 master low, bring it up on the mic knob.
2. Set **KEY** to the Camelot of the record playing (or walk it with C5 / D5 / E5 as you
   mix down the one-step path). Tap the tempo on GLO + F4, or run USB/TRS MIDI clock from
   the laptop (Ableton Link → MIDI clock) and set CLOCK to it.
3. Load the kit: track 4 DRUM with 3STEP or OGENE12; track 1 LOG DRUM (E5) with LOGDRUM;
   track 2 a pad or STABS; track 3 the lead (OJA, OGENE, HILIFE).
4. REVEAL ON for long passages; MUTATE when the room needs a new figure; **D4** to leave a
   record by subtraction; **E4** for the hard stop and the re-entry.
5. USB from the FM-1 to the stream machine: show cues on channel 16 drive the bulbs, the
   column and the Field; the same USB carries Felucca's 44.1 kHz audio input for recording.

---

## 4. Build and install

Prerequisites (Felucca's `BUILDING.md`): Linux x86-64 or macOS with Docker; Python 3 with
Pillow and fontTools; network access to pkgman.jieliapp.com for the JieLi pi32v2 toolchain (the
one thing this tree could not fetch for you: that server was unreachable from where it was built).
The three AC79 SDK files the package needs are vendored in `third_party/ac79-sdk-tools`.

```
sh edda-build.sh                # fetches JieLi's toolchain, uses the vendored SDK tool files
                                # (third_party/ac79-sdk-tools), runs ./build.sh -> build/felucca.fwsc
tests/run_tests.sh              # the host suites (all passing in this tree, goldens included)
```

Install from a local copy of the site (Web MIDI needs localhost), Chrome or Edge:

```
python3 web/make_site.py build/felucca.fwsc dev /tmp/edda-site
cd /tmp/edda-site && python3 -m http.server 8000     # open http://localhost:8000/webapp/installer/
```

or from a terminal: `pip3 install mido python-rtmidi && python3 tools/fm1_install.py build/felucca.fwsc`.

The package identity stays `FM-1_900`, so Felucca's installer and its **Return to official
V15** work unchanged. Installing firmware is at your own risk; if the FM-1 stops starting,
recovery is FM-1-transporter (a Seeed XIAO RP2040 and three wires), as for Felucca.

---

## 5. What is verified, and what is not

Verified on the host (gcc, Felucca's own harness, this fork's `tests/edda_test.c`):
the Camelot wheel (all 24 positions, names, neighbours, the path); every pattern's load
(lengths, divisions, transposition into the root, triads, drum lanes, velocities);
Euclidean rhythms (E(7,12) = the ogene timeline, E(5,16) = bossa-nova, E(3,8) = tresillo);
the run phase by phase against the real sequencer at 120 BPM (lane mask, mutes, the stabs'
ramp, the exact restore, the cues, SHORT and LONG, a stop mid-run); the hard stop and the
re-entry on the one; REVEAL's hole and lane move by bar number; MUTATE's isolation to the
bell lane and its seed; MENU > EDDA rows, names, steps and the editor protocol (MENU_DESC /
MENU_SET ids 12–16); the GLO layer keys and LEDs; the FM6 bank (13 patches, names, presets).
The upstream suites pass: ui, chord, scale, editor, backup, project, upreset, fm6, ratchet,
motion, midi_control, perform, swing, theme, text_ref, mod, drum.

Also verified since the first cut: `tests/regress.c` (97 golden renders, the five new voices
pass its level / DC / clipping health checks, CPU budget kept, goldens rewritten), the web
editor's protocol tests (`web/test_web.mjs`, 234 checks, with the editor's own FM6 tables
extended to the 13 patches), the installer CLI test, and the fill / two-bar additions.

Not yet: a flash onto hardware (the toolchain server is not reachable from this build
environment; `edda-build.sh` is the one-step build on your machine); listening tests of the
five voices (numbers only); the browser emulator (`web/emu`, needs Emscripten); the SLOOP
features marked "not in" above.

---

## 6. Frontier (next)

- **Ableton Link over the AC7911's own Wi-Fi.** The chip is a Wi-Fi/BLE AIoT SoC and Felucca
  does not use the radio yet. Link on the FM-1 would make it the first pocket instrument
  that sits in the stream rig's clock without a cable.
- **Two-bar patterns (32 steps)** for the log drum phrases; the loader already takes LEN.
- **The chant lane**: load the room's recorded "Yaa" into a SAMPLE user slot on the lead
  track; a key is the call. (A dedicated CALL key was removed: layer keys stay silent.)
- **YAA counter in**: the stream kit's /yaa endpoint as MIDI CC into the FM-1 (USB MIDI IN
  is class-compliant) to drive MUTATE density from the room.
- **Set file**: the 40-track crate's keys and tempos as a Camelot path the FM-1 steps
  through with C5 / D5 (the structure is in `edda_cam_neighbours`).

Felucca © 2026 Leo Kuroshita, Hügelton Instruments, GPL-3.0-only. EDDA OS additions
(`firmware/src/edda.c`, `edda.h`, `tests/edda_test.c`, the bank, the palette and the hooks)
are released under the same licence.
