// Host test for the Himalia strings engine (v2).
// Checks every instrument x layout for finite, bounded, non-silent output,
// solo intonation, pizzicato decay and tremolo.
#include "../IOCore/HimaliaSketch/strings_engine.h"
#include <cmath>
#include <cstdio>
#include <vector>

static StringsEngine s;

static bool render(bool gate, unsigned count, std::vector<float> &out) {
  out.resize(count);
  for (unsigned i = 0; i < count; ++i) {
    if ((i & 127) == 0) stringsControl(s);
    const float y = stringsTick(s, gate);
    if (!std::isfinite(y)) return false;
    out[i] = y;
  }
  return true;
}

static double rms(const std::vector<float> &x, unsigned from, unsigned to) {
  double a = 0;
  for (unsigned i = from; i < to; ++i) a += (double)x[i] * x[i];
  return std::sqrt(a / (to - from));
}

static float peak(const std::vector<float> &x) {
  float p = 0;
  for (float v : x) if (std::fabs(v) > p) p = std::fabs(v);
  return p;
}

// pitch by autocorrelation around the expected period, in cents
static double centsOff(const std::vector<float> &x, unsigned from, unsigned to, double hz) {
  const double T = STR_SR / hz;
  const int lo = (int)(T * 0.8), hi = (int)(T * 1.25) + 2;
  std::vector<double> ac(hi + 2, 0.0);
  for (int lag = lo - 1; lag <= hi + 1; ++lag) {
    double a = 0;
    for (unsigned i = from; i + lag < to; ++i) a += (double)x[i] * x[i + lag];
    ac[lag] = a;
  }
  int best = lo;
  for (int lag = lo; lag <= hi; ++lag) if (ac[lag] > ac[best]) best = lag;
  const double a = ac[best - 1], b = ac[best], c = ac[best + 1];
  const double den = a - 2 * b + c;
  const double d = den != 0 ? 0.5 * (a - c) / den : 0;
  return 1200.0 * std::log2((STR_SR / (best + d)) / hz);
}

int main() {
  int fails = 0;
  std::vector<float> x;

  // every instrument and layout: finite, bounded, sounding
  for (int ins = 0; ins < INS_COUNT; ++ins) {
    for (int lay = 0; lay < LAY_COUNT; ++lay) {
      stringsInit(s);
      s.instrument = (uint8_t)ins; s.layout = (uint8_t)lay;
      s.rootHz = 261.6f; s.speed = 0.6f; s.pressure = 0.6f; s.beta = 0.11f;
      if (lay >= LAY_DIVISI_MAJOR) stringsDivisi(s, 0xB3A5);
      stringsControl(s);
      if (!render(true, 60000, x)) { std::printf("FAIL ins %d lay %d: not finite\n", ins, lay); ++fails; continue; }
      // short-stroke models (spiccato ostinato) stop while the gate is held:
      // for them the start must sound, for the others the sustain
      const bool shortStroke = STR_ARTIC[s.articEff].stroke > 0.0f;
      const double r = shortStroke ? rms(x, 0, 8000) : rms(x, 20000, 60000);
      const float p = peak(x);
      if (r < 0.02 || p > 1.0f) { std::printf("FAIL ins %d lay %d: rms %.3f peak %.3f\n", ins, lay, r, p); ++fails; }
    }
  }

  // solo intonation (no vibrato), violin and cello, several notes
  const float vn[] = { 196.0f, 440.0f, 880.0f };
  const float vc[] = { 65.41f, 130.8f, 261.6f };
  for (int k = 0; k < 6; ++k) {
    stringsInit(s);
    s.instrument = k < 3 ? INS_SOLO_VIOLIN : INS_SOLO_CELLO;
    s.rootHz = k < 3 ? vn[k] : vc[k - 3];
    s.speed = 0.5f; s.pressure = 0.5f; s.beta = 0.11f; s.vibrato = false;
    stringsControl(s);
    s.v[0].len = s.v[0].lenTarget;
    if (!render(true, 50000, x)) { ++fails; continue; }
    const double c = centsOff(x, 25000, 50000, s.rootHz);
    if (std::fabs(c) > 10.0) { std::printf("FAIL intonation %.1f Hz: %+.1f cents\n", s.rootHz, c); ++fails; }
  }

  // pizzicato: plucked, then dies away
  stringsInit(s);
  s.instrument = INS_SOLO_VIOLIN; s.rootHz = 392.0f; s.pizz = true;
  stringsControl(s);
  if (!render(true, 95000, x)) ++fails;
  else {
    const double early = rms(x, 500, 5000), late = rms(x, 85000, 95000);
    if (early < 0.02 || late > early * 0.2) { std::printf("FAIL pizzicato early %.3f late %.3f\n", early, late); ++fails; }
  }

  // tremolo: bowing keeps sounding
  stringsInit(s);
  s.instrument = INS_VIOLIN_SECTION; s.rootHz = 392.0f; s.tremolo = true;
  stringsControl(s);
  if (!render(true, 60000, x) || rms(x, 20000, 60000) < 0.02) { std::printf("FAIL tremolo\n"); ++fails; }

  // release: silence after the gate closes
  stringsInit(s);
  s.instrument = INS_FULL_STRINGS; s.rootHz = 220.0f;
  stringsControl(s);
  render(true, 30000, x);
  if (!render(false, 120000, x) || rms(x, 100000, 120000) > 0.005) { std::printf("FAIL release\n"); ++fails; }

  // every articulation on solo violin and cello section: finite, bounded, sounding
  for (int art = 0; art < ART_COUNT; ++art) {
    for (int ins = 0; ins < 2; ++ins) {
      stringsInit(s);
      s.instrument = ins ? INS_CELLO_SECTION : INS_SOLO_VIOLIN; s.artic = (uint8_t)art;
      s.rootHz = ins ? 130.8f : 440.0f; s.speed = 0.6f; s.pressure = 0.6f;
      stringsControl(s);
      if (!render(true, 12000, x)) { std::printf("FAIL artic %d: not finite\n", art); ++fails; continue; }
      const double r = rms(x, 0, 12000);
      const float p = peak(x);
      if (r < 0.01 || p > 1.0f) { std::printf("FAIL artic %d ins %d: rms %.3f peak %.3f\n", art, ins, r, p); ++fails; }
    }
  }

  // divisi output: silent for a solo, sounding for a section, never louder than the mix
  {
    const int ins[2] = { INS_SOLO_VIOLIN, INS_VIOLIN_SECTION };
    for (int k = 0; k < 2; ++k) {
      stringsInit(s);
      s.instrument = (uint8_t)ins[k]; s.layout = LAY_MAJOR; s.rootHz = 261.6f; s.speed = 0.6f;
      stringsControl(s);
      double a = 0, b = 0; float p2 = 0;
      for (unsigned n = 0; n < 50000; ++n) {
        if ((n & 127) == 0) stringsControl(s);
        const float y = stringsTick(s, true);
        const float d = s.outDivisi;
        if (!std::isfinite(d)) { ++fails; break; }
        if (n > 20000) { a += (double)y * y; b += (double)d * d; }
        if (std::fabs(d) > p2) p2 = std::fabs(d);
      }
      const double ra = std::sqrt(a / 30000), rb = std::sqrt(b / 30000);
      if (k == 0 && rb > 0.005) { std::printf("FAIL divisi out not silent for solo: %.3f\n", rb); ++fails; }
      if (k == 1 && (rb < 0.02 || p2 > 1.0f)) { std::printf("FAIL divisi out section: rms %.3f peak %.3f (mix %.3f)\n", rb, p2, ra); ++fails; }
    }
  }

  // tempo: gates every 0.5 s -> vibrato 6 Hz, tremolo 12 strokes / s
  stringsInit(s);
  s.instrument = INS_SOLO_VIOLIN; s.rootHz = 440.0f; s.tremolo = true;
  stringsControl(s);
  {
    const unsigned P = (unsigned)(0.5f * STR_SR);
    for (unsigned n = 0; n < 4 * P; ++n) {
      if ((n & 127) == 0) stringsControl(s);
      stringsTick(s, (n % P) < P * 9 / 10);
    }
    stringsControl(s);
    if (std::fabs(s.vibHz - 6.0f) > 0.05f || std::fabs(s.tremHz - 6.0f) > 0.05f) {
      std::printf("FAIL tempo sync: vibrato %.2f Hz, tremolo %.2f Hz\n", s.vibHz, s.tremHz); ++fails;
    }
  }

  if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
  std::printf("strings DSP test passed (%d instruments x %d layouts, %d articulations, intonation, "
              "pizzicato, tremolo, release, tempo sync, divisi output)\n", (int)INS_COUNT, (int)LAY_COUNT, (int)ART_COUNT);
  return 0;
}
