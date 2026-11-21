# Himalia Strings beta

Branch: `himalia-strings-beta`. `main` stays the resonator / wavetable line.

Firmware: `firmware/Himalia_strings_v1.13.0-beta.uf2`

## Concept

SAMPLE OUT is a bowed-string physical model: up to six bowed strings,
generated from **one** pitch CV. Not a sample player and not polyphonic MIDI:
the ensemble intervals come from the one incoming pitch. The six channels of
the former squares sequencer now drive the **generative divisi**.

## Controls

| Himalia control | Strings function |
|---|---|
| SAMPLE TUNE knob + CV | pitch: knob 7 octaves from A0, CV 1 V/oct (-10 ... +10 V) |
| RATCHET knob | model (18, see below) |
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

## Bowing on the tempo (v1.10)

Players bow according to the note length: the time between the last gates
(from the rhythm, see Tempo) sets the bow speed of every new note: 0.6 s =
normal, shorter notes up to 1.6x faster, longer notes down to 0.6x (free
playing without a clock: a normal bow). With it:
- a fast bow plays a bit further from the bridge, is ~+2 dB louder and a bit
  brighter; a slow bow plays closer to the bridge, ~-1 dB, with a little
  more grip (stable Helmholtz motion up to the top of the range);
- **bow changes**: on long notes the bow runs out (about 4 ... 6 s, every
  player has their own bow length): the player slows down, turns with a small
  scratch and speeds up again (~90 ms, about -7 dB dip). In a section the
  players change bow at different moments, as real sections do.

## Note off (v1.12)

- The released note **keeps its pitch** until the next gate (a sequencer may
  already be on the next note; the ringing string no longer slides along).
- **Stop or lift**, from the tempo: short notes (< 0.35 s between gates) and
  martele: the bow **stops on the string**: a short bite, the bow keeps
  resting and damps the string (~-20 ... -40 dB after 120 ms). Long notes: the
  bow **lifts off**: its grip goes away (not just its speed), a soft whisper,
  the string rings on; the taper takes 50 ... 180 ms, longer for longer notes.
- Vibrato carries on while the note fades.
- **Finger lift**: 0.18 s after a lifted note the finger comes off the string:
  what still rings continues as the open string below it, with a tiny tap.

## Noise (v1.10)

The rosin noise now comes with the slip: most of it while the string slides
under the hair (once per period, ~25 % of it), so it is part of the tone
instead of a constant hiss. The scratch at the start of a stroke goes more
into the string and the part heard directly is band-passed (~1.5 ... 6 kHz,
hair on rosin) instead of highpassed white noise. Measured: harmonics-to-
noise ~35 -> ~43 dB (median over the violin range): cleaner sustain, noise
where the bow does something.

## Vibrato and the noises around it

Vibrato is played like a string player does it:

- a straight tone first (~0.12 ... 0.24 s), then the vibrato starts a bit
  slower and narrower and settles in ~0.5 s;
- every cycle the hand wobbles: rate +-4 %, width +-15 %;
- the finger rolls back from the note, so the pitch swings mostly below it
  (slightly asymmetric shape); the bow arm moves along a little;
- the rosin noise gets rougher where the finger moves fastest (the middle of
  every vibrato swing), so the noise pulses with the vibrato;
- a pitch change while the bow keeps going (legato) is a finger change: a
  small finger tap into the string plus a click, and the vibrato stops and
  starts again;
- trills: every finger hit and lift taps.

, also for ostinati)

Every gate start (TRIGGER or the dynamic gate on CV IN RATE) is logged with
its time and velocity; the last 32 intervals are analysed:

1. **pulse**: the largest grid all intervals are whole multiples of, so
   irregular rhythms work (3+3+2, gallops, long-short-short, 16th ostinati);
2. **pattern**: the shortest repeating group of intervals and accents
   (up to 16 notes), i.e. one bar of the ostinato;
3. **beat**: with a dynamic gate, regular accents mark the grid and the beat
   is its division nearest 128 BPM; otherwise the division of the pattern
   nearest 128 BPM. Half / double tempo is possible and harmless (vibrato and
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
| 2 | solo viola | one viola (own body, vibrato and register) |
| 3 | solo cello | one cello |
| 4 | violin section | six violins |
| 5 | viola section | six violas |
| 6 | cello section | six cellos |
| 7 | full strings | violin I, violin II, viola, cello, cello, double bass |
| 8 | molto vibrato violin | solo violin, wider and faster vibrato |
| 9 | romantic cello | solo cello, slow wide vibrato, every note swells a little (legato) |
| 10 | tremolo section | violins, always tremolo on the tempo, tight ensemble |
| 11 | trill section | violins trilling a whole tone, ~9 notes/s or on the tempo |
| 12 | epic ostinato | violas, cellos and basses in octaves, short punchy spiccato, very tight (trailer-style ostinatos) |
| 13 | suspense | violins sul ponticello tremolo in a semitone cluster (thriller / horror) |
| 14 | heroic theme | full strings in octaves, marcato, big vibrato (epic themes) |
| 15 | emotional swell | full strings, every note swells in over ~1.6 s, rich vibrato |
| 16 | shimmer | violins sul tasto in octaves, slow vibrato, slow swell (airy high pads) |
| 17 | overpressure (experimental) | solo cello with a heavy, creaking bow. **PRESSURE knob**: below 0.4 crunch only, 0.4 ... 0.75 an octave below, above 0.75 an octave + fifth below (subharmonics like the extended technique "ALF") |
| 18 | Threnody swarm (experimental) | violin section that glides apart from the note into a quarter-tone cluster (up to +-2 semitones) and back together, ~12 s per cycle from every gate; tremolo with the BIT switch |

Models 9 ... 18 set some of their own articulation / layout; the knobs for
those are then ignored (the models that leave them free still follow the
knobs). Models 12 ... 16 are inspired by typical film-score string writing,
17 and 18 by 20th-century extended techniques; not copies of any score.

**Overpressure** physics: the simple friction model does not fall into real
subharmonics by itself, so the bow speed is driven per string period (one
period hard, the others weak, locked to the string): the motion repeats every
2 or 3 periods. Measured: the subharmonic series 4 ... 11 dB (octave) and
5 ... 7 dB (twelfth) above the normal harmonics, level made up.

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
direct string sound. Bodies nobody plays on are skipped. (v1.9 ... v1.12 had an
impulse-response body; removed in v1.13: too heavy for the module.)

**Air**: like a mix engineer's air shelf. The body filters the top octave
away, so the air band (4th-order highpass at 9.5 kHz: bow hair, rosin, top
harmonics) is taken from the strings themselves and added after the body,
about +9 dB at 12-20 kHz (`STR_AIR_DEFAULT`, less with mute).

Ensemble humanisation, per player: static intonation (+-6 cents) plus slow
drift (+-4 cents), own vibrato phase, rate (when not synced) and depth that
fades in after the note starts and grows with the bow speed, bow speed (+-10%)
and pressure differences, onset delay up to 35 ms and release delay up to 15 ms.

Tuning (v1.2): refined body resonances (A0, B1-, B1+, bridge hill) per
family, top-end roll-off per family (violin 5.5 kHz ... bass 2 kHz), rosin
noise on the bow (more with pressure and near the bridge), vibrato rate and
depth per family (violin 5.6 Hz +-17 c ... cello 5.0 Hz +-21 c).

## Verification

- `tests/strings_engine_smoke.cpp` (GitHub Actions): every model (18) x layout
  and every articulation finite, bounded and sounding; tempo sync (gates every
  0.5 s -> vibrato 6 Hz, tremolo 12 strokes / s); solo intonation within 10 cents for violin
  G3 / A4 / A5 and cello C2 / C3 / C4; pizzicato decays; tremolo sounds;
  release goes silent.
- Measured on the host: intonation 0 ... +9 cents across the range; bow
  position changes brightness (ponticello bright, tasto dark).
- Firmware builds: ~43 KB flash, ~84 KB RAM.
- **CPU load**: estimated ~80-85% of the time per sample with six players
  (v1.10 ... v1.13 add ~10 % on the host: vibrato, bowing, noise).
  CPU guard: when the audio interrupt takes more than 90% of the time per
  sample, the firmware drops a player (down to 3) and brings it back after
  ~2 s with room to spare. The serial monitor (115200 baud) prints the worst
  interrupt time (`cpu xx%`) and the current player limit.
- Not yet verified on the module: listening, CV calibration, real CPU load.
