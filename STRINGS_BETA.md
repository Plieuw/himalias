# Himalia Strings beta

Branch: `himalia-strings-beta`. `main` stays the resonator / wavetable line.

Firmware: `firmware/Himalia_strings_v1.2.0-beta.uf2`

## Concept

SAMPLE OUT is a bowed-string physical model: up to six bowed strings,
generated from **one** pitch CV. Not a sample player and not polyphonic MIDI:
the ensemble intervals come from the one incoming pitch. The six channels of
the former squares sequencer now drive the **generative divisi**.

## Controls

| Himalia control | Strings function |
|---|---|
| SAMPLE TUNE knob + CV | pitch: knob 7 octaves from A0, CV 1 V/oct (-10 ... +10 V) |
| RATCHET knob | instrument: solo violin, solo cello, violin section, cello section, full strings |
| SPREAD knob + CV | layout: unison, octaves, fifths, major, minor, divisi major, divisi minor |
| SAMPLE SELECT knob + CV | bow pressure |
| CLOCK RATE knob | bow speed |
| CV IN RATE jack | bow speed CV (0 ... +5 V). When patched, the CV alone can bow (swells without a gate) |
| SQUARES MAIN TUNE knob + CV | articulation (11 steps, see below); the CV can sequence articulations |
| TRIGGER in / button | gate: bow / pluck / strike; steps the divisi register; the time between gates is the tempo |
| A/B button (sample side) | A = open, B = con sordino (mute: darker, softer, shorter ring) |
| A/B button (squares side) | A = vibrato, B = senza vibrato |
| BIT switch | tremolo (on the tempo) |
| noise side DAC out | envelope follower, 0 ... +5 V |
| PA22 (digital noise out) | 5 ms trigger when the divisi changes |
| square outputs | silent (their sequencer drives the divisi) |

## Articulations (SQUARES MAIN TUNE, left to right)

| # | articulation | what it does |
|---|---|---|
| 1 | legato | soft attack; a new gate while the bow is still moving continues the stroke, pitch glides ~30 ms |
| 2 | détaché | every note a new stroke, bow direction alternates, small accent |
| 3 | martelé / marcato | strong accent with extra pressure that bites in, then releases |
| 4 | staccato | short stroke (bow stops after ~120 ms even if the gate stays high) |
| 5 | spiccato | very short, light, bouncing stroke that rings on |
| 6 | sul tasto | bow over the fingerboard, light pressure: soft, flute-like |
| 7 | sul ponticello | bow at the bridge: glassy, bright |
| 8 | col legno battuto | the wood of the bow strikes the string: percussive tick with pitch |
| 9 | flageolet | natural harmonic: an octave higher, light, pure |
| 10 | pizzicato | plucked |
| 11 | portamento | expressive slide (~0.2 s) on every pitch change |

After the bow leaves the string it rings on (per articulation, the same time
for every note); con sordino shortens that.

## Tempo

The time between gate starts is the tempo (must repeat within ~15%, 10 ms ...
4 s). Vibrato then runs on the beat subdivision closest to its natural rate
(~5.5 Hz, e.g. 6 Hz at 120 BPM eighths) and tremolo on the subdivision closest
to ~13 strokes per second, with the strokes starting on the beat. Without a
steady clock both run free (every player slightly different).

## Instruments and layouts

- **Solo violin / solo cello**: one player, the layout is ignored.
- **Violin / cello section**: six players of the same family.
- **Full strings**: violin I, violin II, viola, cello, cello, double bass, each
  in their own register (unison = everyone on the note in their octave).
- Layouts per player (semitones): see `STR_SECTION_LAYOUT` / `STR_FULL_LAYOUT`
  in `strings_engine.h`.
- **Divisi**: on every trigger the 16-bit Turing register shifts (25% chance the
  bit flips); its bits pick which players play and which chord tone each takes
  (pool: root, third, fifth, octave, tenth, twelfth, fourth below, two octaves).
- Notes below an instrument's lowest open string are played an octave higher.

## DSP (`IOCore/HimaliaSketch/strings_engine.h`)

Per player:
- digital waveguide string, split at the **bow position** into a bridge-side
  and a nut-side segment;
- **bow / string friction junction** (stick-slip): the friction curve depends
  on the velocity difference between bow and string and on the bow pressure,
  producing Helmholtz motion (the missing harmonics at the bow position appear
  as in a real string, e.g. no 8th harmonic at 1/8);
- lossy, frequency-dependent bridge reflection, rigid nut;
- loop delay compensated for the bridge filter: intonation within a few cents;
- pizzicato pluck, tremolo bow strokes, bow attack/release envelope.

Per family (violin, viola, cello, bass): a modal **body** (6 resonances:
air / wood modes and the bridge hill) driven by the bridge force, plus the
direct string sound. Bodies nobody plays on are skipped.

Ensemble humanisation, per player: static intonation (+-6 cents) plus slow
drift (+-4 cents), own vibrato phase, rate (when not synced) and depth that
fades in after the note starts and grows with the bow speed, bow speed (+-10%)
and pressure differences, onset delay up to 35 ms and release delay up to 15 ms.

Tuning (v1.2): refined body resonances (A0, B1-, B1+, bridge hill) per
family, top-end roll-off per family (violin 5.5 kHz ... bass 2 kHz), rosin
noise on the bow (more with pressure and near the bridge), vibrato rate and
depth per family (violin 5.6 Hz +-17 c ... cello 5.0 Hz +-21 c).

## Verification

- `tests/strings_engine_smoke.cpp` (GitHub Actions): every instrument x layout
  and every articulation finite, bounded and sounding; tempo sync (gates every
  0.5 s -> vibrato 6 Hz, tremolo 12 strokes / s); solo intonation within 10 cents for violin
  G3 / A4 / A5 and cello C2 / C3 / C4; pizzicato decays; tremolo sounds;
  release goes silent.
- Measured on the host: intonation 0 ... +9 cents across the range; bow
  position changes brightness (ponticello bright, tasto dark).
- Firmware builds: 32 KB flash, 82 KB RAM.
- **CPU load**: estimated ~80-85% of the time per sample with six players.
  CPU guard: when the audio interrupt takes more than 90% of the time per
  sample, the firmware drops a player (down to 3) and brings it back after
  ~2 s with room to spare. The serial monitor (115200 baud) prints the worst
  interrupt time (`cpu xx%`) and the current player limit.
- Not yet verified on the module: listening, CV calibration, real CPU load.
