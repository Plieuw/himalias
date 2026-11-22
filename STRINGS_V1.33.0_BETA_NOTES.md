# Himalia Strings v1.33.0-beta — every knob does something on every model

Builds on v1.32.0. Where a model has no use for SPREAD (layout) or SQUARES
MAIN TUNE (articulation), that knob + CV gets a job that fits the model.

## SPREAD knob + CV

| model | SPREAD does |
|---|---|
| sections, full strings, trill / tremolo section, emotional swell | layout, as before |
| solo violin / viola / cello, molto vibrato, romantic cello, overpressure, and the models with a fixed layout (shimmer, heroic theme, epic ostinato) | **expression depth**: left = a neutral player (no phrase shaping, 0.4x vibrato: HAESE "neutral default"), centre = natural default, right = very expressive (2x the phrase shaping, 1.6x vibrato) |
| Threnody swarm | **swarm width**: +-0.5 ... 2 (centre) ... 3.5 semitones |
| suspense | **cluster width**: ~90 cents (almost unison, ghostly) ... 490 (centre) ... 890 cents |

## SQUARES MAIN TUNE knob + CV (models that fix their articulation)

CV 1 V = 10 % of the knob (free models keep 1 V = one articulation step).

| model | the knob does |
|---|---|
| romantic cello | slide time between notes: ~35 ms ... 70 (centre) ... ~290 ms |
| tremolo section, suspense | tremolo speed: ~6 ... 12 (centre) ... 24 strokes/s |
| trill section | trill interval: semitone / whole tone / minor third |
| epic ostinato | stroke length: 20 ... 50 (centre) ... 80 ms |
| heroic theme | accent: 0.3x ... 1x ... 1.7x |
| emotional swell, shimmer | swell time: 0.5x ... 1x ... 2x (emotional swell 0.8 ... 3.2 s) |

All values measured in the host engine. The output ceiling (+-4.98 V) and the
v1.32 player are unchanged; the host test checks the knob jobs.
