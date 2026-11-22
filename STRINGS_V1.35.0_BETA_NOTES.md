# Himalia Strings v1.35.0-beta

## Nieuwe modellen: 2 solo + 5 ensembles, elk in 3 speelkarakters
MODEL (RATCHET) heeft 21 standen: 7 instrumenten met elk 3 karakters.
Een klein stukje draaien geeft een ander karakter. Verder draaien geeft een ander instrument.

| Paneel | Instrument |
|---|---|
| VL | solo viool |
| VC | solo cello |
| VL+ | violen (sectie) |
| VA+ | altviolen (sectie) |
| VC+ | celli (sectie) |
| LOW | celli + bassen |
| TUT | volledig strijkorkest |

Per instrument, in deze volgorde:
1. **Klassiek**: strak en beheerst. Minder vibrato, korte glijders, een kleine spanningsboog en een strak ensemble.
2. **Romantisch**: zingend. Meer en trager vibrato, langzamere glijders, een zachtere aanzet, diepere frasering en een losser ensemble.
3. **Film**: breed. Lange noten zwellen aan (over ongeveer 1/3 van de verwachte nootlengte, korte noten niet), met extra gewicht en accent op de zware tellen en een grote boog over 8 frases.

Het karakter stuurt de adaptive playing (HAESE): hoe de speler de frase leest.

## Wat er verdwijnt
- Solo altviool, molto vibrato, romantic cello, tremolo- en trillersectie, de filmgebaren (shimmer, swell, heroic, ostinato, suspense), overpressure en Threnody.
- Tremolo blijft beschikbaar via de TREM-knop.

## Hoe de knoppen nu werken
- ARTIC is altijd articulatie. Er zijn geen modellen met een vaste articulatie meer.
- SPREAD is op een solist de expressiediepte (keer het karakter) en op een ensemble de layout.

## Glijden en legato zoals op een echte snaar
- Een slide kan alleen over **één snaar** en hooguit een octaaf. Ligt de nieuwe noot onder de lege snaar waar de vinger op staat, of verder dan een octaaf, dan kruist de speler naar de andere snaar. De toon verspringt dan direct (met een zachte vingertik), zonder glijden over snaren heen.
- Omhoog blijft de speler op dezelfde snaar zolang dat kan (opschuiven in positie). Pas als het niet meer gaat, wisselt hij van snaar.
- **Legato** binnen de hand (tot een grote terts): de vinger valt, ongeveer 10 ms, geen hoorbare glijder.
- **Legato met positiewissel** (een kwart of meer op dezelfde snaar): een korte, lichte glijder van 35-80 ms. Romantisch maakt die hoorbaarder, klassiek korter.
- **Portamento**: een bewuste glijder van 0,1-0,25 s, langer bij grotere intervallen. Ook alleen op één snaar.

## Overig
- Alle 21 modellen zijn even luid gemaakt: ze zijn gemeten met frases en daarna gelijkgetrokken.
