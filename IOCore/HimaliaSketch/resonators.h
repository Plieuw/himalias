//
//  Himalia resonator models
//
//  Five physical-modelling voices, all excited by a strike (trigger), by a held
//  gate ("bow" / "blow") and/or by external audio:
//
//    0  STRING   plucked / struck string (Karplus-Strong with loop filter)
//    1  BAR      wooden / metal bar, marimba-like modal resonator
//    2  BELL     bell <-> glass: inharmonic modal resonator, timbre morphs
//    3  DRUM     membrane: modal resonator with a pitch drop after the strike
//    4  WIND     flute <-> brass: self-oscillating blown tube
//
//  Split in two halves:
//    resControl() runs in loop(): pitch, timbre, model, decay -> coefficients
//    resTick()    runs in the audio interrupt: one output sample
//
//  Plain C++ (only <math.h>), so it can also be tested on a computer.
//
#pragma once
#include <math.h>
#include <stdint.h>

#define RES_SR         47619.0f
#define RES_MODELS     5
#define RES_MODES      10          // modal voices
#define RES_STR_MAX    4096        // string / bore delay length (lowest ~12 Hz)
#define RES_JET_MAX    2048

enum { RES_STRING = 0, RES_BAR, RES_BELL, RES_DRUM, RES_WIND };

struct ResState {
  // ---- control (written by resControl)
  volatile int   model;
  volatile float f0;            // Hz
  volatile float timbre;        // 0 ... 1
  volatile float decay;         // 1 = short, 4 = long
  volatile bool  sustain;       // excite continuously while the gate is high

  // ---- excitation
  uint32_t rng;
  float    burst;               // strike envelope
  float    burst_lp;            // filtered noise for the strike
  float    gate_env;            // held gate envelope (bow / blow)
  float    drum_env;            // pitch drop envelope (drum)
  uint32_t mallet_n, mallet_len; // mallet pulse (modal models)
  float    mallet_scale;

  // ---- modal
  float m_a1[RES_MODES], m_a2[RES_MODES], m_g[RES_MODES];
  float m_y1[RES_MODES], m_y2[RES_MODES];
  int   m_n;

  // ---- string / wind waveguide
  float dl[RES_STR_MAX];
  float jet[RES_JET_MAX];
  uint32_t dl_w, jet_w;
  volatile float dl_len;        // samples (fractional)
  volatile float jet_len;
  volatile float loop_gain;     // per round trip
  volatile float loop_lp;       // loop lowpass coefficient (0 = dark ... 1 = bright)
  float lp_state, dc_x, dc_y;
  volatile float wind_brass;    // 0 = flute, 1 = brass
  volatile float wind_boost;    // loop gain above 1 while blowing (per round trip)
  volatile float wind_damp;     // loop loss after the breath stops
};

static inline float resNoise(ResState &r) {
  r.rng ^= r.rng << 13; r.rng ^= r.rng >> 17; r.rng ^= r.rng << 5;
  return (float)(int32_t)r.rng * (1.0f / 2147483648.0f);
}

static void resInit(ResState &r) {
  r.model = RES_STRING; r.f0 = 110.0f; r.timbre = 0.5f; r.decay = 1.0f; r.sustain = false;
  r.rng = 0x9E3779B9u; r.burst = 0; r.burst_lp = 0; r.gate_env = 0; r.drum_env = 0;
  r.mallet_n = 0; r.mallet_len = 0; r.mallet_scale = 0;
  for (int i = 0; i < RES_MODES; i++) { r.m_a1[i] = r.m_a2[i] = r.m_g[i] = r.m_y1[i] = r.m_y2[i] = 0; }
  r.m_n = 0;
  for (int i = 0; i < RES_STR_MAX; i++) r.dl[i] = 0;
  for (int i = 0; i < RES_JET_MAX; i++) r.jet[i] = 0;
  r.dl_w = r.jet_w = 0; r.dl_len = 400; r.jet_len = 150; r.loop_gain = 0.99f; r.loop_lp = 0.5f;
  r.lp_state = r.dc_x = r.dc_y = 0; r.wind_brass = 0; r.wind_boost = 0.1f; r.wind_damp = 0.05f;
}

// ---------------------------------------------------------------- modal sets
// frequency ratio, relative amplitude, relative decay (1 = fundamental)
struct ModeSet { float ratio[RES_MODES]; float amp[RES_MODES]; float dec[RES_MODES]; float t60; };

static const ModeSet MS_BAR = {      // marimba-like tuned bar
  { 1.0f, 3.99f, 9.9f, 17.0f, 26.0f, 36.6f, 2.0f, 0, 0, 0 },
  { 1.0f, 0.55f, 0.30f, 0.16f, 0.09f, 0.05f, 0.04f, 0, 0, 0 },
  { 1.0f, 0.45f, 0.25f, 0.15f, 0.10f, 0.07f, 0.6f, 0, 0, 0 },
  1.2f };
static const ModeSet MS_BELL = {     // church bell partials
  { 0.5f, 1.0f, 1.2f, 1.5f, 2.0f, 2.5f, 2.66f, 3.0f, 4.0f, 5.33f },
  { 0.55f, 0.6f, 0.5f, 0.35f, 1.0f, 0.3f, 0.25f, 0.3f, 0.2f, 0.15f },
  { 1.6f, 1.0f, 0.9f, 0.7f, 0.8f, 0.5f, 0.45f, 0.4f, 0.3f, 0.25f },
  4.0f };
static const ModeSet MS_GLASS = {    // wine glass / metal plate
  { 1.0f, 2.32f, 4.25f, 6.63f, 9.38f, 12.9f, 1.41f, 3.17f, 5.4f, 7.9f },
  { 1.0f, 0.6f, 0.45f, 0.3f, 0.2f, 0.12f, 0.25f, 0.2f, 0.15f, 0.1f },
  { 1.0f, 0.8f, 0.6f, 0.45f, 0.35f, 0.25f, 0.7f, 0.5f, 0.4f, 0.3f },
  3.0f };
static const ModeSet MS_DRUM = {     // circular membrane
  { 1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f, 3.600f, 4.059f },
  { 1.0f, 0.7f, 0.55f, 0.45f, 0.38f, 0.3f, 0.25f, 0.2f, 0.17f, 0.14f },
  { 1.0f, 0.6f, 0.5f, 0.45f, 0.4f, 0.35f, 0.3f, 0.28f, 0.25f, 0.22f },
  0.35f };

// set one 2-pole resonator: frequency f (Hz), decay t60 (s), amplitude
static inline void resMode(ResState &r, int i, float f, float t60, float amp) {
  if (f <= 10.0f || f > 0.45f * RES_SR || amp <= 0.0f) { r.m_a1[i] = r.m_a2[i] = r.m_g[i] = 0; return; }
  const float w  = 6.2831853f * f / RES_SR;
  const float rr = expf(-6.9078f / (t60 * RES_SR));        // -60 dB after t60
  r.m_a1[i] = 2.0f * rr * cosf(w);
  r.m_a2[i] = rr * rr;
  r.m_g[i]  = amp * sinf(w);                                // impulse response amplitude ~ amp
}

// phase delay (samples) of the loop lowpass y += (x - y) * k at frequency f
static inline float resLpDelay(float k, float f) {
  const float w = 6.2831853f * f / RES_SR;
  const float a = 1.0f - k;
  return atan2f(a * sinf(w), 1.0f - a * cosf(w)) / w;
}

// ---------------------------------------------------------------- control (loop)
static void resControl(ResState &r) {
  const float f0 = r.f0, tb = r.timbre, dk = r.decay;
  switch (r.model) {
    case RES_STRING: {
      float len = RES_SR / f0;
      if (len > RES_STR_MAX - 4) len = RES_STR_MAX - 4;
      if (len < 4) len = 4;
      r.dl_len = len;
      r.loop_lp = 0.15f + 0.84f * tb;                         // dark nylon ... bright steel
      len -= resLpDelay(r.loop_lp, f0);                       // the loop lowpass adds delay
      if (len < 4) len = 4;
      r.dl_len = len;
      const float t60 = 1.2f * dk * (0.6f + 0.6f * (1.0f - tb));
      r.loop_gain = powf(0.001f, 1.0f / (t60 * f0));
      r.m_n = 0;
      break;
    }
    case RES_WIND: {
      float len = RES_SR / f0;
      if (len > RES_STR_MAX - 4) len = RES_STR_MAX - 4;
      if (len < 8) len = 8;
      r.dl_len = len;
      r.wind_brass = tb;
      float k = 0.35f + 0.6f * tb;                            // soft flute ... bright brass
      const float kmin = 0.35f + f0 * (1.0f / 2500.0f);       // high notes need a brighter tube
      if (k < kmin) k = kmin;
      if (k > 0.97f) k = 0.97f;
      r.loop_lp = k;
      len -= resLpDelay(r.loop_lp, f0);                       // the loop lowpass adds delay
      if (len < 8) len = 8;
      r.dl_len = len;
      // same build-up speed for every note: ~e^30 per second while blowing,
      // -60 dB in ~0.15 s (x decay) after the breath stops
      // the loop lowpass also loses level at the note: make that up first
      const float w  = 6.2831853f * f0 / RES_SR;
      const float aa = 1.0f - r.loop_lp;
      const float mag = r.loop_lp / sqrtf((1.0f - aa * cosf(w)) * (1.0f - aa * cosf(w)) + aa * aa * sinf(w) * sinf(w));
      float boost = expf(30.0f / f0) / mag - 1.0f;
      if (boost > 1.5f) boost = 1.5f;
      r.wind_boost = boost;
      r.wind_damp = 1.0f - powf(0.001f, 1.0f / (0.15f * dk * f0));
      r.m_n = 0;
      break;
    }
    default: {
      const ModeSet *a = &MS_BAR;
      const ModeSet *b = 0;
      float mix = 0, pitch = f0;
      if (r.model == RES_BELL) { a = &MS_BELL; b = &MS_GLASS; mix = tb; }
      if (r.model == RES_DRUM) { a = &MS_DRUM; pitch = f0 * (1.0f + 0.6f * r.drum_env); }
      // timbre = mallet hardness: soft mallet hides the upper modes (bar / drum)
      for (int i = 0; i < RES_MODES; i++) {
        float ratio = a->ratio[i], amp = a->amp[i], dec = a->dec[i], t60 = a->t60;
        if (b) {
          ratio += (b->ratio[i] - ratio) * mix;
          amp   += (b->amp[i] - amp) * mix;
          dec   += (b->dec[i] - dec) * mix;
          t60   += (b->t60 - t60) * mix;
        } else if (i > 0) {
          amp *= powf(0.15f + 0.85f * tb, (float)i * 0.6f);
        }
        resMode(r, i, pitch * ratio, t60 * dec * dk, amp);
      }
      r.m_n = RES_MODES;
      break;
    }
  }
}

// ---------------------------------------------------------------- audio (interrupt)
// strike: true on the trigger edge; gate: trigger held; ext: external audio (-1 ... 1)
static inline float resTick(ResState &r, bool strike, bool gate, float ext) {
  const float tb = r.timbre;

  // excitation: short noise burst (mallet / pick), its colour from the timbre
  if (strike) {
    r.burst = 1.0f;
    // mallet length: 0.3 ms (hard) ... 4 ms (soft), but never longer than half
    // a period, so a soft mallet still sounds the fundamental on high notes.
    // The bell uses its timbre for bell <-> glass, so it gets a medium mallet.
    float ml = (r.model == RES_BELL) ? 0.0008f : (0.0003f + 0.0037f * (1.0f - tb) * (1.0f - tb));
    const float half_period = 0.5f / r.f0;
    if (ml > half_period) ml = half_period;
    r.mallet_len = (uint32_t)(RES_SR * ml);
    if (r.mallet_len < 2) r.mallet_len = 2;
    r.mallet_scale = 3.14159265f / (2.0f * (float)r.mallet_len);
    r.mallet_n = 0;
  }
  const float n = resNoise(r);
  r.burst_lp += (n - r.burst_lp) * (0.08f + 0.9f * tb * tb);
  float exc = r.burst_lp * r.burst;
  r.burst *= 0.9965f - 0.0025f * tb;                         // 2 ... 6 ms burst

  // held gate: bow / blow envelope
  if (gate) r.gate_env += (1.0f - r.gate_env) * 0.0015f;    // ~15 ms
  else      r.gate_env -= r.gate_env * 0.0006f;              // ~35 ms
  if (r.sustain && r.model != RES_WIND) exc += r.burst_lp * r.gate_env * 0.15f;

  exc += ext;

  // drum pitch drop envelope
  if (strike) r.drum_env = 1.0f;
  r.drum_env *= 0.9994f;                                     // ~35 ms

  float out = 0.0f;
  switch (r.model) {
    case RES_STRING: {
      const float len = r.dl_len;
      float rp = (float)r.dl_w - len;
      if (rp < 0) rp += RES_STR_MAX;
      uint32_t i0 = (uint32_t)rp;
      const float fr = rp - (float)i0;
      const uint32_t i1 = (i0 + 1) & (RES_STR_MAX - 1);
      i0 &= (RES_STR_MAX - 1);
      const float d = r.dl[i0] + (r.dl[i1] - r.dl[i0]) * fr;
      r.lp_state += (d - r.lp_state) * r.loop_lp;
      float y = r.lp_state * r.loop_gain + exc * (0.6f + 1.4f * (1.0f - tb));   // dark pluck needs more energy
      if (y > 1.5f) y = 1.5f; if (y < -1.5f) y = -1.5f;
      r.dl[r.dl_w] = y;
      r.dl_w = (r.dl_w + 1) & (RES_STR_MAX - 1);
      out = d * 1.2f;
      break;
    }
    case RES_WIND: {
      // Blown tube: a delay-line loop whose gain goes above 1 while you blow,
      // so it starts to oscillate by itself; a soft saturation sets the level.
      // Breath noise makes it airy (flute); towards brass the saturation gets
      // harder and the tube brighter.
      float env = r.gate_env;
      if (r.burst > env) env = r.burst;
      const float brass = r.wind_brass;
      const float len = r.dl_len;
      float rp = (float)r.dl_w - len;
      if (rp < 0) rp += RES_STR_MAX;
      uint32_t i0 = (uint32_t)rp;
      const float fr = rp - (float)i0;
      const uint32_t i1 = (i0 + 1) & (RES_STR_MAX - 1);
      i0 &= (RES_STR_MAX - 1);
      const float d = r.dl[i0] + (r.dl[i1] - r.dl[i0]) * fr;
      r.lp_state += (d - r.lp_state) * r.loop_lp;            // tube losses
      r.dc_y = r.lp_state - r.dc_x + 0.995f * r.dc_y;        // keep it centred
      r.dc_x = r.lp_state;
      const float gain = 1.0f + r.wind_boost * env - r.wind_damp * (1.0f - env);   // > 1 while blowing
      float x = r.dc_y * gain
              + n * env * (0.05f - 0.035f * brass)           // breath noise
              + ext * 0.3f;
      x *= 1.0f + 1.5f * brass;                              // harder drive towards brass
      if (x > 1.5f) x = 1.5f; if (x < -1.5f) x = -1.5f;
      x = x - x * x * x * (1.0f / 3.375f);                  // soft clip: +-1 at +-1.5
      x *= 1.0f / (1.0f + 1.0f * brass);
      r.dl[r.dl_w] = x;
      r.dl_w = (r.dl_w + 1) & (RES_STR_MAX - 1);
      out = d * 0.8f;
      break;
    }
    default: {
      // mallet: half-sine pulse, short = hard (bright), long = soft (dark),
      // normalised to unit area so the loudness does not depend on hardness
      float x = 0.0f;
      if (r.mallet_n < r.mallet_len) {
        x = sinf(3.14159265f * (float)r.mallet_n / (float)r.mallet_len) * r.mallet_scale;
        r.mallet_n++;
      }
      x += exc * 0.02f;
      for (int i = 0; i < RES_MODES; i++) {
        const float y = r.m_a1[i] * r.m_y1[i] - r.m_a2[i] * r.m_y2[i] + r.m_g[i] * x;
        r.m_y2[i] = r.m_y1[i];
        r.m_y1[i] = y;
        out += y;
      }
      out *= 0.5f;
      break;
    }
  }
  // gentle saturation so loud strikes do not clip hard
  if (out > 3.0f) out = 3.0f; if (out < -3.0f) out = -3.0f;
  return out * (1.0f - out * out * (1.0f / 27.0f)) * 1.0f;
}

// clear all resonator states (model change)
static void resClear(ResState &r) {
  for (int i = 0; i < RES_MODES; i++) r.m_y1[i] = r.m_y2[i] = 0;
  for (int i = 0; i < RES_STR_MAX; i++) r.dl[i] = 0;
  for (int i = 0; i < RES_JET_MAX; i++) r.jet[i] = 0;
  r.lp_state = r.dc_x = r.dc_y = 0;
}
