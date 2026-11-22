# Himalia Strings v1.27.0-beta

- Raised the calibrated digital output gain from 0.60 to 0.72 (+20%, approximately +1.6 dB). No limiter, compressor, or automatic gain control was added.
- Updated the host loudness/peak report to use the same 0.72 gain so the CI peak-headroom check tests the actual firmware calibration.
- Replaced the per-voice linear-congruential bow-noise generator with xorshift32, matching the better-quality PRNG already used elsewhere in the engine. This aims to reduce patterned/tonal noise that can resemble a digital clock whine.
- Kept the v1.26 body resonance voicing and all CV, articulation, pitch-glide, and ensemble mappings unchanged.

The v1.26 host report measured a maximum peak of 0.8015 at gain 0.60; a linear 20% increase predicts approximately 0.962 at gain 0.72 under the same test conditions. This leaves modest digital headroom, but actual Eurorack output voltage and any hardware clock coupling must still be verified on the module.
