# Himalia Strings v1.53.0-beta

Fix: v1.41 ... v1.52 gave no sound, and from v1.46 the module could stop
showing up on USB. v1.40 works (tested by the user).

- The audio interrupt (~21 KB) now runs from RAM. From flash it did not fit
  the 4 KB flash cache, so every tick paid flash wait states: the interrupt
  ran out of time and USB / loop() starved.
- Back to the -Os build for the engine (the -O2 pragma of v1.46 is gone).
- The CPU guard starts with 2 real players (was 4), drops a player at once on
  an overrun and may go down to 1; it adds players again when there is room.
- ADC as in v1.52: the core's 12-bit settings (the v1.41 12 MHz clock with
  2x averaging needed a 16-bit result setting and gave wrong readings).
