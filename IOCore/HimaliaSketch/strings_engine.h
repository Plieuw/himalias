// Himalia Strings beta - six-voice bowed-string physical model.
// Header-only, fixed-memory DSP for the ATSAMD51. No samples or heap allocation.
// First integration target: 47.619 kHz mono SAMPLE OUT.
//
// This is an initial bowed-string engine, not a finished instrument model.
// It uses nonlinear bow/string friction, fractional-delay string loops,
// bridge loss filtering and a shared modal body response. Parameters are
// deliberately exposed so later listening tests can tune the instrument.

#pragma once
#include <math.h>
#include <stdint.h>

#ifndef STR_SR
#define STR_SR 47619.0f
#endif

#define STR_VOICES 6
#define STR_DELAY_SIZE 2048
#define STR_BODY_MODES 8

enum StringInstrument : uint8_t {
  STR_VIOLIN = 0,
  STR_CELLO = 1
};

enum StringChord : uint8_t {
  STR_UNISON = 0,
  STR_OCTAVES,
  STR_FIFTHS,
  STR_MAJOR,
  STR_MINOR,
  STR_SUS4
};

struct StringVoice {
  float delay[STR_DELAY_SIZE];
  uint32_t write;
  float delayLength;
  float bridgeLP;
  float bridgeState;
  float bowState;
  float envelope;
  float gain;
  float frequency;
  float pan; // reserved for a future stereo hardware revision
  bool active;
};

struct StringBodyMode {
  float a1, a2, gain;
  float y1, y2;
};

struct StringsEngine {
  StringVoice voice[STR_VOICES];
  StringBodyMode body[STR_BODY_MODES];
  float rootHz;
  float bowSpeed;
  float bowPressure;
  float vibratoDepth;
  float vibratoPhase;
  float vibratoRate;
  float gateEnv;
  float outputLevel;
  uint8_t instrument;
  uint8_t chord;
  uint8_t voiceCount;
  bool ensemble;
};

static inline float strClamp(float x, float lo, float hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

static inline float strSoftClip(float x) {
  // Smooth bounded nonlinearity, inexpensive on Cortex-M4.
  return x / (1.0f + 0.35f * x * x);
}

static inline void strSetBodyMode(StringBodyMode &m, float hz,
                                  float t60, float gain) {
  if (hz < 25.0f || hz > STR_SR * 0.42f || t60 <= 0.0f) {
    m.a1 = m.a2 = m.gain = 0.0f;
    return;
  }
  const float w = 6.28318530718f * hz / STR_SR;
  const float r = expf(-6.907755f / (t60 * STR_SR));
  m.a1 = 2.0f * r * cosf(w);
  m.a2 = r * r;
  m.gain = gain * sinf(w);
}

static inline void stringsInit(StringsEngine &s) {
  s.rootHz = 220.0f;
  s.bowSpeed = 0.35f;
  s.bowPressure = 0.45f;
  s.vibratoDepth = 0.0f;
  s.vibratoPhase = 0.0f;
  s.vibratoRate = 5.2f;
  s.gateEnv = 0.0f;
  s.outputLevel = 0.20f;
  s.instrument = STR_VIOLIN;
  s.chord = STR_UNISON;
  s.voiceCount = 1;
  s.ensemble = false;

  for (int i = 0; i < STR_VOICES; ++i) {
    StringVoice &v = s.voice[i];
    for (int j = 0; j < STR_DELAY_SIZE; ++j) v.delay[j] = 0.0f;
    v.write = 0;
    v.delayLength = STR_SR / 220.0f;
    v.bridgeLP = 0.35f;
    v.bridgeState = v.bowState = v.envelope = 0.0f;
    v.gain = 0.999f;
    v.frequency = 220.0f;
    v.pan = 0.0f;
    v.active = (i == 0);
  }

  for (int i = 0; i < STR_BODY_MODES; ++i) {
    s.body[i].a1 = s.body[i].a2 = s.body[i].gain = 0.0f;
    s.body[i].y1 = s.body[i].y2 = 0.0f;
  }
}

// Rebuild the instrument body response outside the audio interrupt.
static inline void stringsSetInstrument(StringsEngine &s, uint8_t instrument) {
  s.instrument = instrument == STR_CELLO ? STR_CELLO : STR_VIOLIN;
  if (s.instrument == STR_VIOLIN) {
    const float f[STR_BODY_MODES] = { 280, 460, 720, 980, 1450, 2050, 2850, 3600 };
    const float a[STR_BODY_MODES] = { .32f, .24f, .18f, .13f, .10f, .075f, .05f, .035f };
    for (int i = 0; i < STR_BODY_MODES; ++i)
      strSetBodyMode(s.body[i], f[i], 0.22f, a[i]);
  } else {
    const float f[STR_BODY_MODES] = { 95, 165, 260, 390, 570, 820, 1250, 1900 };
    const float a[STR_BODY_MODES] = { .34f, .26f, .20f, .15f, .11f, .08f, .055f, .035f };
    for (int i = 0; i < STR_BODY_MODES; ++i)
      strSetBodyMode(s.body[i], f[i], 0.30f, a[i]);
  }
}

// Semitone offsets for a single incoming pitch CV. Voices are independently
// detuned and delayed slightly to suggest section size rather than chorus FX.
static inline int8_t stringsChordOffset(uint8_t chord, uint8_t voice) {
  static const int8_t intervals[6][6] = {
    { 0, 0, 0, 0, 0, 0 },       // unison section
    { 0, 12, 0, 12, -12, 0 },   // octave spread
    { 0, 7, 12, 7, -5, 12 },    // fifths
    { 0, 4, 7, 12, 16, 19 },    // major stack
    { 0, 3, 7, 12, 15, 19 },    // minor stack
    { 0, 5, 7, 12, 17, 19 }     // suspended
  };
  return intervals[chord < 6 ? chord : 0][voice];
}

// Set the incoming 1V/oct pitch as Hz and configure solo/section mode.
// chord is only used when ensemble=true. Call from loop(), not the ISR.
static inline void stringsControl(StringsEngine &s, float rootHz,
                                  bool ensemble, uint8_t chord,
                                  float bowSpeed, float bowPressure,
                                  float vibratoDepth, float vibratoRate) {
  s.rootHz = strClamp(rootHz, 28.0f, 1800.0f);
  s.ensemble = ensemble;
  s.chord = chord < 6 ? chord : STR_UNISON;
  s.voiceCount = ensemble ? STR_VOICES : 1;
  s.bowSpeed = strClamp(bowSpeed, 0.02f, 1.0f);
  s.bowPressure = strClamp(bowPressure, 0.02f, 1.0f);
  s.vibratoDepth = strClamp(vibratoDepth, 0.0f, 0.006f);
  s.vibratoRate = strClamp(vibratoRate, 3.0f, 9.0f);

  for (int i = 0; i < STR_VOICES; ++i) {
    StringVoice &v = s.voice[i];
    v.active = i < s.voiceCount;
    if (!v.active) continue;
    const float detuneCents = ensemble ? ((float)i - 2.5f) * 2.2f : 0.0f;
    const float semis = (float)stringsChordOffset(s.chord, (uint8_t)i);
    v.frequency = s.rootHz * exp2f((semis + detuneCents / 100.0f) / 12.0f);
    v.frequency = strClamp(v.frequency, 28.0f, 3500.0f);
    float length = STR_SR / v.frequency;
    // A little fractional-delay compensation for the bridge low-pass.
    v.bridgeLP = s.instrument == STR_VIOLIN ? 0.34f : 0.25f;
    length -= 0.5f + (1.0f - v.bridgeLP) * 0.35f;
    v.delayLength = strClamp(length, 4.0f, STR_DELAY_SIZE - 4.0f);
    const float t60 = s.instrument == STR_VIOLIN ? 3.2f : 4.8f;
    v.gain = powf(0.001f, 1.0f / (t60 * v.frequency));
  }
}

// Call once per audio sample. gate is the note gate; output is mono.
static inline float stringsTick(StringsEngine &s, bool gate) {
  s.gateEnv += ((gate ? 1.0f : 0.0f) - s.gateEnv) *
               (gate ? 0.00075f : 0.00018f);
  s.vibratoPhase += s.vibratoRate / STR_SR;
  if (s.vibratoPhase >= 1.0f) s.vibratoPhase -= 1.0f;
  const float vib = sinf(6.28318530718f * s.vibratoPhase) * s.vibratoDepth;
  float mix = 0.0f;

  for (int i = 0; i < STR_VOICES; ++i) {
    StringVoice &v = s.voice[i];
    if (!v.active) continue;

    const float voiceVib = sinf(6.28318530718f * (s.vibratoPhase + (float)i * 0.13f)) * s.vibratoDepth;\n    const float modulatedLength = v.delayLength * exp2f(-voiceVib * 1.44269504f);\n    float rp = (float)v.write - modulatedLength;
    if (rp < 0.0f) rp += STR_DELAY_SIZE;
    const uint32_t i0 = ((uint32_t)rp) & (STR_DELAY_SIZE - 1);
    const uint32_t i1 = (i0 + 1) & (STR_DELAY_SIZE - 1);
    const float frac = rp - floorf(rp);
    const float travelling = v.delay[i0] +
                             (v.delay[i1] - v.delay[i0]) * frac;

    // Bow friction: relative bow/string velocity enters a saturating
    // stick-slip characteristic. Bow pressure controls coupling strength.
    const float bowVelocity = s.bowSpeed * s.gateEnv;
    const float relativeVelocity = bowVelocity - travelling * 2.4f;
    const float friction = strSoftClip(relativeVelocity * 7.0f) *
                           s.bowPressure * s.gateEnv;
    v.bowState += (friction - v.bowState) * 0.18f;

    // Bridge loss and string round-trip. The small nonlinear term prevents
    // an unnaturally sterile loop; no random/noise source is required.
    v.bridgeState += (travelling - v.bridgeState) * v.bridgeLP;
    float next = v.bridgeState * v.gain + v.bowState * 0.42f;
    next = strSoftClip(next * 1.8f);
    v.delay[v.write] = next;
    v.write = (v.write + 1) & (STR_DELAY_SIZE - 1);

    // Slow onset/release smoothing avoids clicks on gate transitions.
    v.envelope += ((gate ? 1.0f : 0.0f) - v.envelope) *
                  (gate ? 0.0012f : 0.00025f);
    mix += travelling * v.envelope;
  }

  // Shared instrument-body modal bank.
  float bodyOut = 0.0f;
  for (int i = 0; i < STR_BODY_MODES; ++i) {
    StringBodyMode &m = s.body[i];
    const float y = m.a1 * m.y1 - m.a2 * m.y2 + m.gain * mix;
    m.y2 = m.y1;
    m.y1 = y;
    bodyOut += y;
  }
  const float dry = mix * 0.35f;
  const float out = dry + bodyOut * 0.18f;
  return strSoftClip(out * s.outputLevel);
}

// Clear delay/body state when changing instrument or patch mode.
static inline void stringsClear(StringsEngine &s) {
  for (int i = 0; i < STR_VOICES; ++i) {
    StringVoice &v = s.voice[i];
    for (int j = 0; j < STR_DELAY_SIZE; ++j) v.delay[j] = 0.0f;
    v.write = 0;
    v.bridgeState = v.bowState = v.envelope = 0.0f;
  }
  for (int i = 0; i < STR_BODY_MODES; ++i)
    s.body[i].y1 = s.body[i].y2 = 0.0f;
  s.gateEnv = 0.0f;
}
