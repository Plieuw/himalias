# Himalia Strings v1.40.0-beta: audio-rate modulatie

## Wat het doet
**1V/OCT → pitch-FM.** Een audio-rate signaal op de pitch-ingang moduleert de snaarlengte per sample (1 V = een octaaf).
- Weinig modulatie geeft metalige, klokachtige tonen.
- Meer modulatie breekt de gestreken toon op.

**VELOCITY → strijkstok-FM.** De strijkstok wordt op audio-rate geduwd en gedrukt:
- de strijksnelheid gaat door nul (de stok keert om);
- de stokdruk varieert;
- het contactpunt beweegt.

Dat geeft grom, kraak en zijbanden. Rond 1 V is het subtiel, bij 2,5 V rauw. Vanaf ongeveer 5 V breekt de toon.

## Automatische herkenning
De firmware herkent zelf of er audio-rate binnenkomt: minstens 4 nuldoorgangen in 50 ms, vanaf ongeveer 40 Hz.
- De modulatie fadet in ongeveer 10 ms in en uit.
- Gewone CV, sequencer-stappen en dynamische gates gaan langs de normale weg, precies zoals voorheen.
- Zolang FM loopt, volgen toonhoogte en velocity het trage deel van het signaal (onder ~30 Hz).

## Onder de motorkap
- De audio-interrupt bedient nu beide ADC's zelf, zonder wachten.
  - Op even ticks leest hij de twee audio-rate ingangen (23,8 kHz).
  - Op oneven ticks leest hij om de beurt alle knoppen en andere CV's (4,8–7,9 kHz per kanaal).
- `loop()` leest alleen nog de opgeslagen waarden. Hij wacht dus nooit meer op een ADC en kan de audio niet meer ophouden. Eerder blokkeerde elke ADC-lezing de loop ongeveer 13 µs.
