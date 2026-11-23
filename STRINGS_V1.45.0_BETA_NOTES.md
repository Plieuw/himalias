# Himalia Strings v1.45.0-beta: stereo in orkestopstelling

**De twee audio-uitgangen zijn nu een stereopaar.**
- SAMPLE OUT is links.
- DIVISI OUT is rechts.

**Opstelling per model**
- **Volledig strijkorkest:** violen links, altviolen midden, celli en bassen rechts.
- **Celli + bassen:** celli links van het midden, bassen rechts.
- **Een sectie** (violen, altviolen, celli): de spelers verdeeld van links naar rechts, rond het midden.
- **Solist:** in het midden. Beide uitgangen geven precies hetzelfde signaal.

**Niveau**
- Beide uitgangen zijn even luid geschaald.
- Elke uitgang heeft dezelfde limiter van 4,98 V.
- Wat in het midden staat, klinkt op beide uitgangen even luid, met hetzelfde niveau als voorheen.

**Mono**
- Gebruik SAMPLE OUT. De rechterkant (celli en bassen in het orkest) klinkt daar zachter.

**Breedte zonder extra rekenwerk**
- Elke speler heeft een eigen plek binnen zijn sectie. Dat komt als zijsignaal per familie bij de klankkast.
- Er komen geen extra klankkasten per speler, dus de rekenlast blijft praktisch gelijk.

**ENVELOPE OUT**
- Volgt nu beide kanten samen.

**Diagnose-modus**
- Ongewijzigd: DIVISI OUT toont dan de CPU-belasting.
