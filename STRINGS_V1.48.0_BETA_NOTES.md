# Himalia Strings v1.48.0-beta: alle 6 spelers echt, op halve samplerate

**Andere aanpak voor ensembles.** In een ensemble draait de snaar van elke speler op de halve samplerate (23,8 kHz). De spelers wisselen elkaar af, zodat de belasting gelijk verdeeld blijft. Klankkasten, meeklinkende snaren, lucht, limiter en audio-rate FM blijven op 47,6 kHz. Een solist draait op de volle rate.

## Wat dit oplevert
Een sectie met **6 echte spelers** (elk een eigen noot, streek, vibrato en timing) kost nu ~2.160 instructies per sample.

| Versie | Instructies per sample |
|---|---|
| v1.45 | 3.917 |
| v1.47 (6 echte spelers) | 2.796 |
| v1.48 | ~2.160 |

Pieken bij een nieuwe noot: ~3.400 instructies (eerder tot 8.000).

Akkoord-layouts (groot, klein, cluster) houden alle 6 tonen. Dubbelaars springen alleen nog in als het echt niet past.

## Om de klank gelijk te houden
- **Interpolatie:** 4-punts (kubisch) terug naar 47,6 kHz. Die is vlak tot ~10 kHz en geeft bijna geen spiegelbeelden.
- **Hoge shelf per speler:** +1 dB bij 2 kHz, +5 dB bij 10 kHz.
- **Presence-piek per sectie:** +4 dB bij 2,2 kHz.
- **Toonhoogte:** gecorrigeerd voor de lusvertraging. Een sectie stemt binnen ~2 cent.

**Vergeleken met v1.47 (6 spelers op volle rate),** genormaliseerd op niveau:

| Model | 0–1 kHz | 1–4 kHz | 4–8 kHz | 8–12 kHz | 12–23 kHz |
|---|---|---|---|---|---|
| Violen | +1,5 | −0,6 | +2,5 | −1,8 | +0,7 |
| Celli | +2,4 | +1,3 | +4,8 | +1,2 | +2 |
| Orkest | +1,0 | +0,8 | +1,5 | −1,2 | −0,3 |

## Overig
- Alle modellen opnieuw even luid gemaakt.
- De CPU-bewaking start met 4 echte spelers en voegt er na 2 s rust één toe, tot 6.
