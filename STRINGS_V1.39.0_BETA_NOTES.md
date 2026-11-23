# Himalia Strings v1.39.0-beta

Opschoning en optimalisatie. De klank en de bediening blijven gelijk: de niveaus van alle 21 modellen zijn identiek aan v1.38.

- **Ongebruikte code verwijderd:**
  - triller;
  - overpressure;
  - Threnody-zwerm en suspense-cluster;
  - de modelgebaren op de ARTIC- en SPREAD-knop;
  - de vaste ARTIC-gebaarwaarde in de sketch.
- **Minder rekenwerk per sample:** de wrijvingshelling wordt nu op controlerate voorberekend.
- **Resultaat (gemeten op de host):**
  - volledig orkest ongeveer −14 % CPU;
  - sectie ongeveer −7 %;
  - firmware ruim 1 KB kleiner.
