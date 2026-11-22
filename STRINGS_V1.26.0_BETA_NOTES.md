# Himalia Strings v1.26.0-beta — listener-perspective voicing

Based on v1.25.0-beta. This beta changes only the tonal balance of the physical-model output.

## Changes
- Reduced body/modal resonance gain and Q, with the largest reductions in the cello and bass low modes. This should make the low body/IR-like ringing less prominent and reduce the sense of a resonant low-end bump.
- Reduced sympathetic-string send level from 0.50 to 0.30 so open-string bloom supports the note instead of drawing attention to itself.
- Reduced the high-frequency air layer from 0.50 to 0.28.
- Softened the directly radiated bow/rosin transient and brightness tilt.
- Voicing target: a listener hearing the complete instrument/section at a natural distance, rather than exaggerated close-miked bow/string detail.

## Preserved
- CV mapping, instrument selection, articulation controls and continuous pitch glide.
- Solo and ensemble architecture; six-voice ceiling and divisi behavior.
- No limiter, compressor or automatic gain control.
- Existing output gain calibration constant (STR_OUT_GAIN = 0.60) is unchanged. The 5 V peak target still depends on the actual DAC/output-stage calibration and must be measured on hardware; this firmware change does not add a hard voltage clamp.

## Validation
This source branch must pass the GitHub Actions firmware build and DSP smoke test before its UF2 is treated as ready. Listening and output-voltage checks on a physical Himalia are still required.
