# Himalia Strings beta

Branch: `himalia-strings-beta`

This branch is an isolated development line. The original `main` firmware is unchanged.

## Target

Turn the complete Himalia into a CV-controlled, monophonic bowed-string instrument with an optional internally generated string section. One pitch CV is the root; ensemble voices derive their pitches from that root.

## Initial DSP core

`IOCore/HimaliaSketch/strings_engine.h` introduces a fixed-memory, header-only DSP prototype for the ATSAMD51:

- six independently tuned string voices;
- violin and cello body-mode banks;
- fractional-delay string loops;
- nonlinear bow/string coupling prototype;
- solo and ensemble operation;
- unison, octave, fifth, major, minor and sus4 voice layouts;
- bow pressure, bow speed and vibrato controls.

The six voices are summed to the existing mono SAMPLE OUT. No samples, heap allocation or extra hardware are required.

## Important status

This is the first DSP scaffold, **not yet connected to the live firmware controls and not yet a release-ready bowed-string model**. In particular, the bow friction model, pitch stability, amplitude calibration, body resonances and CPU budget need listening tests and measurement on the actual module.

The next integration pass must:
1. connect the existing 1 V/oct SAMPLE TUNE input to the root pitch;
2. define a clear selector layout for violin, cello, ensemble and chord modes;
3. map the existing controls to bow pressure, bow speed, bow position/timbre and vibrato;
4. keep all expensive coefficient calculations outside the audio interrupt;
5. benchmark worst-case six-voice CPU use at 47.619 kHz;
6. build the UF2 with the repository's Docker/Arduino CLI toolchain and test on hardware.

## Design principle

The original sequencer/sample/resonator firmware on `main` is preserved. This branch is intentionally separate until DSP and control behaviour are validated.
