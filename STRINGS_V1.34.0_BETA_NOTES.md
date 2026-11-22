# Himalia Strings v1.34.1-beta

## Fix v1.34.1: VELOCITY-CV
- Velocity is nu de piek van de eerste 25 ms van de noot. Eerder werd het niveau op het moment dat de gate openging (~0,4 V) genomen, waardoor elke noot even zacht was.
- De stijgende CV werd daardoor ook als maximale aftertouch gelezen; aftertouch telt nu pas na die 25 ms, met een kleine dode zone tegen ruis.
- Een stijgende CV wordt direct gevolgd (geen trage afvlakking meer bij de aanzet).

# v1.34.0-beta

Makkelijker te bespelen, en de bediening staat in een logische volgorde.

## Bediening in speelvolgorde
1. **WAT speelt er**: MODEL, PITCH (+1V/OCT), SPREAD
2. **HOE het gespeeld wordt**: ARTIC, ZONE, BOW SPEED
3. **De UITVOERING**: GATE, VELOCITY, VIB/TREM/MUTE

De ARTIC-knop staat nu in speelvolgorde (12 standen):
- **AUTO**;
- dan van lang naar kort: portamento, legato, détaché, martelé, staccato, spiccato, pizzicato;
- dan klankkleur van zacht naar hard: flageolet, sul tasto, sul ponticello, col legno.

## Nieuw
- **Gatelengte bepaalt de articulatie.**
  - In AUTO: lang of gebonden = legato, normaal = détaché, kort = staccato, heel kort = spiccato.
  - Op détaché t/m spiccato schuift de articulatie mee rond de gekozen streek.
  - Is de ARTIC-CV gepatcht, dan geldt precies de articulatie van die CV.
- **VELOCITY (CV IN RATE) werkt als aftertouch.** Het niveau bij de aanzet bepaalt hoe hard de noot klinkt. Verder aandrukken tijdens de noot geeft meer strijkgewicht, volume en vibrato. Minder aandrukken maakt het lichter.
- **Vooruitkijken.** In een herkend ritmepatroon weet de speler hoe lang de noot wordt. Hij verdeelt de stok daarop: een lange noot begint langzamer, een korte sneller.
- **Lange spanningsboog over 8 frases.** Een herhaalde sequens klinkt de 4e keer anders dan de 1e. Dat gebeurt deterministisch.
- **Streekplanning.** Zware tellen (de maatstart of accenten) vallen op een afstreek. Daartussen wisselt de streek, met een natuurlijke "retake".
- **Ensemble.** Speler 1 (de concertmeester) zet in, de rest volgt een paar ms later. Bij een frase-aanzet ademen ze samen. De streekwissels op lange noten liggen verspreid.
- **Vibrato reageert.** Snelle passages krijgen weinig vibrato. Een zachte lange noot na een sprong omhoog krijgt juist meer.

## Niet mogelijk: DIGITAL NOISE TRIG-jack
Volgens het schema (V1.5) leest geen enkele pin van de microcontroller de ruissectie (J8–J13). De MCU-ingangen zijn J1–J7, en die zijn allemaal al in gebruik. Daarom kan de NOISE TRIG-jack geen legato- of accent-gate worden. Een gebonden noot kun je nu gewoon maken met een lange gate.
