# Himalia Strings v1.57.0-beta

Fixes since v1.42 (all in this release):

- **No sound / "USB device not recognized" (v1.43 ... v1.54).** At power-up the
  firmware read the 1V/OCT calibration from the last flash block, which this
  module cannot read; it faulted before anything ran. The calibration now
  lives in a block inside the firmware. A firmware update erases it:
  calibrate again after updating (without one, the standard values are used).
- **Grain with sequenced CV (v1.40 ... v1.55).** The audio-rate FM detection
  switched on from ADC noise (hysteresis 0.02 V = ~4 counts). Now 0.15 V on
  1V/OCT, 4 % of the range on VELOCITY.
- **Noise in long notes.** A held pitch / velocity CV now reads the audio
  interrupt's ~30 Hz average instead of one raw reading (~+-15 cent pitch
  jitter, bow-force jitter). A real step (> 0.05 V pitch, > 3 % velocity)
  passes at once, so sequencer notes are not slowed.
- Knob glitches on MODEL / SPREAD (v1.41 ADC setup) gone; audio interrupt
  runs from RAM; CPU guard starts low and drops a player at once on overload.

Install: double-tap reset, copy the .uf2 onto the HIMALIA drive.
Tested on the module up to v1.55 (sound, USB); the v1.56 / v1.57 noise fixes
were checked in simulation only.
