# Himalia Strings v1.37.0-beta

De adaptive playing gaat slimmer om met articulatie en tempo.

## Articulatie kiezen zoals een speler (AUTO en détaché t/m spiccato)
| Wat er binnenkomt | Streek |
|---|---|
| Gebonden of (bijna) volle gates | legato, ook in snelle loopjes (gebonden) |
| Heel snelle noten (< 0,11 s) | spiccato/sautillé: de stok springt vanzelf |
| Korte gates in een snel tempo (< 0,28 s per noot) | spiccato |
| Korte gates in een rustig tempo | staccato (de stok stopt) |
| Kort/normaal op een zware tel of frase-aanzet (niet snel) | martelé: de stok bijt |
| Overig | détaché |

- "Gebonden" is nu relatief aan het tempo. Een gat van 40 ms tussen noten van 0,1 s geldt niet meer als gebonden.

## Tempo
- **Stiltes**: een rust is nu een stilte die duidelijk langer is dan de gewone stilte tussen de noten. Bij staccato begon eerder elke noot een nieuwe frase; nu vormen ze één frase.
- **Frase-einde**: in een herkend patroon weet de speler welke noot voor een rust valt. Die noot wordt iets teruggenomen en krijgt wat meer vibrato. De stok tilt op en laat de snaar uitklinken, ongeveer 1,6× langer.
- **Aanzet**: snelle noten worden scherper aangezet, langzame geplaatst.
- **Vibrato**: hoe lang de toon recht blijft en hoe snel het vibrato opbouwt, hangt af van de nootlengte. Een korte noot krijgt meteen een klein vibrato, een lange noot bouwt het rustig op (0,3-1,4 s).
- **Sectie**: in snelle passages speelt een sectie strakker samen.
