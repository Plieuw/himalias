# Himalia Strings beta

Branch: `himalia-strings-beta`

This is an isolated development line. The original `main` firmware remains unchanged.

## Instrument concept

Himalia's mono SAMPLE OUT is now driven by a CV-controlled bowed-string engine. One incoming pitch CV supplies the root note; ensemble intervals are generated internally from that pitch. It is not a polyphonic MIDI instrument.

## Current control mapping

| Himalia control | Strings function |
|---|---|
| SAMPLE TUNE knob + CV | Root pitch; CV remains 1 V/oct |
| A/B button (PA20) | Violin / cello |
| RATCHET knob | Solo, unison section, octaves, fifths, major, minor, sus4 |
| SAMPLE SELECT knob + CV | Bow pressure |
| CLOCK RATE knob | Bow speed |
| TRIGGER input / manual trigger | Note gate |
| BIT switch | Holds the bow gate open |
| SAMPLE OUT | Mono sum of the active string voices |

The six square outputs and their generative sequencer are retained as auxiliary functions. The CV-driven bowed-string voice itself is monophonic at its input; chord layouts are generated from its one root pitch.

## DSP engine

`IOCore/HimaliaSketch/strings_engine.h` contains a fixed-memory, header-only engine for the ATSAMD51:

- six independently tuned fractional-delay string voices;
- separate violin and cello modal body banks;
- nonlinear bow/string coupling prototype and bridge-loss filtering;
- solo and six-voice ensemble modes;
- unison, octave, fifth, major, minor and sus4 interval layouts;
- per-voice vibrato phase offsets;
- output DC blocking.

No sample playback, heap allocation or additional hardware is required. The engine is rendered at the existing 47.619 kHz audio interrupt rate.

## Verification status

- Host-side DSP smoke test: **passing**. GitHub Actions compiles the engine with C++11 and checks finite output, bounded peak level, non-silent solo output and six-voice ensemble activation.
- Full ATSAMD51 firmware build: handled by the separate `Himalia firmware build` workflow; check its latest run before treating the firmware as build-verified.
- Physical module listening, CV calibration and worst-case interrupt-time measurement: **not yet verified**.

This remains an experimental bowed-string model, not an acoustically validated violin/cello emulation. The bow interaction, pitch stability, attack/release, body response and six-voice CPU budget still need listening tests and refinement.

## Next DSP work

1. Measure worst-case six-voice CPU time at 47.619 kHz.
2. Tune bow friction, pressure response and onset behaviour against recorded reference notes.
3. Refine the string model and body resonances for a more convincing violin/cello timbre.
4. Calibrate output level and the physical CV/control ranges on the actual module.
5. Test legato, tremolo and pizzicato as later articulation modes.
