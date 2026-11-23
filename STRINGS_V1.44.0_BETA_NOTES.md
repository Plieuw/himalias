# Himalia Strings v1.44.0-beta: ENVELOPE OUT

**Square-uitgang 6 is nu ENVELOPE OUT.** De andere square-uitgangen blijven gewoon spelers 1-5.

**Wat de uitgang volgt**
- De envelope van SAMPLE OUT: aanzet ~3 ms, loslaten ~120 ms.
- Daar zit de hele uitvoering in: velocity, aftertouch, frasering en het uitklinken.
- 0 V = stil, maximaal = luid spelen.

**Hoe het werkt**
- De firmware stuurt de envelope als 1-bit sigma-delta-stroom (47,6 kHz) naar de uitgang.
- De eigen filtercondensator van die uitgang (de oude SQUARES FILTER-schakeling, PA13) staat aan en maakt er een CV van.

**Nog onbekend**
- De waarde van die condensator. Hoe glad de CV wordt (rimpel) en de maximale spanning moeten op de module blijken.
- Voor een VCA of filter-modulatie is dit een gewone envelope.
