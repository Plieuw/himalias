Ready-to-flash firmware. A .uf2 must be smaller than 1,961,472 bytes, otherwise
it does not fit on the bootloader's HIMALIA drive. Put the module in update mode (double-tap reset, or
open the USB serial port at 1200 baud) and copy one .uf2 to the HIMALIA drive.

- `Himalia_v1.0.0-beta.uf2`      - resonator models (string, bar, bell/glass, drum, wind), external excitation, folder, sequencer (beta, current)
- `Himalia_v0.2.0-beta.uf2`      - v0.1 + generative sequencer on the squares (beta)
- `Himalia_v0.1.0-beta.uf2`      - 8 tables x 64 waves + LPG mode on the BIT switch (beta)
- `Himalia_samples.uf2`    - original sample player, with the fixes
- `../Himalia_BODFIX.uf2`  - factory firmware
- `Himalia_QSPI_test.uf2`        - test only: checks the 16 MB QSPI flash chip, report on the USB serial port (115200 baud)
- `Himalia_strings_v1.25.0-beta.uf2` - bowed strings, 18 models in four groups (instruments, playing styles, film-score gestures, experimental), flat level over three octaves, per-model peak calibration without a limiter, realistic note off, sympathetic resonance, analog CPU diagnose mode, knobs in real terms, playing-zone knob, beat LED, noise transients, dynamic gates, ostinato tempo detection, realistic vibrato with finger noises, air, tempo-coupled bowing with bow changes, slip noise, 11 articulations, tempo sync, divisi outputs, Eurorack level (branch himalia-strings-beta), see STRINGS_BETA.md
