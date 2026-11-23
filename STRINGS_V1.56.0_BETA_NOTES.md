# Himalia Strings v1.56.0-beta

Fixed: grain / noise when a sequencer drives 1V/OCT, gate and velocity.

The audio-rate FM detection (v1.40) switches on when a jack crosses zero
often enough. Its hysteresis on 1V/OCT was 0.02 V, about 4 ADC counts: with
normal ADC noise (2-3 counts) it switched pitch FM on almost all the time,
modulating the pitch with noise. Simulated (noise + sequencer steps): FM on
98.8 % of the time before, 0 % now; a 0.3 V audio-rate signal still turns it
on. Hysteresis now 0.15 V (1V/OCT) and 4 % of the range (VELOCITY).
Not tested on the module itself.
