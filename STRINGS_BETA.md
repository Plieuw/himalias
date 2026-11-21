# Himalia Strings beta

Branch: `himalia-strings-beta`. `main` stays the resonator / wavetable line.

Firmware: `firmware/Himalia_strings_v1.7.0-beta.uf2`

## Concept

SAMPLE OUT is a bowed-string physical model: up to six bowed strings,
generated from **one** pitch CV. Not a sample player and not polyphonic MIDI:
the ensemble intervals come from the one incoming pitch. The six channels of
the former squares sequencer now drive the **generative divisi**.

## Controls

| Himalia control | Strings function |
|---|---|
| SAMPLE TUNE knob + CV | pitch: knob 7 octaves from A0, CV 1 V/oct (-10 ... +10 V) |
| RATCHET knob | model (14, see below) |
| SPREAD knob + CV | layout: unison, octaves, fifths, major, minor, divisi major, divisi minor, cluster |
| SAMPLE SELECT knob + CV | bow pressure |
| CLOCK RATE knob | bow speed |
| CV IN RATE jack | **dynamic gate** / bow CV, 0 ... 8 V (up to 10 V a bit more): the level is how hard the bow plays. Above ~0.4 V it is also a gate (plays notes, steps the divisi, gives the tempo). A slow CV here makes swells |
| SQUARES MAIN TUNE knob + CV | articulation (11 steps, see below); the CV can sequence articulations |
| TRIGGER in / button | gate: bow / pluck / strike; steps the divisi register; the rhythm of the gates gives the tempo (also irregular ostinati) |
| A/B button (sample side) | A = open, B = con sordino (mute: darker, softer, shorter ring) |
| A/B button (squares side) | A = vibrato, B = senza vibrato |
| BIT switch | tremolo (on the tempo) |
| SAMPLE OUT | full ensemble, Eurorack level +-5 V |
| noise side DAC out | divisi players only (players 2 ... 6) as strings, +-5 V; with SAMPLE OUT usable as a stereo pair |
| PA22 (digital noise out) | 5 ms trigger when the divisi changes |
| square outputs 1 ... 6 | each player's note as a square wave while it sounds (synth layer following the divisi) |

## Dynamic gates

The TRIGGER input only knows on / off. For velocity, patch a dynamic gate
(gate whose height is the velocity) into CV IN RATE. Per note the level sets:
bow speed (loudness, continuously, so a falling gate also fades), bow
pressure (harder = grittier), accent, scratch amount, vibrato depth,
pizzicato strength and finger hardness, and the brightness of the radiated
sound. Measured: velocity 0.3 -> 1.25 is about +7 dB and brighter; the top
end is softly limited.

## Output level

Both audio outputs are scaled to Eurorack level: digital +-1 = +-5 V
(10 Vpp), centred on the DAC code that gives 0 V at the jack. The scaling
comes from the output stage in the preliminary schematic
(Vout = DAC volts x 3.397 - 6.05 V); if a module differs, adjust
`SEQ_CV_ZERO_CODE` / `SEQ_CV_CODES_PER_V`. Typical playing peaks at ~+-5 V,
loud accents are softly limited.

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

Noise transients (v1.5): every stroke starts with a short bow scratch
(~12 ms, more for martele / spiccato / ponticello, little for legato and
tasto), legato note changes and tremolo bow changes add a small scratch,
the bow lifting off gives a puff, pizzicato a finger snap and col legno a
wood click. Part of the noise drives the string, part is heard directly
through the body (highpassed).

After the bow leaves the string it rings on (per articulation, the same time
for every note); con sordino shortens that.

## Tempo (rhythm analysis, also for ostinati)

Every gate start (TRIGGER or the dynamic gate on CV IN RATE) is logged with
its time and velocity; the last 32 intervals are analysed:

1. **pulse**: the largest grid all intervals are whole multiples of, so
   irregular rhythms work (3+3+2, gallops, long-short-short, 16th ostinati);
2. **pattern**: the shortest repeating group of intervals and accents
   (up to 16 notes), i.e. one bar of the ostinato;
3. **beat**: with a dynamic gate, regular accents mark the grid and the beat
   is its division nearest 120 BPM; otherwise the division of the pattern
   nearest 120 BPM. Half / double tempo is possible and harmless (vibrato and
   tremolo pick subdivisions anyway).

A change of tempo is picked up after two notes off the grid; a jump of the
beat needs a second confirmation; after 4 s without gates the clock stops.
Vibrato runs on the subdivision closest to its natural rate (~5.5 Hz) and
tremolo on the one closest to ~13 strokes per second, strokes starting on
every note. Without a clock both run free. Serial monitor shows bpm and the
pattern (notes / pulses).

## Models (RATCHET knob, left to right)

| # | model | what it is |
|---|---|---|
| 1 | solo violin | one violin |
| 2 | solo cello | one cello |
| 3 | violin section | six violins |
| 4 | cello section | six cellos |
| 5 | full strings | violin I, violin II, viola, cello, cello, double bass |
| 6 | molto vibrato violin | solo violin, wider and faster vibrato |
| 7 | romantic cello | solo cello, slow wide vibrato, every note swells a little (legato) |
| 8 | tremolo section | violins, always tremolo on the tempo, tight ensemble |
| 9 | trill section | violins trilling a whole tone, ~9 notes/s or on the tempo |
| 10 | epic ostinato | violas, cellos and basses in octaves, short punchy spiccato, very tight (trailer-style ostinatos) |
| 11 | suspense | violins sul ponticello tremolo in a semitone cluster (thriller / horror) |
| 12 | heroic theme | full strings in octaves, marcato, big vibrato (epic themes) |
| 13 | emotional swell | full strings, every note swells in over ~1.6 s, rich vibrato |
| 14 | shimmer | violins sul tasto in octaves, slow vibrato, slow swell (airy high pads) |

Models 7 ... 14 set their own articulation and/or layout; the knobs for those
are then ignored (the models that leave them free still follow the knobs).
Models 10 ... 14 are inspired by typical film-score string writing, not copies
of any score.

## Instruments and layouts

- **Solo violin / solo cello**: one player, the layout is ignored.
- **Violin / cello section**: six players of the same family.
- **Full strings**: violin I, violin II, viola, cello, cello, double bass, each
  in their own register (unison = everyone on the note in their octave).
- **Low strings** (used by epic ostinato): violas, cellos and basses.
- **Cluster** layout: neighbouring semitones around the note.
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
