# Himalia Strings v1.42.0-beta: quantizer en triggers

## Quantizer, zoals een speler de noot vindt
- Zolang de toonhoogte beweegt, volgt hij vrij. Dat geldt voor een glijder, een geslewde CV en aan de knop draaien.
- Staat de toonhoogte ongeveer 15 ms stil, dan schuift de intonatie in ongeveer 20 ms naar de dichtstbijzijnde halve toon (A = 440 Hz).
- Een nauwkeurige sequencer-CV hoor je niet veranderen. Een ongequantiseerde bron (LFO, random) speelt zuivere noten.
- **Uitzetten:** zet de A/B-schakelaar (mute) aan tijdens het opstarten. Daarna speelt de module vrij of microtonaal.

## Triggers als noot
- Een gate-puls korter dan 8 ms is een aanslag zonder lengte.
- De noot wordt dan aangehouden voor ongeveer 80 % van de verwachte nootlengte (uit het ritme).
- De streek volgt het tempo. Rustig gespeeld wordt het détaché, snel (< 0,11 s per noot) wordt het spiccato.
- Een nieuwe trigger tijdens het aanhouden zet direct de volgende noot in.

## Wat gelijk blijft
- CV-bereiken en de min/max-waarden zijn niet veranderd.
