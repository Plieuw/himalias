// Relative loudness and peak report for all Himalia Strings models.
// Host-side measurement only; no firmware behavior is changed.
#define STR_OUT_GAIN 0.60f
#include "../IOCore/HimaliaSketch/strings_engine.h"
#include <cmath>
#include <cstdio>

static StringsEngine s;
struct Level { double rms; double peak; };

static Level renderLevel(int model, float hz) {
  stringsInit(s);
  s.instrument = (uint8_t)instrument;
  s.rootHz = hz;
  s.speed = 0.6f;
  s.pressure = 0.6f;
  s.beta = 0.11f;
  stringsControl(s);
  const bool shortStroke = STR_ARTIC[s.articEff].stroke > 0.0f;
  const unsigned count = 60000;
  const unsigned from = shortStroke ? 0 : 20000;
  const unsigned to = shortStroke ? 8000 : 60000;
  double sum = 0.0;
  double peak = 0.0;
  for (unsigned n = 0; n < count; ++n) {
    if ((n & 127) == 0) stringsControl(s);
    const float y = stringsTick(s, true);
    if (n >= from && n < to) {
      sum += (double)y * y;
      const double a = std::fabs((double)y);
      if (a > peak) peak = a;
    }
  }
  Level result = { std::sqrt(sum / (to - from)), peak };
  return result;
}

int main() {
  const float notes[] = {130.81f, 261.6f, 440.0f};
  for (int model = 0; model < MDL_COUNT; ++model) {
    double total = 0.0;
    double peak = 0.0;
    for (unsigned n = 0; n < sizeof(notes) / sizeof(notes[0]); ++n) {
      const Level level = renderLevel(model, notes[n]);
      total += level.rms;
      if (level.peak > peak) peak = level.peak;
    }
    const char *name =
      model == 0 ? "solo_violin" : model == 1 ? "solo_viola" :
      model == 2 ? "solo_cello" : model == 3 ? "violin_section" :
      model == 4 ? "viola_section" : model == 5 ? "cello_section" :
      model == 6 ? "full_strings" : model == 7 ? "molto_vibrato" :
      model == 8 ? "romantic_cello" : model == 9 ? "tremolo_section" :
      model == 10 ? "trill_section" : model == 11 ? "shimmer" :
      model == 12 ? "emotional_swell" : model == 13 ? "heroic" :
      model == 14 ? "epic_ostinato" : model == 15 ? "suspense" :
      model == 16 ? "overpressure" : "threnody";
    if (peak > 0.995) {
      std::fprintf(stderr, "FAIL model=%02d peak=%.6f exceeds calibrated headroom\n", model, peak);
      return 1;
    }
    std::printf("LEVEL model=%02d name=%s rms=%.6f peak=%.6f\n",
      model, name, total / 3.0, peak);
  }
  return 0;
}
