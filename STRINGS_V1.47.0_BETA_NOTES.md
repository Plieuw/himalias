# Himalia Strings v1.47.0-beta: secties die passen

Volgens de Cortex-M4-meting kan de chip ~2–3 volledige strijkers per sample doorrekenen. Een ensemble van 6 echte spelers paste nooit. Deze versie lost dat op twee manieren op.

## 1. Lichtere spelers (−20 % per speler)
- Alles wat langzaam verandert, wordt eens per 4 samples (84 µs) berekend: strijkenvelop, glijden, vibrato, frase-energie, streekwissels en tremolo.
- Snaarlengte en strijksnelheid lopen daartussen per sample lineair mee, dus er stapt niets.
- De snaar zelf (vertragingslijnen, wrijving, ruis) rekent nog elke sample.
- Klank vergeleken met v1.46: per frequentieband binnen ±0,6 dB.

## 2. Dubbelaars
- De CPU-bewaking bepaalt hoeveel spelers volledig worden doorgerekend. Bij het opstarten zijn dat er 3; de bewaking past het aan.
- De andere spelers zijn **dubbelaars**: een vertraagde kopie van een echte speler van dezelfde familie, met:
  - eigen timing (3–14 ms, naar de persoonlijkheid van de speler);
  - een licht zwevende vertraging, wat een paar cent intonatieverschil geeft;
  - eigen volume (humanize);
  - een eigen plek in het stereobeeld.
- Welke spelers echt zijn, gaat op belang: in het orkest eerst van elke familie één (viool I, cello, bas, dan altviool).
- Meer echte spelers komt er alleen bij bij een nieuwe noot. Minder kan direct, omdat de CPU dat vraagt.
- In een ensemble deelt speler 1 nu de klankkast en de meeklinkende snaren van zijn sectie. Een solist houdt zijn eigen klankkast.

## Gemeten (geëmuleerde Cortex-M4F, instructies per sample)
| | v1.45 | v1.47 |
|---|---|---|
| Solist | 1.481 | 1.298 |
| Sectie, 6 echte spelers | 3.917 | 2.796 |
| Sectie, 3 echt + 3 dubbelaars | — | ~2.000 |
| Sectie, 2 echt + 4 dubbelaars | — | ~1.700 |

## Overig
- Alle 21 modellen zijn opnieuw even luid gemaakt, op het stereogemiddelde.
- Diagnose-modus: de CLK-LED en DIV TRIG branden zolang er dubbelaars spelen.
- RAM: +25 KB voor de geschiedenis van de spelers (21 ms per speler). Totaal ~175 KB van 256 KB.
