# Changelog

## v0.1.0-beta

Eerste beta van de wavetable-firmware voor de IO Instruments Himalia.

**Flashen:** zet de module in update-modus (twee keer snel op reset, of de USB-poort openen op 1200 baud) en kopieer `Himalia_v0.1.0-beta.uf2` naar het station HIMALIA. Terug naar fabriek: `Himalia_BODFIX.uf2` uit de repo.

**Sample-kant: wavetable-oscillator**
| control | functie |
|---|---|
| Sample tune + CV | toonhoogte (~16 Hz – 8 kHz, niet geijkt op 1V/oct) |
| Sample select + CV | scannen door 64 golfvormen |
| Ratchet-knop + CV | tabel 1–8 |
| A/B-knop | wavefolder aan/uit · in LPG-modus: korte/lange decay |
| Trigger in / knop | sync · in LPG-modus: opent de low pass gate |
| Bit-schakelaar | LPG-modus (vactrol-stijl), vervangt bytebeat |

Tabellen: 56_BookOfWaves, acid, acidvoicewt, HarmonicSeries, Special_K, Spectral_Sync_01, Unlimited_Power, WALDO1.

**Verder**
- volledige 1 MB flash van de ATSAMD51J20A in gebruik
- squares- en noise-kant ongewijzigd (spread-keuze met hysterese)

**Beta:** gebouwd en gesimuleerd, nog niet uitgebreid op hardware getest. Problemen graag als issue melden.
