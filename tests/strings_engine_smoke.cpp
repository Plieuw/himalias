#include <algorithm>
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
    if (!render(true, 500, x)) ++fails;                     // a real gate (> 8 ms), not a trigger
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

  // v1.45 stereo (SAMPLE OUT = left, DIVISI OUT = right): a soloist in the
  // middle (both equal), full strings violins left / basses right, a section
  // spread (the two sides differ), never above the ceiling
  {
    const int ins[3] = { MDL_SOLO_VIOLIN, MDL_FULL_STRINGS, MDL_VIOLIN_SECTION };
    for (int k = 0; k < 3; ++k) {
      stringsInit(s);
      s.instrument = (uint8_t)ins[k]; s.layout = LAY_UNISON; s.rootHz = 261.6f; s.speed = 0.6f;
      stringsControl(s);
      double a = 0, b = 0, d2 = 0; float pk = 0;
      for (unsigned n = 0; n < 50000; ++n) {
        if ((n & 127) == 0) stringsControl(s);
        const float y = stringsTick(s, true);
        const float d = s.outDivisi;
        if (!std::isfinite(d)) { ++fails; break; }
        if (n > 20000) { a += (double)y * y; b += (double)d * d; d2 += (double)(y - d) * (y - d); }
        pk = std::fmax(pk, std::fmax(std::fabs(y), std::fabs(d)));
      }
      const double L = std::sqrt(a / 30000), R = std::sqrt(b / 30000), diff = std::sqrt(d2 / 30000);
      if (pk > 1.0f || L < 0.01 || R < 0.01) { std::printf("FAIL stereo %d: L %.3f R %.3f peak %.3f\n", ins[k], L, R, pk); ++fails; }
      if (k == 0 && diff > 0.05 * L) { std::printf("FAIL soloist not in the middle (%.3f)\n", diff / L); ++fails; }
      if (k == 2 && diff < 0.05 * L) { std::printf("FAIL section has no width (%.3f)\n", diff / L); ++fails; }
      std::printf("stereo model %2d: L %.3f R %.3f, side %.0f %%\n", ins[k], L, R, 100 * diff / (L + R));
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

  // v1.35: 7 instruments x 3 characters; SPREAD = expression on a soloist,
  // layout on an ensemble; the character shapes the interpretation
  {
    stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; s.knobSpread = 0.0f; stringsControl(s);
    if (s.expr != 0.0f) { std::printf("FAIL expression knob on a soloist\n"); ++fails; }
    stringsInit(s); s.instrument = MDL_VIOLIN_SECTION; s.knobSpread = 0.0f; stringsControl(s);
    if (std::fabs(s.expr - STR_CHAR[CHR_CLASSICAL].expr) > 1e-6f) { std::printf("FAIL SPREAD must stay the layout on a section\n"); ++fails; }
    stringsInit(s); s.instrument = MDL_SOLO_CELLO_ROM; s.knobSpread = 0.5f; stringsControl(s);
    const float gRom = s.glideMul, eRom = s.expr;
    stringsInit(s); s.instrument = MDL_SOLO_CELLO; s.knobSpread = 0.5f; stringsControl(s);
    if (!(gRom > s.glideMul && eRom > s.expr)) { std::printf("FAIL romantic character\n"); ++fails; }
    if (MDL_COUNT != 21) { std::printf("FAIL model count\n"); ++fails; }
  }

  // v1.34: gate length -> articulation (AUTO), the knob as the base, a
  // patched ARTIC CV wins; look-ahead, heavy beats, leader, long arc
  {
    struct Case { float ioi, duty; uint8_t base; bool locked; uint8_t want; const char *name; };
    const Case cs[] = {
      { 0.50f, 0.98f, ART_AUTO,     false, ART_LEGATO,   "AUTO long gate -> legato" },
      { 0.50f, 0.70f, ART_AUTO,     false, ART_DETACHE,  "AUTO normal gate -> detache" },
      { 0.50f, 0.36f, ART_AUTO,     false, ART_STACCATO, "AUTO short gate, moderate tempo -> staccato" },
      { 0.25f, 0.30f, ART_AUTO,     false, ART_SPICCATO, "AUTO short gate, fast tempo -> spiccato" },
      { 0.10f, 0.95f, ART_AUTO,     false, ART_LEGATO,   "AUTO fast tied run -> legato (slurred)" },
      { 0.10f, 0.60f, ART_AUTO,     false, ART_SPICCATO, "AUTO very fast -> sautille / spiccato" },
      { 0.50f, 0.98f, ART_MARTELE,  false, ART_MARTELE,  "martele long stays martele" },
      { 0.50f, 0.36f, ART_MARTELE,  false, ART_STACCATO, "martele short -> staccato" },
      { 0.50f, 0.36f, ART_DETACHE,  true,  ART_DETACHE,  "ARTIC CV patched: no gate articulation" },
      { 0.50f, 0.36f, ART_TASTO,    false, ART_TASTO,    "tone colour stays" },
    };
    for (const Case &c : cs) {
      stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; s.artic = c.base; s.articLocked = c.locked; s.rootHz = 440.0f;
      stringsControl(s);
      const int P = (int)(c.ioi * STR_SR), G = (int)(c.duty * c.ioi * STR_SR);
      // the stroke most notes get (v1.37: heavy beats may bite as martele)
      int cnt[ART_COUNT] = {0};
      for (int n = 0; n < 12 * P; ++n) {
        if ((n & 127) == 0) stringsControl(s);
        stringsTick(s, (n % P) < G);
        if (n >= 4 * P && n % P == 200) cnt[s.articEff]++;
      }
      int best = 0; for (int a = 1; a < ART_COUNT; ++a) if (cnt[a] > cnt[best]) best = a;
      if (best != c.want) { std::printf("FAIL %s (got %d)\n", c.name, best); ++fails; }
    }
    // look-ahead: long-short-short pattern -> the note length comes from the pattern
    stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; s.artic = ART_AUTO; s.rootHz = 440.0f; stringsControl(s);
    const int pat[3] = { 24000, 12000, 12000 };
    int t = 0, idx = 0, okAhead = 0, checked = 0, downs = 0, notes = 0; uint8_t ph0 = 255, ph4 = 255;
    float e1 = 0, e4 = 0;
    for (int bar = 0; bar < 12; ++bar)
      for (int j = 0; j < 3; ++j) {
        const int L = pat[j];
        for (int n = 0; n < L; ++n) {
          if ((t & 127) == 0) stringsControl(s);
          stringsTick(s, n < L * 6 / 10); ++t;
          if (n == 1) {
            ++notes; if (s.brain.downBow) ++downs;
            if (bar >= 6) { ++checked; if (std::fabs(s.noteAhead - (float)L / STR_SR) < 0.03f) ++okAhead; }
                      }
        }
        ++idx;
      }
    if (okAhead < checked - 1) { std::printf("FAIL look-ahead %d/%d\n", okAhead, checked); ++fails; }
    if (downs < 6 || downs > notes / 2) { std::printf("FAIL heavy beats on a down-bow (%d of %d)\n", downs, notes); ++fails; }
    (void)ph0; (void)ph4; (void)e1; (void)e4; (void)idx;
    // the long arc: the same 2-note phrase, separated by rests, gets different energy
    stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; s.rootHz = 440.0f; stringsControl(s);
    float en[8] = {0};
    for (int ph = 0; ph < 8; ++ph)
      for (int n = 0; n < 60000; ++n) {
        if ((n & 127) == 0) stringsControl(s);
        stringsTick(s, n < 20000 || (n >= 24000 && n < 40000));
        if (n == 1) en[ph] = s.brain.energy;
      }
    if (!(en[5] > en[0] + 0.05f) || !(en[1] != en[4])) { std::printf("FAIL long arc %.3f %.3f %.3f\n", en[0], en[4], en[5]); ++fails; }
    // the section follows its leader: player 1 starts at once, the others later
    stringsInit(s); s.instrument = MDL_VIOLIN_SECTION; s.rootHz = 440.0f; stringsControl(s);
    stringsTick(s, true);
    bool lead = s.v[0].gate, others = false;
    for (int i = 1; i < STR_VOICES; ++i) if (s.v[i].active && s.v[i].onset) others = true;
    if (!lead || !others) { std::printf("FAIL concertmaster leads\n"); ++fails; }
  }

  // v1.34 aftertouch: pressing CV IN RATE in during the note adds bow weight
  {
    double r[2], br[2];
    for (int k = 0; k < 2; ++k) {
      stringsInit(s); s.instrument = MDL_SOLO_CELLO; s.artic = ART_LEGATO; s.rootHz = 130.8f;
      s.velocity = 0.5f; s.speedCV = 0.5f; stringsControl(s);
      std::vector<float> o;
      for (int n = 0; n < 60000; ++n) {
        if (n == 15000 && k) s.velocity = 1.0f;          // pressed in (the bow speed stays)
        if ((n & 127) == 0) stringsControl(s);
        o.push_back(stringsTick(s, true));
      }
      r[k] = rms(o, 40000, 60000);
      std::vector<float> d; for (unsigned n = 40001; n < 60000; ++n) d.push_back(o[n] - o[n - 1]);
      br[k] = rms(d, 0, (unsigned)d.size()) / r[k];          // brightness
      if (k && !(s.v[0].at > 0.3f)) { std::printf("FAIL aftertouch not seen (%.2f)\n", s.v[0].at); ++fails; }
    }
    if (!(br[1] > br[0] * 1.01) || !(r[1] > r[0] * 1.08)) { std::printf("FAIL aftertouch: no brighter (%.3f, level %.3f)\n", br[1] / br[0], r[1] / r[0]); ++fails; }
  }

  // v1.34.1: the gate opens while the velocity CV still rises -> the note
  // gets the peak, not the threshold level, and no false aftertouch
  {
    double lv[2]; const float tgt[2] = { 0.3f, 1.0f };
    for (int k = 0; k < 2; ++k) {
      stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; s.artic = ART_LEGATO; s.rootHz = 440.0f; stringsControl(s);
      std::vector<float> o;
      for (int n = 0; n < 40000; ++n) {
        if ((n & 127) == 0) {                                  // the loop: CV rising in steps
          const float r = std::min(1.0f, (float)n / 900.0f);
          s.velocity = s.speedCV = 0.06f + (tgt[k] - 0.06f) * r;
          stringsControl(s);
        }
        o.push_back(stringsTick(s, true));
      }
      lv[k] = rms(o, 20000, 40000);
      if (std::fabs(s.v[0].vel - tgt[k]) > 0.02f) { std::printf("FAIL velocity caught %.2f vs %.2f\n", s.v[0].vel, tgt[k]); ++fails; }
      if (std::fabs(s.v[0].at) > 0.01f) { std::printf("FAIL false aftertouch %.2f\n", s.v[0].at); ++fails; }
    }
    if (!(lv[1] > lv[0] * 1.6)) { std::printf("FAIL velocity range %.2f\n", lv[1] / lv[0]); ++fails; }
    std::printf("velocity 0.3 vs 1.0: %.1f dB\n", 20 * std::log10(lv[1] / lv[0]));
  }

  // v1.35: a slide only along one string, at most an octave
  {
    struct Sl { float from, to; bool slide; const char *name; };
    const Sl sl[] = {
      { 440.0f, 587.3f, true,  "A4 -> D5 on the A string: slide" },
      { 587.3f, 659.3f, true,  "D5 -> E5 stays on the A string: slide" },
      { 440.0f, 261.6f, false, "A4 -> C4 is below the A string: cross" },
      { 392.0f, 987.8f, false, "G4 -> B5 more than an octave: no slide" },
    };
    for (const Sl &c : sl) {
      stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; s.artic = ART_LEGATO; s.rootHz = c.from; stringsControl(s);
      for (int n = 0; n < 30000; ++n) { if ((n & 127) == 0) stringsControl(s); stringsTick(s, true); }
      const float l0 = s.v[0].len;
      s.rootHz = c.to;
      for (int n = 0; n < 480; ++n) { if ((n & 127) == 0) stringsControl(s); stringsTick(s, true); }   // 10 ms
      const float done = (l0 - s.v[0].len) / (l0 - s.v[0].lenTarget + 1e-9f);
      const bool slid = std::fabs(s.v[0].len - s.v[0].lenTarget) > 0.02f * std::fabs(l0 - s.v[0].lenTarget);
      if (slid != c.slide) { std::printf("FAIL %s (%.2f done after 10 ms)\n", c.name, done); ++fails; }
    }
  }

  // v1.35 legato: a finger drop inside the hand is quick, a shift is a short
  // slide, portamento a long one
  {
    struct Lg { uint8_t art; float to; float t10, t60; const char *name; };   // fraction done after 10 / 60 ms
    auto run = [&](uint8_t art, float to, float &f10, float &f60) {
      stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; s.artic = art; s.rootHz = 440.0f; stringsControl(s);
      for (int n = 0; n < 30000; ++n) { if ((n & 127) == 0) stringsControl(s); stringsTick(s, true); }
      const float l0 = s.v[0].len; s.rootHz = to;
      for (int n = 0; n < 2880; ++n) {
        if ((n & 127) == 0) stringsControl(s);
        stringsTick(s, true);
        const float f = (l0 - s.v[0].len) / (l0 - s.v[0].lenTarget);
        if (n == 479) f10 = f;
        if (n == 2879) f60 = f;
      }
    };
    float a10, a60, b10, b60, c10, c60;
    run(ART_LEGATO, 493.9f, a10, a60);       // A4 -> B4: finger drop
    run(ART_LEGATO, 659.3f, b10, b60);       // A4 -> E5 on the A string: a shift
    run(ART_PORTAMENTO, 659.3f, c10, c60);   // portamento
    if (!(a10 > 0.5f && a60 > 0.97f)) { std::printf("FAIL legato finger drop %.2f %.2f\n", a10, a60); ++fails; }
    if (!(b10 < a10 && b60 > 0.6f))   { std::printf("FAIL legato shift %.2f %.2f\n", b10, b60); ++fails; }
    if (!(c60 < b60))                 { std::printf("FAIL portamento slower than a shift %.2f %.2f\n", c60, b60); ++fails; }
    std::printf("legato drop %.0f%%/10ms, shift %.0f%%/60ms, portamento %.0f%%/60ms\n", a10 * 100, b60 * 100, c60 * 100);
  }

  // v1.36: a note harder than the music around it gets a real accent and
  // more bow; a softer one is taken back (the velocity is read 6 ms late)
  {
    stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; s.rootHz = 440.0f; stringsControl(s);
    const float vel[6] = { 0.6f, 0.6f, 0.6f, 1.1f, 0.6f, 0.3f };
    float acc[6], en[6];
    for (int k = 0; k < 6; ++k)
      for (int n = 0; n < 16000; ++n) {
        if ((n & 127) == 0) { if (n == 128) s.velocity = vel[k]; stringsControl(s); }   // CV arrives after the gate
        stringsTick(s, n < 13000);
        if (n == 1000) { acc[k] = s.brain.accent; en[k] = s.brain.energy; }
      }
    if (!(acc[3] > 1.5f * acc[2] && en[3] > en[2])) { std::printf("FAIL velocity accent %.2f/%.2f\n", acc[3], acc[2]); ++fails; }
    if (!(acc[5] < acc[4] && en[5] < en[4]))         { std::printf("FAIL soft note taken back\n"); ++fails; }
  }

  // v1.37: staccato gaps are no rests; inside a pattern the note before a
  // rest is known as a phrase end; heavy beats bite (martele) in AUTO
  {
    stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; s.artic = ART_AUTO; s.rootHz = 440.0f; stringsControl(s);
    int starts = 0, notes = 0, mart = 0;
    for (int k = 0; k < 16; ++k)
      for (int n = 0; n < 24000; ++n) {
        if ((n & 127) == 0) stringsControl(s);
        stringsTick(s, n < 9000);                              // 0.5 s notes, short gates
        if (n == 200 && k >= 6) { ++notes; if (s.brain.phraseStart) ++starts; if (s.articEff == ART_MARTELE) ++mart; }
      }
    if (starts > 1) { std::printf("FAIL staccato gaps read as rests (%d)\n", starts); ++fails; }
    if (mart < 1 || mart > notes / 2) { std::printf("FAIL heavy beats bite (%d of %d)\n", mart, notes); ++fails; }
    // pattern: 3 notes and a rest
    stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; s.rootHz = 440.0f; stringsControl(s);
    const int L[3] = { 12000, 12000, 24000 }, G[3] = { 11000, 11000, 10000 };
    int endsOk = 0, endsBad = 0;
    for (int bar = 0; bar < 10; ++bar)
      for (int j = 0; j < 3; ++j)
        for (int n = 0; n < L[j]; ++n) {
          if ((n & 127) == 0) stringsControl(s);
          stringsTick(s, n < G[j]);
          if (n == 200 && bar >= 5) { if (s.brain.phraseEnd == (j == 2)) ++endsOk; else ++endsBad; }
        }
    if (endsBad > 1) { std::printf("FAIL phrase end look-ahead (%d ok, %d wrong)\n", endsOk, endsBad); ++fails; }
  }

  // v1.38 humanize: in an ensemble every player has its own timing and
  // level, the leader stays first, a soloist is untouched
  {
    stringsInit(s); s.instrument = MDL_VIOLIN_SECTION; s.rootHz = 440.0f; stringsControl(s);
    float tmin = 1, tmax = 0, lmin = 9, lmax = 0;
    for (int i = 1; i < STR_VOICES; ++i) { tmin = std::min(tmin, s.v[i].hTime); tmax = std::max(tmax, s.v[i].hTime);
                                           lmin = std::min(lmin, s.v[i].hLev);  lmax = std::max(lmax, s.v[i].hLev); }
    if (!(s.v[0].hTime == 0.0f && tmax - tmin > 0.2f && lmax / lmin > 1.1f && lmax / lmin < 1.45f)) { std::printf("FAIL player personalities\n"); ++fails; }
    { double lg = 0; for (int i = 0; i < STR_VOICES; ++i) lg += std::log2(s.v[i].hLev);
      if (std::fabs(lg) > 1e-3) { std::printf("FAIL section level must stay the same on average\n"); ++fails; } }
    // onsets over many notes: player 1 always first, the others spread, late players stay late on average
    double sum[STR_VOICES] = {0};
    for (int k = 0; k < 20; ++k)
      for (int n = 0; n < 12000; ++n) {
        if ((n & 127) == 0) stringsControl(s);
        stringsTick(s, n < 9000);
        if (n == 0) for (int i = 0; i < STR_VOICES; ++i) sum[i] += s.v[i].onset;
      }
    int late = 1; for (int i = 2; i < STR_VOICES; ++i) if (s.v[i].hTime > s.v[late].hTime) late = i;
    int early = 1; for (int i = 2; i < STR_VOICES; ++i) if (s.v[i].hTime < s.v[early].hTime) early = i;
    if (!(sum[0] == 0 && sum[late] > sum[early])) { std::printf("FAIL humanized timing\n"); ++fails; }
    stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; s.rootHz = 440.0f; stringsControl(s);
    for (int n = 0; n < 2000; ++n) { if ((n & 127) == 0) stringsControl(s); stringsTick(s, true); }
    if (std::fabs(s.v[0].gain - 1.0f) > 1e-6f) { std::printf("FAIL soloist humanized\n"); ++fails; }
  }

  // v1.40 audio-rate FM: an audio-rate signal modulates, a sequencer step
  // or slow CV does not; FM changes the spectrum
  {
    StrFm f; f.lp = 0; f.g = 0; f.hys = 0.05f; f.n = f.cross = f.quiet = 0; f.pos = f.on = false;
    float maxStep = 0;
    for (int n = 0; n < 48000; ++n) { const float x = (n / 6000) % 2 ? 1.0f : 0.0f; maxStep = std::max(maxStep, std::fabs(strFmRun(f, x))); }
    if (f.on || maxStep > 0.01f) { std::printf("FAIL FM: sequencer steps leak (%.3f)\n", maxStep); ++fails; }
    float maxSine = 0;
    for (int n = 0; n < 24000; ++n) maxSine = std::max(maxSine, std::fabs(strFmRun(f, 0.5f * std::sin(6.2831853f * 220.0f * n / STR_SR))));
    if (!f.on || maxSine < 0.4f) { std::printf("FAIL FM: audio-rate not detected (%.3f)\n", maxSine); ++fails; }
    // through the engine: bow FM at 110 Hz changes the tone, then dies out
    double e[2];
    for (int k = 0; k < 2; ++k) {
      stringsInit(s); s.instrument = MDL_SOLO_CELLO; s.artic = ART_LEGATO; s.rootHz = 130.8f; stringsControl(s);
      std::vector<float> o;
      for (int n = 0; n < 48000; ++n) {
        if ((n & 127) == 0) stringsControl(s);
        stringsFmInput(s, 0.0f, 0.5f + (k ? 0.3f * std::sin(6.2831853f * 110.0f * n / STR_SR) : 0.0f));
        o.push_back(stringsTick(s, true));
      }
      // sideband energy at f0 + 110 Hz (Goertzel), relative to the total
      const double w = 6.2831853 * (130.8 + 110.0) / STR_SR, c = 2 * std::cos(w);
      double q1 = 0, q2 = 0;
      for (unsigned n = 30000; n < 48000; ++n) { const double q0 = o[n] + c * q1 - q2; q2 = q1; q1 = q0; }
      e[k] = std::sqrt(q1 * q1 + q2 * q2 - c * q1 * q2) / 18000.0 / rms(o, 30000, 48000);
    }
    if (!(e[1] > 2.5 * e[0])) { std::printf("FAIL bow FM has no effect (%.2f)\n", e[1] / e[0]); ++fails; }
    std::printf("bow FM sideband x%.1f (%.1f dB under the total)\n", e[1] / e[0], -20 * std::log10(e[1] * 2));
  }

  // v1.42 triggers: a 5 ms pulse plays a note of ~85 % of the expected
  // length; the articulation then follows the tempo
  {
    struct Tc { int P; uint8_t want; const char *name; };
    const Tc tc[] = { { 24000, ART_DETACHE, "triggers at 0.5 s -> detache" }, { 4800, ART_SPICCATO, "triggers at 0.1 s -> spiccato" } };
    for (const Tc &c : tc) {
      stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; s.artic = ART_AUTO; s.rootHz = 440.0f; stringsControl(s);
      float held = 0; int cnt[ART_COUNT] = {0};
      for (int n = 0; n < 12 * c.P; ++n) {
        if ((n & 127) == 0) stringsControl(s);
        stringsTick(s, (n % c.P) < 240);
        if (n >= 6 * c.P && n % c.P == c.P / 2) held += s.v[0].gate ? 1.0f : 0.0f;
        if (n >= 4 * c.P && n % c.P == 300) cnt[s.articEff]++;
      }
      int best = 0; for (int a = 1; a < ART_COUNT; ++a) if (cnt[a] > cnt[best]) best = a;
      if (held < 5.0f || best != c.want) { std::printf("FAIL %s (held %.0f/6, got %d)\n", c.name, held, best); ++fails; }
    }
  }

  // v1.43: a model change while playing fades out, switches, fades in (no
  // jump); heavy beats and phrase ends raise the DIV TRIG flag
  {
    stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; s.artic = ART_LEGATO; s.rootHz = 440.0f; stringsControl(s);
    std::vector<float> o;
    for (int n = 0; n < 40000; ++n) {
      if (n == 20000) s.instrument = MDL_CELLO_SECTION;
      if ((n & 127) == 0) stringsControl(s);
      o.push_back(stringsTick(s, true));
    }
    float jump = 0; for (int n = 19000; n < 24000; ++n) jump = std::max(jump, std::fabs(o[n] - o[n - 1]));
    float before = 0; for (int n = 15000; n < 19000; ++n) before = std::max(before, std::fabs(o[n] - o[n - 1]));
    if (s.curModel != MDL_CELLO_SECTION || jump > 2.5f * before + 0.01f) { std::printf("FAIL soft model change (%.3f vs %.3f)\n", jump, before); ++fails; }
    stringsInit(s); s.instrument = MDL_SOLO_VIOLIN; s.rootHz = 440.0f; stringsControl(s);
    int beats = 0, ends = 0;
    const int L[3] = { 12000, 12000, 24000 }, G[3] = { 11000, 11000, 10000 };
    for (int bar = 0; bar < 10; ++bar)
      for (int j = 0; j < 3; ++j)
        for (int n = 0; n < L[j]; ++n) {
          if ((n & 127) == 0) stringsControl(s);
          stringsTick(s, n < G[j]);
          if (s.trigOut) { if (s.trigOut == 1) ++beats; else ++ends; s.trigOut = 0; }
        }
    if (beats < 3 || ends < 3) { std::printf("FAIL DIV TRIG flags (beats %d, phrase ends %d)\n", beats, ends); ++fails; }
  }

  if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
  std::printf("strings DSP test passed (%d instruments x %d layouts, %d articulations, intonation, "
              "pizzicato, tremolo, release, adaptive modulation, divisi output, velocity, ostinato tempo, output ceiling, deterministic phrase, phrase analysis, characters, gate-length articulation, look-ahead, down-bows, long arc, leader, aftertouch, one-string slides, legato, velocity phrasing, tempo-aware strokes, phrase ends, humanize, audio-rate FM, triggers, model fade, trigger out)\n", (int)INS_COUNT, (int)LAY_COUNT, (int)ART_COUNT);
  return 0;
}
