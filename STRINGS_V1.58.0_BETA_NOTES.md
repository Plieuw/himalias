# Himalia Strings v1.58.0-beta

Everything stays inside the existing framework and the existing CV control: no
new controls, no changed knob behaviour. Player 1 (the leader, and the whole
soloist) is untouched -- its sound is what v1.57 made, sample for sample.

## A section is six different instruments now

- **Own brightness per player** (+-1 dB above ~5 kHz): every player has a
  slightly different bridge / body balance. It rides on a sum that is already
  there, so it costs nothing per sample.
- **Own amount of rosin noise** (+-15 %).

Both come from the personality a player already had (timing / legato
tendency), so nothing is drawn twice and the humanising stays deterministic.

## A fuller body

One more body resonance per family, in the gap between the wood modes and the
bridge hill (the transition hill): violin 1500 Hz, viola 1200 Hz, cello 650 Hz,
bass 420 Hz (Q 6 ... 8, gain 0.26 ... 0.30).

## A cheaper engine

The audio interrupt runs from RAM since v1.53, so the engine is built with -O2
instead of -Os; one divide per sample instead of one per player; the
half/full-rate choice is hoisted out of the player loop; a soloist no longer
keeps the doubler history.

## Measured (tests/m4bench, instructions per sample, Cortex-M4F)

| model | v1.57 | v1.58 |
|---|---|---|
| solo violin | 1505 | 1335 |
| violin section | 2368 | 2154 |
| full strings | 2696 | 2506 |

Host tests: `strings_engine_smoke` passes (0 failures); `strings_level_report`
peaks <= 0.996. Every model moved +0.2 ... +0.4 dB -- uniform, so the balance
between the models is unchanged.

## Known

- The full ensemble now sits at 2506 of the ~2520 instruction budget per sample
  (99 %): the CPU guard has little headroom left for extra ffs. When that is
  too tight, dropping the new mode for one family or lowering its gain brings
  it back.
- The new sound was checked in simulation and in the measurements above, not by
  ear on the module (the v1.56 / v1.57 noise fixes were also checked in
  simulation only). If the extra body is too much, lowering its gain
  (0.30 -> 0.20) halves its contribution.

Install: double-tap reset, copy the .uf2 onto the HIMALIA drive.
