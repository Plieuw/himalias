# Himalia Strings v1.41.0-beta: audio-rate op volle snelheid

**Bemonstering van de audio-ingangen**
- 1V/OCT en VELOCITY worden nu op **47,6 kHz** gelezen, elke tick. In v1.40 was dat 23,8 kHz, om de tick.
- Daarmee is FM bruikbaar tot ongeveer **20 kHz** (was ~11,9 kHz).

**De ADC**
- De ADC loopt nu op 12 MHz (was 1,5 MHz).
- De meettijd per waarde blijft **4 µs, net als voorheen**. Daardoor blijven de nauwkeurigheid en de **min/max-waarden van alle knoppen en CV's gelijk**.
- Per meting worden 2 metingen gemiddeld. Dat geeft iets minder ruis en dezelfde 12-bit schaal.

**Timing in de audio-interrupt**
- Elke meting start aan het begin van de audio-interrupt, die door de timer wordt aangestuurd. Er is dus geen timing-jitter.
- Een meting duurt ongeveer 10 µs en past ruim in één tick van 21 µs.

**Knoppen en CV's**
- Om de 8 ticks wordt in plaats van de audio-ingang een knop of CV gelezen.
  - ADC0: ongeveer 1,2 kHz per knop.
  - ADC1: ongeveer 2 kHz per knop.
- Die ene audio-waarde wordt dan geschat uit de twee vorige.
