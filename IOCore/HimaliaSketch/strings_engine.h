//
//  Himalia Strings -- bowed-string physical model, version 2
//
//  Header-only, fixed memory, no samples, no heap. Plain C++ + <math.h>, so the
//  same code runs on the ATSAMD51 and in the host tests (tests/).
//
//  Per voice: a digital waveguide string with the bow at a real position
//  between bridge and nut (two delay segments), a bow/string friction junction
//  (stick-slip: the string sticks to the bow until the velocity difference
//  gets too big, then slips back -- Helmholtz motion), a lossy bridge
//  reflection and a rigid nut. The force at the bridge drives an instrument
//  body (modal resonances) per instrument family: violin, viola, cello, bass.
//
//  Ensemble: up to six voices from one pitch CV. Each voice has its own
//  intonation (static offset + slow drift), vibrato rate/depth/phase with a
//  delayed onset, bow speed and pressure differences and a small onset delay,
//  so a section sounds like players, not like a stack of oscillators.
//
//  Split in two halves:
//    stringsControl()  in loop(): pitch, instrument, layout, bow -> targets
//    stringsTick()     in the audio interrupt: one output sample
//
#pragma once
#include <math.h>
#include <stdint.h>

#ifndef STR_SR
#define STR_SR 47619.0f
#endif

#define STR_VOICES     6
#define STR_NECK_LEN   2048          // nut side segment (lowest note ~ 23 Hz)
#define STR_BRIDGE_LEN 512           // bridge side segment (bow position <= 0.3)
#define STR_FAMILIES   4
#define STR_BODY_MODES 6

enum StrFamily     : uint8_t { FAM_VIOLIN = 0, FAM_VIOLA, FAM_CELLO, FAM_BASS };
enum StrInstrument : uint8_t { INS_SOLO_VIOLIN = 0, INS_SOLO_CELLO, INS_VIOLIN_SECTION,
                               INS_CELLO_SECTION, INS_FULL_STRINGS, INS_COUNT };
enum StrLayout     : uint8_t { LAY_UNISON = 0, LAY_OCTAVES, LAY_FIFTHS, LAY_MAJOR, LAY_MINOR,
                               LAY_DIVISI_MAJOR, LAY_DIVISI_MINOR, LAY_COUNT };

// lowest open string per family (Hz): notes below are played an octave up
static const float STR_FAMILY_LOW[STR_FAMILIES] = { 196.0f, 130.8f, 65.4f, 41.2f };

struct StrVoice {
  float neck[STR_NECK_LEN];
  float bridge[STR_BRIDGE_LEN];
  uint32_t nw, bw;                // write positions
  float len, lenTarget;           // loop length in samples (glides to target)
  float lp;                       // bridge loss filter state
  float env;                      // bow speed envelope (0 ... 1)
  float vs, vc;                   // vibrato oscillator (sin / cos)
  float vibK;                     // vibrato rate coefficient (2 pi f / sr)
  float vibAmt;                   // vibrato depth, fades in after the onset
  float vibDepth;                 // this player's depth
  float tremPh, tremInc;          // tremolo bow strokes
  float speedScale, pressOffs;    // this player's bow
  float detune;                   // static intonation (ratio)
  float drift, driftTarget;       // slow intonation drift (cents)
  float gain;
  uint32_t onset;                 // samples until this player starts / stops
  uint32_t pluckN, pluckLen;      // pizzicato pluck pulse
  uint8_t family;
  bool active, gate, pluck;
};

struct StrMode { float a1, a2, g, y1, y2; };

struct StringsEngine {
  StrVoice v[STR_VOICES];
  StrMode body[STR_FAMILIES][STR_BODY_MODES];
  float bodyDirect[STR_FAMILIES];
  // control (written from loop)
  float rootHz;
  float speed;                    // bow speed 0 ... 1
  float pressure;                 // bow pressure 0 ... 1
  float beta;                     // bow position: 0.04 ponticello ... 0.3 tasto
  float speedCV;                  // expression CV multiplier (1 = none)
  bool  pizz, vibrato, tremolo;
  uint8_t instrument, layout;
  // divisi from the generative register
  uint8_t divisiMask;
  int8_t  divisiTone[STR_VOICES];
  // internal
  bool  gatePrev;
  float slope;                    // friction curve slope from the pressure
  float loss;                     // bridge reflection gain per round trip
  float bridgeK;                  // bridge lowpass coefficient
  float lenComp;                  // loop delay compensation (samples)
  float dcx, dcy, level;
  uint8_t famUsed;                // bodies with players
  uint32_t rng;
};

// ------------------------------------------------------------------ helpers
static inline float strRand(StringsEngine &s) {            // -1 ... 1
  s.rng ^= s.rng << 13; s.rng ^= s.rng >> 17; s.rng ^= s.rng << 5;
  return (float)(int32_t)s.rng * (1.0f / 2147483648.0f);
}
static inline float strClamp(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

static inline void strMode(StrMode &m, float hz, float q, float gain) {
  if (hz < 20.0f || hz > 0.42f * STR_SR) { m.a1 = m.a2 = m.g = 0; return; }
  const float w = 6.2831853f * hz / STR_SR;
  const float r = expf(-3.14159265f * hz / (q * STR_SR));   // bandwidth = hz / q
  m.a1 = 2.0f * r * cosf(w);
  m.a2 = r * r;
  m.g  = gain * (1.0f - r) * 2.0f * sinf(w);                 // peak gain ~ gain
}

// Body resonances per family (main air / wood modes, bridge hill).
// frequency (Hz), Q, gain
static const float STR_BODY[STR_FAMILIES][STR_BODY_MODES][3] = {
  { { 275, 18, 1.0f }, { 460, 22, 1.3f }, { 550, 20, 1.1f }, { 1100, 8, 0.6f }, { 2500, 4, 1.0f }, { 3300, 6, 0.5f } },  // violin
  { { 230, 16, 1.0f }, { 380, 20, 1.3f }, { 460, 18, 1.1f }, {  900, 8, 0.6f }, { 2100, 4, 0.9f }, { 2900, 6, 0.4f } },  // viola
  { { 100, 14, 1.1f }, { 175, 18, 1.3f }, { 205, 16, 1.1f }, {  440, 8, 0.6f }, { 1500, 4, 0.7f }, { 2200, 6, 0.3f } },  // cello
  { {  60, 12, 1.2f }, { 100, 16, 1.2f }, { 125, 14, 1.0f }, {  260, 8, 0.6f }, {  900, 4, 0.5f }, { 1500, 6, 0.2f } },  // bass
};
static const float STR_BODY_DIRECT[STR_FAMILIES] = { 0.35f, 0.4f, 0.45f, 0.5f };

// ------------------------------------------------------------------ layouts
// semitones per voice. Sections use one family; full strings uses
// violin I, violin II, viola, cello, cello, bass.
static const int8_t STR_SECTION_LAYOUT[LAY_COUNT][STR_VOICES] = {
  { 0, 0, 0, 0, 0, 0 },          // unison
  { 0, 0, 12, 12, 0, -12 },      // octaves
  { 0, 0, 7, 7, 12, -5 },        // fifths
  { 0, 4, 7, 12, 16, 19 },       // major
  { 0, 3, 7, 12, 15, 19 },       // minor
  { 0, 0, 0, 0, 0, 0 },          // divisi (tones come from the register)
  { 0, 0, 0, 0, 0, 0 },
};
static const int8_t STR_FULL_LAYOUT[LAY_COUNT][STR_VOICES] = {
  { 0, 0, 0, -12, -12, -24 },    // unison: everybody on the note, in their register
  { 12, 0, 0, -12, -12, -24 },   // octaves
  { 7, 0, -5, -12, -17, -24 },   // fifths
  { 16, 7, 4, -12, -5, -24 },    // major
  { 15, 7, 3, -12, -5, -24 },    // minor
  { 0, 0, 0, -12, -12, -24 },    // divisi: register adds chord tones on top
  { 0, 0, 0, -12, -12, -24 },
};
static const uint8_t STR_FULL_FAMILY[STR_VOICES] = { FAM_VIOLIN, FAM_VIOLIN, FAM_VIOLA, FAM_CELLO, FAM_CELLO, FAM_BASS };
static const int8_t STR_DIVISI_MAJOR[8] = { 0, 4, 7, 12, 16, 19, -5, 24 };
static const int8_t STR_DIVISI_MINOR[8] = { 0, 3, 7, 12, 15, 19, -5, 24 };

// ------------------------------------------------------------------ init
static inline void stringsInit(StringsEngine &s) {
  s.rng = 0x2545F491u;
  for (int f = 0; f < STR_FAMILIES; f++) {
    for (int m = 0; m < STR_BODY_MODES; m++) {
      strMode(s.body[f][m], STR_BODY[f][m][0], STR_BODY[f][m][1], STR_BODY[f][m][2]);
      s.body[f][m].y1 = s.body[f][m].y2 = 0;
    }
    s.bodyDirect[f] = STR_BODY_DIRECT[f];
  }
  for (int i = 0; i < STR_VOICES; i++) {
    StrVoice &v = s.v[i];
    for (int k = 0; k < STR_NECK_LEN; k++) v.neck[k] = 0;
    for (int k = 0; k < STR_BRIDGE_LEN; k++) v.bridge[k] = 0;
    v.nw = v.bw = 0;
    v.len = v.lenTarget = STR_SR / 220.0f;
    v.lp = v.env = 0;
    v.vs = 0; v.vc = 1;
    // every player gets their own vibrato rate, depth, bow and intonation
    v.vibK = 6.2831853f * (5.0f + 1.2f * (0.5f + 0.5f * strRand(s))) / STR_SR;
    v.vibAmt = 0;
    v.vibDepth = 0.010f * (0.75f + 0.5f * (0.5f + 0.5f * strRand(s)));   // ~ +-17 cents
    v.tremPh = 0.5f * (0.5f + 0.5f * strRand(s));
    v.tremInc = (12.0f + 2.0f * strRand(s)) / STR_SR;
    v.speedScale = 1.0f + 0.10f * strRand(s);
    v.pressOffs = 0.06f * strRand(s);
    v.detune = exp2f((6.0f * strRand(s)) / 1200.0f);                    // +-6 cents
    v.drift = v.driftTarget = 0;
    v.gain = 1.0f;
    v.onset = 0; v.pluckN = v.pluckLen = 0;
    v.family = FAM_VIOLIN;
    v.active = (i == 0); v.gate = false; v.pluck = false;
  }
  s.rootHz = 220.0f; s.speed = 0.5f; s.pressure = 0.5f; s.beta = 0.12f; s.speedCV = 1.0f;
  s.pizz = false; s.vibrato = true; s.tremolo = false;
  s.instrument = INS_SOLO_VIOLIN; s.layout = LAY_UNISON;
  s.divisiMask = 0x3F;
  for (int i = 0; i < STR_VOICES; i++) s.divisiTone[i] = 0;
  s.gatePrev = false;
  s.slope = 3.0f; s.loss = 0.95f; s.bridgeK = 0.7f; s.lenComp = 0.0f;
  s.dcx = s.dcy = 0; s.level = 1.0f; s.famUsed = 1;
}

// phase delay (samples) of y += (x - y) * k at frequency f
static inline float strLpDelay(float k, float f) {
  const float w = 6.2831853f * f / STR_SR;
  const float a = 1.0f - k;
  return atan2f(a * sinf(w), 1.0f - a * cosf(w)) / w;
}

// ------------------------------------------------------------------ control (loop)
static inline void stringsControl(StringsEngine &s) {
  const uint8_t ins = s.instrument < INS_COUNT ? s.instrument : 0;
  const uint8_t lay = s.layout < LAY_COUNT ? s.layout : 0;
  const bool solo = (ins == INS_SOLO_VIOLIN || ins == INS_SOLO_CELLO);
  const bool full = (ins == INS_FULL_STRINGS);
  const bool divisi = (lay == LAY_DIVISI_MAJOR || lay == LAY_DIVISI_MINOR);
  const int8_t *pool = (lay == LAY_DIVISI_MAJOR) ? STR_DIVISI_MAJOR : STR_DIVISI_MINOR;

  // bow: pressure sets the friction curve (higher pressure = longer sticking,
  // more grit), bridge losses a little lower for pizzicato so it rings
  s.slope   = 5.0f - 4.0f * strClamp(s.pressure, 0.0f, 1.0f);
  s.loss    = s.pizz ? 0.996f : 0.95f;
  // string damping follows the bow position: near the bridge (ponticello) the
  // tone is glassy and bright, over the fingerboard (tasto) soft and dark
  const float b = strClamp(s.beta, 0.04f, 0.30f);
  s.bridgeK = (s.pizz ? 0.55f : 0.90f) - 1.9f * (b - 0.04f);

  int nActive = 0;
  for (int i = 0; i < STR_VOICES; i++) {
    StrVoice &v = s.v[i];
    bool on = solo ? (i == 0) : true;
    if (divisi && !solo) on = (i == 0) || ((s.divisiMask >> i) & 1);
    v.active = on;
    if (!on) continue;
    nActive++;

    uint8_t fam;
    int semis;
    if (solo)      { fam = (ins == INS_SOLO_CELLO) ? FAM_CELLO : FAM_VIOLIN; semis = 0; }
    else if (full) { fam = STR_FULL_FAMILY[i]; semis = STR_FULL_LAYOUT[lay][i]; }
    else           { fam = (ins == INS_CELLO_SECTION) ? FAM_CELLO : FAM_VIOLIN; semis = STR_SECTION_LAYOUT[lay][i]; }
    if (divisi && !solo && i > 0) semis += pool[s.divisiTone[i] & 7];
    v.family = fam;

    // slow intonation drift: every player wanders a few cents
    if ((s.rng & 0xFF) < 2) v.driftTarget = 4.0f * strRand(s);
    strRand(s);
    v.drift += (v.driftTarget - v.drift) * 0.002f;

    float hz = s.rootHz * exp2f((float)semis / 12.0f);
    while (hz < STR_FAMILY_LOW[fam]) hz *= 2.0f;            // keep it on the instrument
    if (hz > 3500.0f) hz = 3500.0f;
    if (!solo) hz *= v.detune;
    hz *= exp2f(v.drift / 1200.0f);
    // loop length minus the delay of the bridge filter and the junction
    float len = STR_SR / hz - strLpDelay(s.bridgeK, hz) - s.lenComp;
    v.lenTarget = strClamp(len, 8.0f, (float)(STR_NECK_LEN - 8));

    // family balance in the full section; high notes lose level in the
    // short string loop, give them some back
    v.gain = (fam == FAM_BASS) ? 0.85f : (fam == FAM_VIOLA ? 0.95f : (fam == FAM_CELLO ? 0.8f : 1.0f));
    const float hi = hz * (1.0f / (2.5f * STR_FAMILY_LOW[fam]));
    if (hi > 1.0f) v.gain *= strClamp(hi, 1.0f, 2.0f);
  }
  // keep the sum at a similar level for 1 ... 6 players
  s.level = 1.0f / sqrtf((float)(nActive > 0 ? nActive : 1));
  if (solo) s.level *= 1.3f;
  // which instrument bodies are in use
  uint8_t used = 0;
  for (int i = 0; i < STR_VOICES; i++) if (s.v[i].active) used |= (uint8_t)(1 << s.v[i].family);
  s.famUsed = used;
}

// ------------------------------------------------------------------ audio (interrupt)
static inline __attribute__((always_inline)) float strRead(const float *buf, uint32_t mask, uint32_t w, float d) {
  float rp = (float)w - d;
  if (rp < 0.0f) rp += (float)(mask + 1);
  uint32_t i0 = (uint32_t)rp;
  const float fr = rp - (float)i0;
  i0 &= mask;
  const uint32_t i1 = (i0 + 1) & mask;
  return buf[i0] + (buf[i1] - buf[i0]) * fr;
}

// gate: bow on (arco) / pluck on the rising edge (pizzicato)
static inline float stringsTick(StringsEngine &s, bool gate) {
  // ---- gate edges: every player gets a small onset delay in a section
  const bool solo = (s.instrument == INS_SOLO_VIOLIN || s.instrument == INS_SOLO_CELLO);
  if (gate != s.gatePrev) {
    for (int i = 0; i < STR_VOICES; i++) {
      StrVoice &v = s.v[i];
      const float r = 0.5f + 0.5f * strRand(s);
      v.onset = solo ? 0 : (uint32_t)(r * (gate ? 0.035f : 0.015f) * STR_SR);
    }
    s.gatePrev = gate;
  }

  const float bowMax = (0.03f + 0.20f * s.speed) * s.speedCV;
  float famBus[STR_FAMILIES] = { 0, 0, 0, 0 };

  for (int i = 0; i < STR_VOICES; i++) {
    StrVoice &v = s.v[i];
    if (!v.active) continue;

    // this player's gate (after the onset delay)
    if (v.onset) v.onset--;
    else if (v.gate != gate) {
      v.gate = gate;
      if (gate) {
        if (s.pizz) {                                        // pluck
          v.pluck = true; v.pluckN = 0;
          v.pluckLen = (uint32_t)(v.len * 0.25f) + 2;
        }
        v.vibAmt = 0;                                        // vibrato starts later
        if (v.env < 0.05f) v.len = v.lenTarget;              // new note: no glide
      }
    }

    // legato: glide to the new pitch while the bow keeps going (~30 ms)
    v.len += (v.lenTarget - v.len) * 0.0007f;

    // bow envelope: attack ~8 ms, release ~60 ms
    const bool bowing = v.gate && !s.pizz;
    if (bowing) v.env += (1.0f - v.env) * 0.0025f;          // ~8 ms
    else        v.env -= v.env * 0.00035f;

    // vibrato: own oscillator, fades in ~0.3 s after the note starts
    v.vs += v.vibK * v.vc;
    v.vc -= v.vibK * v.vs;
    const float vibTarget = (s.vibrato && v.gate) ? v.vibDepth : 0.0f;
    v.vibAmt += (vibTarget - v.vibAmt) * 0.00007f;
    const float L = v.len * (1.0f + v.vibAmt * v.vs);

    // bow position splits the string: bridge side beta, nut side 1 - beta
    float beta = s.beta;
    float lb = L * beta;
    if (lb > (float)(STR_BRIDGE_LEN - 4)) lb = (float)(STR_BRIDGE_LEN - 4);
    if (lb < 1.5f) lb = 1.5f;
    const float ln = L - lb;

    const float bridgeOut = strRead(v.bridge, STR_BRIDGE_LEN - 1, v.bw, lb);
    const float neckOut   = strRead(v.neck,   STR_NECK_LEN - 1,   v.nw, ln);

    // reflections: lossy, inverting bridge; rigid, inverting nut
    v.lp += (bridgeOut - v.lp) * s.bridgeK;
    const float bridgeRefl = -v.lp * s.loss;
    const float nutRefl    = -neckOut;
    const float stringVel  = bridgeRefl + nutRefl;

    float newVel = 0.0f;
    if (bowing || v.env > 0.001f) {
      // bow speed: tremolo = fast up / down strokes
      float bv = bowMax * v.speedScale * v.env;
      if (s.tremolo) {
        v.tremPh += v.tremInc;
        if (v.tremPh >= 1.0f) v.tremPh -= 1.0f;
        float tri = 4.0f * (v.tremPh < 0.5f ? v.tremPh : 1.0f - v.tremPh) - 1.0f;
        tri *= 3.0f;
        tri = strClamp(tri, -1.0f, 1.0f);
        bv *= tri;
      }
      // friction: stick while the velocity difference is small, slip above
      const float dv = bv - stringVel;
      float t = dv * (s.slope + v.pressOffs * 4.0f) + 0.001f;
      t = (t < 0 ? -t : t) + 0.75f;
      t = t * t; t = t * t;                                  // ^4
      float f = 1.0f / t;
      if (f > 1.0f) f = 1.0f;
      newVel = dv * f;
    }
    if (v.pluck) {                                           // pizzicato pulse
      if (v.pluckN < v.pluckLen) {
        const float x = (float)v.pluckN / (float)v.pluckLen;
        newVel += 0.5f * (1.0f - x) * (x < 0.15f ? x / 0.15f : 1.0f);
        v.pluckN++;
      } else v.pluck = false;
    }

    v.neck[v.nw]   = bridgeRefl + newVel;
    v.bridge[v.bw] = nutRefl + newVel;
    v.nw = (v.nw + 1) & (STR_NECK_LEN - 1);
    v.bw = (v.bw + 1) & (STR_BRIDGE_LEN - 1);

    famBus[v.family] += bridgeOut * v.gain;                  // force on the bridge
  }

  // ---- bodies: one per family that is playing
  float out = 0.0f;
  for (int f = 0; f < STR_FAMILIES; f++) {
    StrMode *m = s.body[f];
    // skip bodies nobody plays on, once they have rung out
    if (!((s.famUsed >> f) & 1)) {
      const float e = m[0].y1 * m[0].y1 + m[1].y1 * m[1].y1 + m[2].y1 * m[2].y1;
      if (e < 1e-12f) continue;
    }
    const float x = famBus[f];
    float y = x * s.bodyDirect[f];
    for (int k = 0; k < STR_BODY_MODES; k++) {
      const float yy = m[k].a1 * m[k].y1 - m[k].a2 * m[k].y2 + m[k].g * x;
      m[k].y2 = m[k].y1;
      m[k].y1 = yy;
      y += yy;
    }
    out += y;
  }
  out *= s.level;

  // dc blocker + soft limit
  s.dcy = out - s.dcx + 0.9995f * s.dcy;
  s.dcx = out;
  float o = s.dcy * 1.2f;
  o = strClamp(o, -1.5f, 1.5f);
  return o - o * o * o * (1.0f / 6.75f);                    // smooth clip, max +-1
}

// clear all string and body states (instrument change)
static inline void stringsClear(StringsEngine &s) {
  for (int i = 0; i < STR_VOICES; i++) {
    StrVoice &v = s.v[i];
    for (int k = 0; k < STR_NECK_LEN; k++) v.neck[k] = 0;
    for (int k = 0; k < STR_BRIDGE_LEN; k++) v.bridge[k] = 0;
    v.lp = v.env = v.vibAmt = 0; v.gate = false; v.pluck = false;
  }
  for (int f = 0; f < STR_FAMILIES; f++)
    for (int m = 0; m < STR_BODY_MODES; m++) s.body[f][m].y1 = s.body[f][m].y2 = 0;
  s.dcx = s.dcy = 0;
  s.gatePrev = false;
}

// generative divisi: the Turing register picks who plays and which chord tone
static inline void stringsDivisi(StringsEngine &s, uint16_t reg) {
  uint8_t mask = 1;                                          // voice 0 always plays
  for (int i = 1; i < STR_VOICES; i++) {
    if ((reg >> (i + 5)) & 1) mask |= (uint8_t)(1 << i);
    s.divisiTone[i] = (int8_t)((reg >> (i * 2)) & 7);
  }
  if (((mask >> 1) & 0x1F) == 0) mask |= 0x04;               // at least two players
  s.divisiMask = mask;
}
