#include "../IOCore/HimaliaSketch/strings_engine.h"
#include <cmath>
#include <cstdio>

static bool finiteRun(StringsEngine &s, bool gate, unsigned count,
                      double &sum, double &sumSq, float &peak) {
  for (unsigned i = 0; i < count; ++i) {
    const float y = stringsTick(s, gate);
    if (!std::isfinite(y)) return false;
    sum += y;
    sumSq += (double)y * (double)y;
    if (std::fabs(y) > peak) peak = std::fabs(y);
  }
  return true;
}

int main() {
  StringsEngine s;
  stringsInit(s);
  stringsSetInstrument(s, STR_VIOLIN);
  stringsControl(s, 220.0f, false, STR_UNISON, 0.35f, 0.45f, 0.0018f, 5.2f);

  double sum = 0.0, sumSq = 0.0;
  float peak = 0.0f;
  if (!finiteRun(s, true, 120000, sum, sumSq, peak)) return 1;

  const double mean = sum / 120000.0;
  const double rms = std::sqrt(sumSq / 120000.0);
  if (rms < 0.0001 || std::fabs(mean) > 0.08 || peak > 1.0f) {
    std::fprintf(stderr, "solo output outside bounds: mean=%f rms=%f peak=%f\n",
                 mean, rms, peak);
    return 2;
  }

  stringsControl(s, 110.0f, true, STR_MAJOR, 0.55f, 0.65f, 0.003f, 6.0f);
  unsigned active = 0;
  for (unsigned i = 0; i < STR_VOICES; ++i) if (s.voice[i].active) ++active;
  if (active != STR_VOICES) return 3;

  sum = sumSq = 0.0;
  peak = 0.0f;
  if (!finiteRun(s, true, 120000, sum, sumSq, peak)) return 4;
  if (peak > 1.0f || std::sqrt(sumSq / 120000.0) < 0.0001) return 5;

  stringsSetInstrument(s, STR_CELLO);
  stringsClear(s);
  stringsControl(s, 65.406f, false, STR_UNISON, 0.25f, 0.50f, 0.0f, 5.2f);
  sum = sumSq = 0.0;
  peak = 0.0f;
  if (!finiteRun(s, true, 120000, sum, sumSq, peak)) return 6;

  std::printf("strings DSP smoke test passed; solo RMS=%f, peak=%f\n", rms, peak);
  return 0;
}
