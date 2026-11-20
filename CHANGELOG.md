# Changelog

## v0.5.1-beta

- **Sequencer-CV volgt de squares main tune-knop:** helemaal links begint de laagste noot op -5 V. Rechtsom schuift de hele melodie omhoog, tot de hoogste noot +5,1 V bereikt (het maximum van de uitgang). Er wordt dus nooit iets afgekapt. Een CV op de main tune-ingang telt mee.

## v0.5.0-beta

- **Sequencer pitch-CV-uitgang** op de DAC-uitgang van de noise-kant: de melodienoot als 1 V/oct, laagste noot = 0 V, alleen positieve spanning (0 tot ~2 V). Patch hem bijvoorbeeld in sample tune CV, dan speelt de wavetable de sequence mee.
- De delay is daardoor mono: beide ping-pong-kanten gaan naar sample out.

## v0.4.1-beta

- **Delay rechts klinkt schoner:** de echo's worden geïnterpoleerd in plaats van getrapt (geen metaalachtige spiegeltonen meer), kleine tempo-schommelingen van de klok (< 2%) veranderen de delaytijd niet meer (geen jankende toonhoogte), en er lekt geen linker echo meer naar rechts.

## v0.4.0-beta

- **Wavetables nagelopen:** (bijna) stille frames worden overgeslagen, plotseling zachte frames worden opgehaald en frames die in fase omgedraaid zijn worden teruggedraaid. Scannen heeft geen plotselinge gaten meer; geleidelijke verlopen blijven.
- **CV in sample tune en sample select zijn bipolair: -10 tot +10 V.** Tune: 1 V/oct bovenop de knop. Select: ±10 V scant de hele tabel. De ijking is berekend uit het schema; met de seriële monitor (115200 baud) zie je de ruwe waarden om bij te stellen.

## v0.3.1-beta

- **CV in rate (noise-kant) is nu de klokingang van de delay.** Gate/trigger, hoog boven ~2V. Niet ingeplugd, of 4 s geen pulsen: de delay volgt de sample-trigger.
- Feedback staat vast op 45%.

## v0.3.0-beta

**Digitale noise → stereo ping-pong delay** op de wavetable/LPG-stem
| control / jack | functie |
|---|---|
| Sample trigger in | klok: delaytijd volgt de tijd tussen triggers |
| Clock rate-knop | deling: 1/8 1/6 1/4 1/3 3/8 1/2 2/3 3/4 1 4/3 3/2 2 |
| CV in rate | feedback (niet ingeplugd: 45%) |
| Sample out | links: droog + echo's |
| DAC-uitgang noise-kant | rechts: echo's (ping-pong) |
| Rate-ledje | knippert op elke echo |

Max 2 s per kant; langere tijden worden gehalveerd. De analoge noise blijft zoals hij was.

## v0.2.0-beta

**Squares: generatieve sequencer** (vervangt de spread-presets)
| control | functie |
|---|---|
| Sample trigger in / knop | klok: elke trigger is één stap (slaat ook de LPG aan) |
| Squares main tune + CV | grondtoon |
| Spread-knop + CV | verandering: links = loop vast, rechts = elke stap nieuw |
| A/B-knop (squares) | toonladder: A = mineur pentatonisch, B = natuurlijk mineur |

6 stemmen: melodie, sub-octaaf, twee harmonieën, octaaf hoger en een drone. Per stap bepaalt het register welke stemmen klinken, hun pulsbreedte en welke filtercondensatoren meedoen.

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
