// Host test for the Himalia strings engine (v2).
// Checks every instrument x layout for finite, bounded, non-silent output,
// solo intonation, pizzicato decay and tremolo.
// STR_OUT_GAIN: the firmware value (no override)
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
    s.instrument = k < 3 ? MDL_SOLO_VIOLIN : MDL_SOLO_CELLO;
    s.rootHz = k < 3 ? vn[k] : vc[k - 3];
    s.speed = 0.5f; s.pressure = 0.5f; s.beta = 0.11f; s.vibrato = false;
    stringsControl(s);
    s.v[0].len = s.v[0].lenTarget;
    if (!render(true, 50000, x)) { ++fails; continue; }
    const double c = centsOff(x, 25000, 50000, s.rootHz);
    if (std::fabs(c) > 10.0) { std::printf("FAIL intonation %.1f Hz: %+.1f cents\n", s.rootHz, c); ++fails; }
  }

  // legato finger travel is continuous and supports fractional pitch CV for
  // both a solo instrument and an ensemble player (no fret/semitone snapping).
  for (int model = 0; model < 2; ++model) {
    stringsInit(s);
    s.instrument = model == 0 ? MDL_SOLO_VIOLIN : MDL_VIOLIN_SECTION;
    s.artic = ART_LEGATO;
    s.rootHz = 440.0f; s.speed = 0.6f; s.pressure = 0.6f;
    stringsControl(s);
    if (!render(true, 30000, x)) { ++fails; continue; }
    const float fractionalHz = 440.0f * std::exp2(5.37f / 12.0f);
    s.rootHz = fractionalHz;
    stringsControl(s);
    const float before = s.v[0].len;
    const float target = s.v[0].lenTarget;
    if (model == 0 && std::fabs(s.v[0].hz - fractionalHz) > 1.0f) {
      std::printf("FAIL fractional pitch CV was quantized: %.3f vs %.3f\n", s.v[0].hz, fractionalHz); ++fails;
    }
    stringsTick(s, true);
    if (std::fabs(s.v[0].len - target) >= std::fabs(before - target) ||
        std::fabs(s.v[0].len - target) < 0.005f * std::fabs(before - target)) {
      std::printf("FAIL model %d legato slide snapped or moved in wrong direction\n", model); ++fails;
    }
    if (!render(true, 40000, x) || std::fabs(s.v[0].len - s.v[0].lenTarget) > 0.5f) {
      std::printf("FAIL model %d legato slide did not settle smoothly\n", model); ++fails;
    }
  }

  // pizzicato: plucked, then dies away
  stringsInit(s);
  s.instrument = MDL_SOLO_VIOLIN; s.rootHz = 392.0f; s.pizz = true;
  stringsControl(s);
  if (!render(true, 95000, x)) ++fails;
  else {
    const double early = rms(x, 500, 5000), late = rms(x, 85000, 95000);
    if (early < 0.02 || late > early * 0.2) { std::printf("FAIL pizzicato early %.3f late %.3f\n", early, late); ++fails; }
  }

  // tremolo: bowing keeps sounding
  stringsInit(s);
  s.instrument = MDL_VIOLIN_SECTION; s.rootHz = 392.0f; s.tremolo = true;
  stringsControl(s);
  if (!render(true, 60000, x) || rms(x, 20000, 60000) < 0.02) { std::printf("FAIL tremolo\n"); ++fails; }

  // release: silence after the gate closes
  stringsInit(s);
  s.instrument = MDL_FULL_STRINGS; s.rootHz = 220.0f;
  stringsControl(s);
  render(true, 30000, x);
  if (!render(false, 120000, x) || rms(x, 100000, 120000) > 0.005) { std::printf("FAIL release\n"); ++fails; }

  // natural-default bow envelope: smooth instrument-aware attack and release
  {
    stringsInit(s);
    s.instrument = MDL_SOLO_VIOLIN; s.artic = ART_LEGATO; s.rootHz = 440.0f;
    stringsControl(s);
    if (!render(true, 1, x)) ++fails;
    const float violinAttackK = s.v[0].attackK;
    if (!(violinAttackK > 0.0002f && violinAttackK < 0.0008f) || s.v[0].env > 0.002f) {
      std::printf("FAIL violin natural attack coefficient / first-sample envelope: %.7f / %.5f\\n", violinAttackK, s.v[0].env); ++fails;
    }
    if (!render(true, 500, x) || !(s.v[0].env > 0.08f && s.v[0].env < 0.30f)) {
      std::printf("FAIL violin attack did not ease in smoothly: %.4f\\n", s.v[0].env); ++fails;
    }
    if (!render(false, 1, x)) ++fails;
    const float violinReleaseK = s.v[0].relK;

    stringsInit(s);
    s.instrument = MDL_SOLO_CELLO; s.artic = ART_LEGATO; s.rootHz = 130.8f;
    stringsControl(s);
    if (!render(true, 1, x)) ++fails;
    const float celloAttackK = s.v[0].attackK;
    if (!render(false, 1, x)) ++fails;
    const float celloReleaseK = s.v[0].relK;
    if (!(celloAttackK < violinAttackK && celloReleaseK < violinReleaseK)) {
      std::printf("FAIL cello should have a more gradual attack/release than violin (attack %.7f/%.7f release %.7f/%.7f)\\n",
                  celloAttackK, violinAttackK, celloReleaseK, violinReleaseK); ++fails;
    }
  }

  // every articulation on solo violin and cello section: finite, bounded, sounding
  for (int art = 0; art < ART_COUNT; ++art) {
    for (int ins = 0; ins < 2; ++ins) {
      stringsInit(s);
      s.instrument = ins ? MDL_CELLO_SECTION : MDL_SOLO_VIOLIN; s.artic = (uint8_t)art;
      s.rootHz = ins ? 130.8f : 440.0f; s.speed = 0.6f; s.pressure = 0.6f;
      stringsControl(s);
      if (!render(true, 12000, x)) { std::printf("FAIL artic %d: not finite\n", art); ++fails; continue; }
      const double r = rms(x, 0, 12000);
      const float p = peak(x);
      const double minRms = STR_ARTIC[art].mode == MODE_STRIKE ? 0.005 : 0.01;
      if (r < minRms || p > 1.0f) { std::printf("FAIL artic %d ins %d: rms %.3f peak %.3f\n", art, ins, r, p); ++fails; }
    }
  }

  // divisi output: silent for a solo, sounding for a section, never louder than the mix
  {
    const int ins[2] = { MDL_SOLO_VIOLIN, MDL_VIOLIN_SECTION };
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

  // dynamic gate: a higher velocity must play louder
  {
    double prev = 0;
    const float vels[3] = { 0.3f, 0.7f, 1.2f };
    for (int k = 0; k < 3; ++k) {
      stringsInit(s);
      s.instrument = MDL_SOLO_VIOLIN; s.artic = ART_DETACHE; s.rootHz = 392.0f; s.speed = 0.7f;
      s.velocity = vels[k]; s.speedCV = vels[k];
      stringsControl(s);
      if (!render(true, 20000, x)) { ++fails; continue; }
      const double r = rms(x, 4000, 20000);
      // louder with every step; no limiter or automatic gain reduction
      if (k > 0 && r <= prev * (k == 2 ? 1.02 : 1.2)) { std::printf("FAIL velocity %.1f not louder (%.3f vs %.3f)\n", vels[k], r, prev); ++fails; }
      prev = r;
    }
  }

  // adaptive vibrato and tremolo: rates follow playing energy, not BPM subdivisions
  {
    stringsInit(s);
    s.instrument = MDL_SOLO_VIOLIN; s.rootHz = 440.0f; s.tremolo = true;
    s.speed = 0.25f; s.pressure = 0.25f; s.velocity = 0.35f; s.noteDur = 0.6f;
    stringsControl(s);
    const float vibSoft = s.v[0].vibK;
    const float tremSoft = s.v[0].tremInc;
    const float phase = s.v[0].tremPh;
    s.speed = 0.9f; s.pressure = 0.85f; s.velocity = 1.2f;
    stringsControl(s);
    const float vibStrong = s.v[0].vibK;
    const float tremStrong = s.v[0].tremInc;
    if (!(vibStrong > vibSoft && tremStrong > tremSoft)) {
      std::printf("FAIL adaptive playing rates did not respond to stronger bowing (vib %.6f -> %.6f, trem %.8f -> %.8f)\\n",
                  vibSoft, vibStrong, tremSoft, tremStrong); ++fails;
    }
    if (std::fabs(s.v[0].tremPh - phase) > 1e-6f) {
      std::printf("FAIL control update reset tremolo phase\\n"); ++fails;
    }
    const float adaptiveDrive = 0.45f * s.speed + 0.35f * s.pressure
                              + 0.20f * (s.velocity / 1.25f);
    const float expectedInc = (4.5f + 3.5f * adaptiveDrive) * s.v[0].vibRateMul / STR_SR;
    if (std::fabs(s.v[0].tremInc - expectedInc) > 1e-8f) {
      std::printf("FAIL tremolo increment does not match adaptive playing energy\\n"); ++fails;
    }
  }

  // rhythm: a 3+3+2 ostinato at 140 bpm, then a tempo change to 90 bpm
  {
    stringsInit(s);
    s.instrument = MDL_SOLO_VIOLIN; stringsControl(s);
    const float e140 = 60.0f / 140.0f / 2.0f, q90 = 60.0f / 90.0f;
    const float pat[3] = { 3 * e140, 3 * e140, 2 * e140 };
    unsigned n = 0, gateEnd = 0; double next = 0; int k = 0;
    float bpmA = 0; int patA = 0;
    while (n < (unsigned)(18.0f * STR_SR)) {
      const bool second = n > (unsigned)(9.0f * STR_SR);
      if (n >= (unsigned)(next * STR_SR)) {
        const float iv = second ? q90 : pat[k % 3];
        gateEnd = n + (unsigned)(0.5f * iv * STR_SR); next += iv; ++k;
      }
      if ((n & 127) == 0) stringsControl(s);
      if (n == (unsigned)(9.0f * STR_SR)) { bpmA = 60.0f * STR_SR / (float)(s.clockPeriod ? s.clockPeriod : 1); patA = s.patLen; }
      stringsTick(s, n < gateEnd); ++n;
    }
    const float bpmB = 60.0f * STR_SR / (float)(s.clockPeriod ? s.clockPeriod : 1);
    if (std::fabs(bpmA - 140.0f) > 2.0f || patA != 3) { std::printf("FAIL ostinato tempo %.1f bpm, pattern %d notes\n", bpmA, patA); ++fails; }
    if (std::fabs(bpmB - 90.0f) > 1.5f && std::fabs(bpmB - 180.0f) > 3.0f) { std::printf("FAIL tempo change %.1f bpm\n", bpmB); ++fails; }
  }

  // v1.32: the output never goes past the safety ceiling (4.98 V), also at the
  // knob extremes with a 10 V dynamic gate
  {
    float mx = 0.0f;
    for (int m = 0; m < MDL_COUNT; ++m) {
      stringsInit(s); s.instrument = (uint8_t)m; s.rootHz = 65.4f; s.speed = 1.0f; s.pressure = 1.0f;
      s.velocity = 1.25f; s.speedCV = 1.25f; stringsControl(s);
      for (int n = 0; n < 40000; ++n) {
        if ((n & 127) == 0) stringsControl(s);
        const float o = stringsTick(s, (n % 19000) < 15000);
        mx = std::fmax(mx, std::fmax(std::fabs(o), std::fabs(s.outDivisi)));
      }
    }
    if (mx > STR_LIM_CEIL + 1e-4f) { std::printf("FAIL output peak %.4f above the ceiling\n", mx); ++fails; }
  }

  // v1.32 HAESE natural default: deterministic -- the same CV / gate phrase
  // played twice from the same start gives exactly the same output
  {
    const float mel[6] = { 0, 2, 4, 7, 12, 5 };
    double sum[2] = { 0, 0 };
    for (int pass = 0; pass < 2; ++pass) {
      stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; stringsControl(s);
      for (int n = 0; n < 6 * 12000; ++n) {
        s.rootHz = 293.66f * std::exp2(mel[n / 12000] / 12.0f);
        if ((n & 127) == 0) stringsControl(s);
        const float o = stringsTick(s, (n % 12000) < 9000);
        sum[pass] += o * (1.0 + (n % 7));
      }
    }
    if (sum[0] != sum[1]) { std::printf("FAIL phrase not deterministic\n"); ++fails; }
  }

  // v1.32 phrase analysis: a rest starts a new phrase (down-bow, gentler
  // attack), a rising line inside the phrase gets more bow energy
  {
    stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; stringsControl(s);
    float eFirst = 0, eRise = 0, attFirst = 0, attRest = 0; bool startAfterRest = false;
    const float mel[5] = { 0, 4, 7, 0, 0 };                 // notes 0..2 legato-ish, rest, note 4
    int note = -1;
    for (int n = 0; n < 5 * 14000; ++n) {
      const int k = n / 14000;
      const bool gate = (k < 3) ? (n % 14000) < 13000 : (k == 4 && (n % 14000) < 12000);
      s.rootHz = 293.66f * std::exp2(mel[k] / 12.0f);
      if ((n & 127) == 0) stringsControl(s);
      stringsTick(s, gate);
      if (gate && k != note && (n % 14000) == 1) {
        note = k;
        if (k == 0) { eFirst = s.brain.energy; attFirst = s.brain.attackMul; }
        if (k == 2) eRise = s.brain.energy;
        if (k == 4) { startAfterRest = s.brain.phraseStart; attRest = s.brain.attackMul; }
      }
    }
    if (!(eRise > eFirst)) { std::printf("FAIL rising line energy %.3f vs %.3f\n", eRise, eFirst); ++fails; }
    if (!startAfterRest || attRest <= 1.0f || attFirst <= 1.0f) { std::printf("FAIL phrase start after a rest\n"); ++fails; }
  }

  if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
  std::printf("strings DSP test passed (%d instruments x %d layouts, %d articulations, intonation, "
              "pizzicato, tremolo, release, adaptive modulation, divisi output, velocity, ostinato tempo, output ceiling, deterministic phrase, phrase analysis)\n", (int)INS_COUNT, (int)LAY_COUNT, (int)ART_COUNT);
  return 0;
}
