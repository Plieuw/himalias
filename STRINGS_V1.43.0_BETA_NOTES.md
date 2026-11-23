# Himalia Strings v1.43.0-beta: Eurorack-vriendelijker (rest van de brainstorm)

## DIV TRIG volgt de frasering
- 5 ms op elke zware tel (afstreek).
- 25 ms aan het eind van een frase.
- Er komt geen puls meer bij een divisi-wissel.

## Gate-LED
- De LED laat de noot zien zoals de speler hem speelt, ook vastgehouden triggers.
- Korte streken (staccato, spiccato, pizzicato, col legno) flitsen alleen kort.

## Zachte modelwissel
- Draai je tijdens het spelen aan MODEL, dan fadet het geluid ~5 ms uit, wisselt het model en fadet het ~10 ms weer in.
- Daardoor geen klik.

## 1V/OCT-kalibratie, bewaard in flash
1. Houd TRIGGER ingedrukt en zet A/B aan tijdens het aanzetten.
2. De CLK-LED knippert langzaam: zet precies **1 V** op 1V/OCT en druk op TRIGGER.
3. De CLK-LED knippert snel: zet precies **3 V** op 1V/OCT en druk op TRIGGER.
4. Uitslag:
   - de gate-LED brandt 1 s: opgeslagen;
   - beide LED's flikkeren: de meting valt buiten bereik en er is niets opgeslagen.

Daarna start de module normaal, met de quantizer aan.

- De kalibratie geldt alleen voor 1V/OCT, ook voor pitch-FM.
- Zonder geldige kalibratie gebruikt de module de vaste waarden uit het schema.
- De kalibratie staat in het laatste flash-blok. Een firmware-update laat hem staan.

## CPU-bewaking
- Ensembles mogen nu terug naar **2** spelers (was 3). Een test op een geëmuleerde Cortex-M4 laat zien dat 3 spelers al krap passen.
- De bewaking reageert alleen op aanhoudende belasting, niet meer op de ene drukke tick bij een nieuwe noot. Eerder zette elke nieuwe noot een speler uit.
- Nieuw in de repo: `tests/m4bench/run.sh` draait de engine op een geëmuleerde Cortex-M4F en telt de instructies per sample.

## Bewust niet gedaan
- **CV-bereiken:** min- en max-waarden blijven zoals ze zijn.
- **Pitch- of envelope-uitgang:** er is geen vrije analoge uitgang.
- **Knoppen onthouden:** de potmeters zijn absoluut, dus er valt niets te onthouden.
- **Gedeelde klankkast per sectie:** op jouw keuze. Elke speler blijft apart, met minder spelers als de CPU vol is.
