// Relative loudness and peak report for all Himalia Strings models.
// Host-side measurement only; no firmware behavior is changed.
#define STR_OUT_GAIN 0.72f
#include "../IOCore/HimaliaSketch/strings_engine.h"
#include <cmath>
#include <cstdio>

static StringsEngine s;
struct Level { double rms; double peak; };

static Level renderLevel(int model, float hz) {
  stringsInit(s);
  s.instrument = (uint8_t)model;
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
    static const char *INS[7] = { "solo_violin", "solo_cello", "violin_section", "viola_section", "cello_section", "low_strings", "full_strings" };
    static const char *CHR[3] = { "classical", "romantic", "film" };
    char name[48]; std::snprintf(name, sizeof name, "%s_%s", INS[model / 3], CHR[model % 3]);
    if (peak > STR_LIM_CEIL + 1e-4) {
      std::fprintf(stderr, "FAIL model=%02d peak=%.6f exceeds calibrated headroom\n", model, peak);
      return 1;
    }
    std::printf("LEVEL model=%02d name=%s rms=%.6f peak=%.6f\n",
      model, name, total / 3.0, peak);
  }
  return 0;
}
