# Himalia Strings v1.51.0-beta: logische opbouw voor de Eurorack-gebruiker

## Correctie: ENVELOPE OUT
- De zes square-pinnen komen in de hardware samen op één jack: 6x SQUARES OUT.
- In v1.44 zat de envelope op één van die zes pinnen. Daardoor kwam hij gemengd met de blokgolven uit dezelfde jack en was hij niet bruikbaar.
- Nu is die jack **ENV**, een schone envelope-CV van het ensemble.
- **Zo werkt het:** de zes pinnen samen vormen een kleine DAC met 7 stappen. De envelope bepaalt hoeveel pinnen hoog staan, een sigma-delta dithert tussen twee stappen, en alle zes filtercondensatoren staan aan. Dat geeft zesmaal minder rimpel dan met één pin.
- De envelope volgt beide uitgangen: aanzet ~3 ms, loslaten ~120 ms.
- De blokgolf-laag van de spelers vervalt. Dat scheelt ook wat CPU.

## Handleiding opnieuw opgebouwd (HANDLEIDING.md)
De volgorde volgt nu het paneel:
1. snel beginnen;
2. het paneel per kolom, met de originele Himalia-opdruk erbij:
   - links de ingangen;
   - in het midden de knoppen (boven de strijkstok, midden de toon, onder de keuzes);
   - rechts de uitgangen en knoppen;
3. MODEL (stijl-blokken);
4. spelen;
5. opstartopties;
6. grenzen.

Daarnaast:
- **Richting:** alle knoppen werken van links naar rechts van minder naar meer.
- **Stereo:** de uitgangen heten nu STRINGS (L) en DIVISI (R).
