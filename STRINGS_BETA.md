# Himalia Strings beta

Branch: `himalia-strings-beta`. `main` stays the resonator / wavetable line.

Firmware: `firmware/Himalia_strings_v1.1.0-beta.uf2`

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
| SQUARES MAIN TUNE knob + CV | bow position: sul tasto (left) ... normal ... sul ponticello (right) |
| TRIGGER in / button | gate: bows while high; plucks in pizzicato; steps the divisi register |
| A/B button (sample side) | A = arco, B = pizzicato |
| A/B button (squares side) | A = vibrato, B = senza vibrato |
| BIT switch | tremolo |
| noise side DAC out | envelope follower, 0 ... +5 V |
| PA22 (digital noise out) | 5 ms trigger when the divisi changes |
| square outputs | silent (their sequencer drives the divisi) |

Legato: change the pitch while the gate stays high and the players glide
(~30 ms) without a new attack.

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
drift (+-4 cents), own vibrato rate (5.0 - 6.2 Hz) and depth that fades in
after the note starts, bow speed (+-10%) and pressure differences, onset delay
up to 35 ms and release delay up to 15 ms.

## Verification

- `tests/strings_engine_smoke.cpp` (GitHub Actions): every instrument x layout
  finite, bounded and sounding; solo intonation within 10 cents for violin
  G3 / A4 / A5 and cello C2 / C3 / C4; pizzicato decays; tremolo sounds;
  release goes silent.
- Measured on the host: intonation 0 ... +9 cents across the range; bow
  position changes brightness (ponticello bright, tasto dark).
- Firmware builds: 32 KB flash, 82 KB RAM.
- **CPU load**: estimated ~75% of the time per sample with six players in full
  strings. Not measured on the module yet: the serial monitor (115200 baud)
  prints the worst interrupt time as `cpu xx%`. Above ~90% the audio will
  glitch; then the voice count or body size has to come down.
- Not yet verified on the module: listening, CV calibration, real CPU load.
