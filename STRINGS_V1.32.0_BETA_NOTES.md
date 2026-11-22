# Himalia Strings v1.32.0-beta — Natural Default player, clean output

Builds on v1.31.0. HAESE phase D (phrase recognition) starts here, plus three
fixes from the v1.31 review.

## Natural Default: the player interprets the phrase

A small Control Interpreter + Adaptive Playing Brain (`StrBrain`), kept apart
from the string physics. Once per gate start it reads the note history
(previous pitch, time between notes, the silence before the note, the typical
note length) and hands per-note intentions to the players:

- **Phrase continuity**: a rest (long against the rhythm, > 0.25 s) or a long
  pause starts a new phrase; a gate that hardly opened (< 40 ms) is a tied note
  (same bow, no new attack).
- **Phrase start**: down-bow, the bow is placed (attack ~25 % gentler), a
  little less energy.
- **Bow energy** over the phrase: grows over the first notes; rising lines lean
  in (+1 % per semitone), falling lines relax; a new high point of the phrase
  gets a little more; repeated notes alternate down / up-bow weight.
- **Leaps** (>= a fourth): a more deliberate placement and an expressive
  arrival (more vibrato).
- **Vibrato intention**: quick notes hardly any, long notes the full vibrato,
  which then still grows while the note lasts (~1.4 s). A calm phrase stays
  calm.
- Deterministic: no randomness in the brain; the same CV / gate phrase gives
  the same performance (host test). The players' own small differences stay.

Example (solo violin, two 8th-note phrases at 100 BPM with a rest between):

| note | new phrase | energy | attack | vibrato |
|---|---|---|---|---|
| D | yes (down-bow) | 0.92 | 1.25 | 1.00 |
| E ... B (rising) | | 1.02 ... 1.07 | 1.00 | 1.15 ... 0.81 |
| A (after the rest) | yes (down-bow) | 0.92 | 1.25 | 0.65 |
| G ... D (falling) | | 0.93 ... 0.98 | 1.00 | 0.83 ... 0.64 |
| A (leap down a fifth) | | 0.95 | 1.10 | 0.67 |

## Fixes from the v1.31 review

- **No digital clipping**: without a limiter, firm playing (knobs at 3/4) went
  past 5 V in 14 of 18 models and the DAC clipped hard (up to 27 V digital with
  overpressure). Now:
  - a **dynamic curve** per player (HAESE P(D)): above normal playing the level
    grows with exponent 0.6 (the tone still gets fully brighter / grippier),
    below normal untouched;
  - output gain 0.62 (normal ~1.5 ... 3.7 V, firm ~2.5 ... 4.5 V);
    overpressure make-up 1.6 / 2.0 (was 2.3 / 3.0);
  - a **safety net** before the DAC: look-ahead limiter (1.3 ms, peak hold) that
    only moves the gain, never clips; ceiling **+-4.98 V**. Measured over all 18
    models, 6 pitches and the knob / 10 V gate extremes: max 4.980 V. In normal
    and firm playing it does not act (only at the extremes of overpressure and
    the swarm).
- **Glide at control rate**: v1.31 computed log2f + sqrtf per sample per
  player; now once per control cycle (and the cello / bass finger has a little
  more weight). Legato phrase in a section: back to the v1.20 cost (host).
- **Release**: the bow starts to slow down with a ~20 ms ramp instead of a kink.
  (The "release tick" from the review was a measurement error: the Helmholtz
  corner has the same high-frequency peaks all through the sustain.)
- **Flush-to-zero** enabled on the FPU at start-up: decaying filters can never
  slow the audio interrupt with denormals.
- The host test uses the firmware's output gain (was 0.60 vs 0.72) and checks
  the output ceiling, determinism and the phrase analysis.
- Workflows run on every `himalia-strings-v*-beta` branch.

## Preserved

CV pitch / gate authoritative, no quantising, CV scaling unchanged; adaptive
glide, vibrato and tremolo of v1.29-v1.31 (no BPM lock); the natural attack /
release of v1.31.

The output voltage and CPU load still need to be checked on the module
(diagnose mode: hold TRIGGER at power-up).
