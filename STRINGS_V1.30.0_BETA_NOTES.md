# Himalia Strings v1.30.0-beta

Based on v1.29.0-beta.

## Softer, adaptive attack and release

- Reduced the initial scratch/click transient and made its level follow note velocity, pressure, and small per-note variation.
- Bowed-string attacks are more restrained; low-velocity notes ease in more slowly than strong notes.
- Note-off transients are substantially quieter and vary with the note's dynamic level.
- Release envelopes are smoother and longer, including short-note bow stops.
- Reduced incidental transients from legato finger changes, tremolo bow changes, bow reversals, trill taps, and finger lift.
- Preserved the pizzicato pluck impulse itself; the transient layer is quieter without removing the plucked-string character.

No limiter, compressor, or automatic gain control added. Output gain and CV mapping remain unchanged.
