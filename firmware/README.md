Ready-to-flash firmware. Put the module in update mode (double-tap reset, or
open the USB serial port at 1200 baud) and copy one .uf2 to the HIMALIA drive.

- `Himalia_v0.8.0-beta.uf2`      - wavetable, LPG + 6-fold wavefolder, hard sync in, sequencer + pitch CV + trigger out (beta, current)
- `Himalia_v0.2.0-beta.uf2`      - v0.1 + generative sequencer on the squares (beta)
- `Himalia_v0.1.0-beta.uf2`      - 8 tables x 64 waves + LPG mode on the BIT switch (beta)
- `Himalia_wavetables_16x16.uf2` - 16 tables x 16 waves (previous version)
- `Himalia_samples.uf2`    - original sample player, with the fixes
- `../Himalia_BODFIX.uf2`  - factory firmware
- `Himalia_QSPI_test.uf2`        - test only: checks the 16 MB QSPI flash chip, report on the USB serial port (115200 baud)
