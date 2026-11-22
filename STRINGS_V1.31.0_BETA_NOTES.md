# Himalia Strings v1.31.0-beta — Natural Adaptive Attack & Release

This beta begins the Hybrid Adaptive Expressive String Engine (HAESE) implementation with **Natural Default** behavior.

## Included in this phase

- Replaced the old per-sample attack coefficient with a per-note, one-pole bow-envelope coefficient.
- Attack time is instrument-aware: violin/viola respond more quickly; cello and bass establish the bow more gradually.
- Velocity and articulation shape attack time continuously. Stronger notes can speak more promptly; softer notes ease in more slowly.
- Release is a separate bow-energy decay. Short stopped-bow releases are softened, while lifted long notes decay more gradually.
- Cello and bass receive longer release times than violin.
- Reduced non-pitched onset, finger-change, bow-change, tremolo-change, and release scratch impulses so they no longer create a sharp click on top of the physical string response.
- The string and body resonators are not globally shortened or muted; they continue to ring after bow energy is removed.

## Preserved

- CV pitch and gate remain authoritative.
- No pitch quantization or change to CV scaling.
- Existing articulations, pizzicato pulse, adaptive glide, adaptive vibrato/tremolo and ensemble layouts remain in place.
- No limiter, compressor, or automatic gain control.
- STR_OUT_GAIN and the existing output calibration are unchanged.

## Validation

The host DSP smoke test now checks the natural attack envelope and verifies that cello attack/release coefficients are more gradual than violin. The GitHub Actions firmware compile and full DSP test must both pass before treating this beta as ready for hardware testing.

The physical output voltage still needs to be measured on the actual module.
