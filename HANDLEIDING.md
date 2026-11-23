# Himalia Strings: handleiding (v1.47)

Firmware voor de IO Instruments Himalia: een fysiek model van gestreken snaren, van solist tot strijkorkest. Elke frase die je erin stuurt, wordt als een uitvoering gespeeld.

De knoppen en jacks zitten op hun vertrouwde plek. Hieronder staan ze in **speelvolgorde**.

---

## 1. Wat speelt er

| Bediening | Functie |
|---|---|
| **MODEL** (RATCHET) | 21 standen: 7 instrumenten × 3 speelkarakters (zie tabel hieronder). Een klein stukje draaien = ander karakter, verder draaien = ander instrument. Tijdens het spelen wisselen fadet zacht over (~15 ms). |
| **PITCH** (SAMPLE TUNE) + **1V/OCT** | De knop bestrijkt het echte bereik van het instrument (links = laagste losse snaar). De CV werkt in 1 V/oct. De quantizer laat de toon op een halve toon landen (zie §5). |
| **SPREAD** + CV | Bij een ensemble: de layout (unisono, octaven, kwinten, groot, klein, divisi groot/klein, cluster). Bij een solist: hoe expressief de speler is (links neutraal, midden natuurlijk, rechts heel expressief). |

**Instrumenten** (paneellabel):
- VL: solo viool
- VC: solo cello
- VL+: violen
- VA+: altviolen
- VC+: celli
- LOW: celli + bassen
- TUT: volledig strijkorkest

**Karakters** (per instrument, in deze volgorde):
1. **Klassiek**: strak en beheerst.
2. **Romantisch**: zingend, meer vibrato en hoorbare glijders.
3. **Film**: breed, lange noten zwellen aan, gewicht op de zware tellen.

## 2. Hoe het gespeeld wordt

| Bediening | Functie |
|---|---|
| **ARTIC** (SQUARES MAIN TUNE) + CV | 12 standen (zie hieronder). |
| **ZONE** (SAMPLE SELECT) + CV | Contactpunt en kracht samen: links bij de toets (zacht, donker), midden normaal, rechts bij de kam (luid, helder). CV: 1 V = 10 %. |
| **BOW SPEED** (CLOCK RATE) | Strijksnelheid, ~9 … 25 … 70 cm/s. |

**De 12 ARTIC-standen**, van lang naar kort en daarna van zacht naar hard:
- AUTO
- portamento
- legato
- détaché
- martelé
- staccato
- spiccato
- pizzicato
- flageolet
- sul tasto
- sul ponticello
- col legno

**AUTO** kiest de streek zoals een speler, op basis van gatelengte en tempo:

| Wat je stuurt | Streek |
|---|---|
| Gebonden of (bijna) volle gates | legato, ook in snelle loopjes |
| Heel snelle noten (< 0,11 s) | spiccato/sautillé |
| Korte gates in een snel tempo | spiccato |
| Korte gates in een rustig tempo | staccato |
| Kort of normaal op een zware tel | martelé |
| Overig | détaché |

Op de standen détaché tot en met spiccato schuift de streek mee met de gatelengte. Een gepatchte ARTIC-CV kiest precies die ene articulatie (1 V = 1 stap).

**Glijden en legato** gaan zoals op een echte snaar:
- Glijden kan alleen over één snaar, hooguit een octaaf. Anders steekt de speler over naar een andere snaar.
- Legato binnen de hand is een vingerval van ~10 ms.
- Een positiewissel is een korte, lichte glijder.
- Portamento is een bewuste glijder van 0,1–0,25 s.

## 3. De uitvoering

| Bediening | Functie |
|---|---|
| **GATE** in / knop | Streek, pluk of slag. Een puls korter dan 8 ms is een aanslag zonder lengte: de noot klinkt ~80 % van de verwachte nootlengte. |
| **VELOCITY** (CV IN RATE) | Velocity, 0–5 V of 0–10 V (het bereik past zich aan). Mét GATE: alleen velocity, ~24 dB bereik, nooit stil. Hard is ook helderder, met een accent en meer vibrato. Tijdens de noot = aftertouch: verder aandrukken geeft meer strijkgewicht en vibrato. Zonder GATE: dynamische gate (de CV is de stok). |
| **A/B** (squares-kant) | Vibrato aan/uit. |
| **BIT** | Tremolo. |
| **A/B** (sample-kant) | Con sordino (demper). |

**Audio-rate.** Een audiosignaal vanaf ~40 Hz op 1V/OCT of VELOCITY wordt herkend en geeft FM:
- 1V/OCT: pitch-FM, 1 V = een octaaf.
- VELOCITY: strijkstok-FM, met grom, kraak en zijbanden.

Gewone CV en sequencer-stappen werken zoals altijd. De ingangen worden op 47,6 kHz gelezen, dus FM is bruikbaar tot ~20 kHz.

**Wat de speler zelf doet (adaptive playing):**
- frasering en een lange spanningsboog over 8 frases;
- zware tellen op een afstreek;
- de stok verdeeld over de verwachte nootlengte;
- vibrato dat reageert op tempo en sprongen;
- harde noten krijgen een accent;
- versnellen drijft de lijn op, vertragen verbreedt hem.

In ensembles heeft elke speler een eigen timing en volume. De concertmeester zet in en het gemiddelde niveau blijft gelijk.

## 4. Uitgangen

| Uitgang | Functie |
|---|---|
| **SAMPLE OUT** | Stereo links, max ±4,98 V. Voor mono: gebruik deze. |
| **DIVISI OUT** | Stereo rechts, max ±4,98 V. |
| **DIV TRIG** | 5 ms op elke zware tel, 25 ms aan het eind van een frase. |
| **Squares 1–5** | Spelers 1–5 als blokgolf zolang ze klinken. |
| **Square 6** | ENVELOPE OUT: volgt de luidheid van de strings, 0 V = stil. |
| **LED CLK** | Knippert op de herkende tel. |
| **LED gate** | De noot zoals hij gespeeld wordt. Korte streken flitsen alleen. |

**Stereo-opstelling:**
- Orkest: violen links, altviolen midden, celli en bassen rechts.
- Een sectie: verspreid rond het midden.
- Een solist: in het midden.

## 5. Bij het aanzetten

| Wat je vasthoudt of instelt | Wat er gebeurt |
|---|---|
| Niets | Normaal spelen, quantizer aan. |
| **A/B (demper) aan** | Quantizer uit: vrij of microtonaal. |
| **TRIGGER ingedrukt** | Diagnose: DIVISI OUT = CPU-belasting (0 … +5 V = 0 … 100 %). CLK-LED en DIV TRIG gaan aan zolang er dubbelaars spelen. |
| **TRIGGER ingedrukt + A/B aan** | 1V/OCT-kalibratie (zie hieronder). |

**1V/OCT-kalibreren:**
1. Zet bij het aanzetten A/B aan en houd TRIGGER ingedrukt.
2. De CLK-LED knippert langzaam: zet 1 V op 1V/OCT en druk op TRIGGER.
3. De CLK-LED knippert snel: zet 3 V op 1V/OCT en druk op TRIGGER.
4. Brandt de gate-LED 1 s, dan is de kalibratie opgeslagen. Flikkeren beide LED's, dan viel de meting buiten bereik.

De kalibratie blijft bewaard, ook na een firmware-update.

## 6. Grenzen

- **Rekenkracht en dubbelaars.** De chip kan 2–3 volledige strijkers per sample doorrekenen. In een ensemble rekent hij er zoveel volledig door als passen. Welke dat zijn, kiest hij op belang: in het orkest eerst van elke familie één.
  - De andere spelers worden **dubbelaars**: een vertraagde kopie van een echte speler van dezelfde familie, met eigen timing (3–14 ms), een licht zwevende intonatie, eigen volume en een eigen plek in het stereobeeld.
  - Je hoort dus altijd het hele ensemble.
  - Akkoord-layouts houden de tonen van de echte spelers; de dubbelaars verdubbelen die.
  - In het orkest kan de altviool wegvallen als er maar 3 echte spelers passen.
  - Gemeten op een geëmuleerde Cortex-M4 (`tests/m4bench`): een sectie met 2 echte spelers en 4 dubbelaars kost ~1.700 instructies per sample, met 3 echte ~2.000. Dat was ~3.900 met 6 echte spelers in v1.45.
- **ENVELOPE OUT.** Hoe glad deze CV is, hangt af van de filtercondensator op de uitgang. Die waarde is onbekend.
- **Ruissectie.** De analoge ruissectie en de NOISE TRIG-jack zijn alleen hardware. De firmware leest ze niet.
