# Himalia Strings: handleiding (v1.51)

Gestreken snaren als fysiek model voor de IO Instruments Himalia: van solist tot strijkorkest. Elke frase die je erin stuurt, speelt de module als een uitvoering.

Tussen haakjes staat steeds de originele opdruk van het Himalia-paneel.

---

## 1. Snel beginnen
1. Patch de **gate** van je sequencer in **GATE** (SAMPLE TRIGGER IN).
2. Patch de **toonhoogte** in **1V/OCT** (CV IN SAMPLE TUNE).
3. Verbind de uitgangen met je mixer:
   - **STRINGS** (SAMPLE OUT) naar links;
   - **DIVISI** (noise-kant DAC OUT) naar rechts.

   Mono? Gebruik alleen STRINGS.
4. Draai **MODEL** helemaal links (klassiek, solo viool) en **ARTIC** helemaal links (AUTO). Zet alle andere knoppen op 12 uur.
5. Speel. De gatelengte bepaalt de streek, het ritme de frasering.

Daarna:
- **VELOCITY** (CV IN RATE): patch hier een velocity- of accent-CV in.
- **MODEL**: kies een stijl en een instrument.

---

## 2. Het paneel
Alle knoppen werken van links naar rechts van minder naar meer: zachter → harder, donkerder → helderder.

Een CV telt op bij de knopstand. Uitzondering: SPREAD. Daar tellen knop en CV in de hardware bij elkaar op.

### Linker kolom: ingangen (van boven naar onder)
| Jack | Origineel | Functie | CV |
|---|---|---|---|
| **VELOCITY** | CV IN RATE | Hoe hard de noot is: het niveau bij de aanzet. Tijdens de noot werkt het als aftertouch. Vanaf ~40 Hz wordt het strijkstok-FM. | 0–5 of 0–10 V (automatisch) |
| **GATE** | SAMPLE TRIGGER IN | Streek, pluk of slag. De gatelengte bepaalt de articulatie. Een puls korter dan 8 ms is een aanslag zonder lengte. | gate |
| **1V/OCT** | CV IN SAMPLE TUNE | Toonhoogte, met quantizer (§5). Vanaf ~40 Hz wordt het pitch-FM. | 1 V/oct |
| **ZONE** | CV IN 16x A&B | Speelzone | 1 V = 10 % |
| **ARTIC** | CV IN TUNE SQUARES | Articulatie. Gepatcht geldt precies de gekozen stand. | 1 V = 1 stap |
| **SPREAD** | CV IN SPREAD SQUARES | Layout (bij een ensemble) of expressie (bij een solist) | opgeteld met de knop |
| *NOISE CV, NOISE TRIG* | – | Analoge ruissectie, alleen hardware: de firmware leest ze niet | – |

### Midden: knoppen (van boven naar onder)
Bovenaan zit **de strijkstok**, in het midden **de toon** en onderaan **de keuzes**.

| Knop | Origineel | Functie |
|---|---|---|
| **BOW SPEED** | CLOCK RATE | Strijksnelheid, ~9 … 25 … 70 cm/s. Hoort bij de VELOCITY-jack. |
| **GATE**-knop + LED | MANUAL TRIGGER | Handmatige gate. De LED toont de noot zoals de speler hem speelt; korte streken flitsen alleen. |
| **PITCH** | SAMPLE TUNE | Toonhoogte over het echte bereik van het instrument. Hoort bij de 1V/OCT-jack. |
| **ZONE** | SAMPLE SELECT | Links bij de toets (zacht, donker), in het midden normaal, rechts bij de kam (luid, helder). |
| **MODEL** | RATCHET | Stijl + instrument, 24 standen (§3). |
| **MUTE** | A/B (sample) | Con sordino (demper). |
| **SPREAD** | SPREAD SQUARES TUNE | Bij een ensemble de layout: unisono, octaven, kwinten, groot, klein, divisi groot, divisi klein, cluster. Bij een solist de expressie: links neutraal, rechts zeer expressief. |
| **ARTIC** | SQUARES MAIN TUNE | AUTO, portamento, legato, détaché, martelé, staccato, spiccato, pizzicato, flageolet, sul tasto, sul ponticello, col legno. |

### Rechter kolom: uitgangen en knoppen (van boven naar onder)
| Jack / knop | Origineel | Functie |
|---|---|---|
| **DIV TRIG** | DIGITAL NOISE OUT | 5 ms op elke zware tel, 25 ms aan het eind van een frase |
| **DIVISI** | noise-kant DAC OUT | Stereo **rechts**, ±4,98 V |
| **TREM** | BIT | Tremolo aan/uit |
| **STRINGS** | SAMPLE OUT | Stereo **links**, ±4,98 V. Voor mono gebruik je deze. |
| **VIB** | SQUARES FILTER | Vibrato of senza vibrato |
| **ENV** | 6x SQUARES OUT | **Envelope-CV van het ensemble**: 0 V = stil, maximaal = luid spelen. Stuur er een VCA, filter of effect mee aan met de frasering. |
| *NOISE, COMP* | – | Analoge ruissectie (hardware) |

**LED's:**
- **BEAT** knippert op de herkende tel.
- De **gate-LED** toont de gespeelde noot.

---

## 3. MODEL: stijl en instrument
De knop loopt in **stijl-blokken**, van rustig naar ritmisch:

| Blok | Instrumenten (van links naar rechts) | Speelwijze |
|---|---|---|
| **Klassiek** | VL VC VL+ VA+ VC+ LOW TUT | strak en beheerst; traditionele akkoorden |
| **Romantisch** | VL VC VL+ VA+ VC+ LOW TUT | zingend: meer vibrato, glijders, iets achter de tel |
| **Film** | VL+ VC+ LOW TUT | breed, zwellende lange noten; open akkoorden |
| **Soul** | VL+ VC+ TUT | laid back, scoops in lange noten, een val aan het frase-eind, maj9/m9-akkoorden, nadruk op 2 en 4 |
| **Funk** | VL VL+ TUT | strakke korte stabs, nadruk op 2 en 4, een val aan het frase-eind, dom7/m7-akkoorden |

| Afkorting | Instrument |
|---|---|
| VL | solo viool |
| VC | solo cello |
| VL+ | violen |
| VA+ | altviolen |
| VC+ | celli |
| LOW | altviolen + celli + bassen |
| TUT | volledig orkest |

**Stereo-opstelling:**
- Orkest: violen links, altviolen in het midden, celli en bassen rechts.
- Sectie: spelers verspreid rond het midden, elk op een eigen afstand (en dus met een eigen fase).
- Solist: in het midden.

Draai je MODEL tijdens het spelen, dan fadet het geluid zacht over.

---

## 4. Spelen
### Articulatie: AUTO
ARTIC op AUTO kiest de streek zoals een speler dat doet, op basis van gatelengte en tempo:

| Gate | Streek |
|---|---|
| Gebonden of (bijna) vol | legato |
| Heel snel (< 0,11 s) | spiccato |
| Kort in een snel tempo | spiccato |
| Kort in een rustig tempo | staccato |
| Kort of normaal op een zware tel | martelé |
| Overig | détaché |

Romantisch en soul leunen naar langere streken, funk naar kortere.

### Glijden en legato
- Glijden gaat alleen over één snaar, tot hooguit een octaaf.
- Legato binnen de hand is een vingerval; een positiewissel geeft een korte glijder.
- Portamento is een bewuste glijder.

### Velocity
- ~24 dB bereik, en nooit helemaal stil.
- Harder aanzetten klinkt helderder, met accent en meer vibrato.
- Aandrukken tijdens de noot werkt als aftertouch.

### Divisi volgt de frase
- Een frase begint met 2 partijen en opent noot voor noot.
- Een sprong naar een nieuw hoogtepunt opent extra.
- De noot voor een rust gaat naar unisono.
- Velocity opent of sluit het akkoord ook een partij.

### De speler denkt mee
- frasering en een spanningsboog over 8 frases;
- zware tellen op een afstreek;
- de stok verdeeld over de verwachte nootlengte;
- vibrato dat reageert op het tempo en op sprongen;
- in een ensemble heeft elke speler een eigen timing, volume, plek en fase, met de concertmeester voorop.

### Audio-rate
Een oscillator (vanaf ~40 Hz) op 1V/OCT geeft pitch-FM, op VELOCITY strijkstok-FM (grom, kraak). Gewone CV werkt gewoon zoals altijd.

---

## 5. Bij het aanzetten
| Instellen of vasthouden | Gevolg |
|---|---|
| niets | normaal; quantizer aan (de toon landt op halve tonen, glijders gaan vrij) |
| **MUTE aan** | quantizer uit (vrij, microtonaal) |
| **GATE-knop ingedrukt** | diagnose: de DIVISI-jack geeft de CPU-belasting (0 … +5 V = 0 … 100 %). BEAT-LED en DIV TRIG branden zolang er dubbelaars spelen. |
| **GATE-knop + MUTE aan** | kalibratie van 1V/OCT |

**Kalibratie van 1V/OCT:**
1. Zet MUTE aan, houd de GATE-knop ingedrukt en zet de module aan.
2. De BEAT-LED knippert langzaam: zet 1 V op 1V/OCT en druk op GATE.
3. De BEAT-LED knippert snel: zet 3 V op 1V/OCT en druk op GATE.
4. Brandt de gate-LED 1 seconde, dan is de kalibratie opgeslagen. Ze blijft ook na een firmware-update bewaard.

---

## 6. Grenzen
- **CPU:** past een ensemble niet helemaal in de rekentijd, dan worden de minst belangrijke spelers dubbelaars: kopieën met een eigen timing, intonatie, volume en plek. In het orkest kan de altviool dan wegvallen.
- **ENV:** de maximale spanning en hoe glad de envelope is, hangen af van de weerstanden en condensatoren op de squares-uitgang. Meet het één keer op jouw module.
- **Faceplate:** de bestaande Strings-faceplate heeft nog de oude MODEL-schaal en de oude labels.
