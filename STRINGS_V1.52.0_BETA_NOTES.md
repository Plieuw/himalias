# Himalia Strings v1.52.0-beta: geen geluid na v1.34.2, waarschijnlijke oorzaak hersteld

## Melding
v1.34.2 geeft geluid, v1.51 niet.

## Waarschijnlijke oorzaak: de ADC-wijziging van v1.40/v1.41
Sinds v1.40 leest de audio-interrupt zelf de ADC's. Twee dingen konden daarbij misgaan:
1. **Wachten op een register.** De interrupt wachtte op registers die moeten synchroniseren, zonder maximale wachttijd. In de originele code staat bij ADC1 de opmerking "HÄNGT!!!" (hangt). Blijft zo'n wachtlus hangen, dan staat de audio-interrupt stil en komt er geen geluid.
2. **Middelen in v1.41.** Ik liet de ADC twee metingen middelen terwijl hij in 12-bit-modus staat. Volgens de datasheet moet de ADC voor middelen in 16-bit-modus staan, dus de metingen kunnen fout zijn geweest.

## Wat v1.52 anders doet
- **Elke wachtlus op de ADC heeft een maximale wachttijd.** De audio kan dus nooit meer blijven hangen.
- **De ADC's houden de instellingen van de core:** 1,5 MHz, 12 bit, geen middeling. Dat zijn precies dezelfde metingen als in v1.39 en eerder.
- **Audio-rate blijft:** de audio-ingangen worden nog steeds 7 van de 8 ticks gelezen. Een meting duurt ~12,7 µs en past in één tick van 21 µs.
