# Himalia Strings v1.46.0-beta: zuiniger en een handleiding

## Minder rekenwerk, dezelfde klank
- **De audio-engine wordt met -O2 gecompileerd** in plaats van -Os. Op een geëmuleerde Cortex-M4F (`tests/m4bench`) scheelt dat:

  | Model | Instructies per sample |
  |---|---|
  | Solist | 1.481 → 1.343 (−9 %) |
  | Sectie | 3.917 → 3.646 (−7 %) |
  | Volledig orkest | 4.117 → 3.899 (−5 %) |

  De firmware wordt daardoor ~1,4 KB groter.
- **Per speler** worden de vaste delen van de berekening (niveau, kleur, vibratodiepte, strijksnelheid) alleen nog opnieuw uitgerekend als ze veranderen. Dat gebeurt bij een noot of op control-rate, niet meer elke sample.
- **De sympathische snaren** lezen met vooraf berekende leesposities.

**Klankcontrole.** De uitkomst is wiskundig hetzelfde; alleen de afrondingen verschillen. Vergeleken met v1.45 ligt het niveau per 0,1 s gemiddeld 0,03 dB anders.

## Handleiding
- Nieuw: `HANDLEIDING.md`, de complete bediening van de huidige versie in speelvolgorde.
- Inhoud: alle knoppen, jacks, uitgangen, opstartopties, de kalibratie en de grenzen van de module.
