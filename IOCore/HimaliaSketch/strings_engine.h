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
// base ensembles
enum StrInstrument : uint8_t { INS_SOLO_VIOLIN = 0, INS_SOLO_CELLO, INS_VIOLIN_SECTION,
                               INS_CELLO_SECTION, INS_FULL_STRINGS, INS_LOW_STRINGS, INS_BASE_COUNT };
enum StrLayout     : uint8_t { LAY_UNISON = 0, LAY_OCTAVES, LAY_FIFTHS, LAY_MAJOR, LAY_MINOR,
                               LAY_DIVISI_MAJOR, LAY_DIVISI_MINOR, LAY_CLUSTER, LAY_COUNT };
// models on the RATCHET knob: a base ensemble plus playing style
enum StrModel : uint8_t { MDL_SOLO_VIOLIN = 0, MDL_SOLO_CELLO, MDL_VIOLIN_SECTION, MDL_CELLO_SECTION,
                          MDL_FULL_STRINGS, MDL_MOLTO_VIBRATO, MDL_ROMANTIC_CELLO, MDL_TREMOLO_SECTION,
                          MDL_TRILL_SECTION, MDL_EPIC_OSTINATO, MDL_SUSPENSE, MDL_HEROIC,
                          MDL_EMOTIONAL_SWELL, MDL_SHIMMER, MDL_COUNT };
#define INS_COUNT MDL_COUNT        // the RATCHET knob selects one of the models

// Articulations (SQUARES MAIN TUNE knob + CV)
enum StrArtic : uint8_t { ART_LEGATO = 0, ART_DETACHE, ART_MARTELE, ART_STACCATO, ART_SPICCATO,
                          ART_TASTO, ART_PONTICELLO, ART_COL_LEGNO, ART_FLAGEOLET, ART_PIZZICATO,
                          ART_PORTAMENTO, ART_COUNT };
enum StrExcite : uint8_t { MODE_BOW = 0, MODE_PLUCK, MODE_STRIKE };

struct StrArticDef {
  uint8_t mode;        // bow / pluck (pizzicato) / strike (col legno)
  float beta;          // bow position (bridge = 0)
  float press;         // pressure multiplier
  float speed;         // bow speed multiplier
  float attack;        // bow envelope attack per sample
  float accent;        // extra bow speed at the start of a stroke
  float accentDecay;   // per sample
  float bite;          // extra pressure during the accent
  float stroke;        // max stroke length in seconds (0 = as long as the gate)
  float glide;         // pitch glide per sample while the gate is held
  float ring;          // ring-out time (s, -60 dB) once the bow is off / after a pluck
  int8_t semis;        // pitch offset (flageolet sounds an octave higher)
  bool  alternate;     // bow direction changes every note
  bool  legato;        // no new attack while the gate stays high
};

static const StrArticDef STR_ARTIC[ART_COUNT] = {
  //  mode        beta  press speed attack   accent decay     bite stroke glide    ring(s) st alt  leg
  { MODE_BOW,    0.11f, 1.0f, 1.0f, 0.0015f, 0.0f, 0.9990f,  0.0f, 0.0f,  0.0007f, 0.60f, 0, false, true  },  // legato
  { MODE_BOW,    0.11f, 1.0f, 1.0f, 0.0030f, 0.35f,0.9993f,  0.3f, 0.0f,  0.0030f, 0.60f, 0, true,  false },  // detache
  { MODE_BOW,    0.10f, 1.1f, 1.0f, 0.0060f, 1.0f, 0.99965f, 1.0f, 0.0f,  0.0030f, 0.70f, 0, true,  false },  // martele / marcato
  { MODE_BOW,    0.11f, 1.0f, 1.0f, 0.0050f, 0.5f, 0.9993f,  0.5f, 0.12f, 0.0030f, 0.40f, 0, true,  false },  // staccato
  { MODE_BOW,    0.13f, 0.6f, 1.2f, 0.0080f, 0.6f, 0.9985f,  0.2f, 0.05f, 0.0030f, 0.30f, 0, true,  false },  // spiccato
  { MODE_BOW,    0.28f, 0.5f, 0.8f, 0.0008f, 0.0f, 0.9990f,  0.0f, 0.0f,  0.0007f, 0.60f, 0, false, true  },  // sul tasto / flautando
  { MODE_BOW,    0.035f,1.1f, 1.0f, 0.0015f, 0.0f, 0.9990f,  0.0f, 0.0f,  0.0007f, 0.60f, 0, false, true  },  // sul ponticello
  { MODE_STRIKE, 0.11f, 1.0f, 1.0f, 0.0f,    0.0f, 0.9990f,  0.0f, 0.0f,  0.0030f, 0.12f, 0, false, false },  // col legno battuto
  { MODE_BOW,    0.08f, 0.35f,0.9f, 0.0010f, 0.0f, 0.9990f,  0.0f, 0.0f,  0.0007f, 1.00f, 12, false, true },  // flageolet (harmonics)
  { MODE_PLUCK,  0.11f, 1.0f, 1.0f, 0.0f,    0.0f, 0.9990f,  0.0f, 0.0f,  0.0030f, 1.20f, 0, false, false },  // pizzicato
  { MODE_BOW,    0.11f, 1.0f, 1.0f, 0.0010f, 0.0f, 0.9990f,  0.0f, 0.0f,  0.00012f, 0.60f, 0, false, true  },  // portamento
};

// ------------------------------------------------------------------ models
struct StrModelDef {
  uint8_t ins;        // base ensemble
  int8_t  artic;      // forced articulation, -1 = SQUARES MAIN TUNE knob
  int8_t  layout;     // forced layout, -1 = SPREAD knob
  int8_t  trem;       // 1 = always tremolo, 0 = never, -1 = BIT switch
  float   vibDepth;   // vibrato depth multiplier
  float   vibRate;    // vibrato rate multiplier
  float   swell;      // seconds for a note to swell to full bow (0 = off)
  int8_t  trill;      // trill interval in semitones (0 = off)
  float   accent;     // accent multiplier
  float   tight;      // onset spread multiplier (ensemble timing)
  float   ring;       // ring-out multiplier (short = punchy)
  float   gain;       // level (short low notes need more)
};
static const StrModelDef STR_MODELS[MDL_COUNT] = {
  //  base                art            layout        trem vibD  vibR  swell trill accent tight ring gain
  { INS_SOLO_VIOLIN,    -1,            -1,           -1, 1.0f, 1.0f,  0.0f, 0,   1.0f,  1.0f, 1.0f, 1.0f },  // solo violin
  { INS_SOLO_CELLO,     -1,            -1,           -1, 1.0f, 1.0f,  0.0f, 0,   1.0f,  1.0f, 1.0f, 1.0f },  // solo cello
  { INS_VIOLIN_SECTION, -1,            -1,           -1, 1.0f, 1.0f,  0.0f, 0,   1.0f,  1.0f, 1.0f, 1.0f },  // violin section
  { INS_CELLO_SECTION,  -1,            -1,           -1, 1.0f, 1.0f,  0.0f, 0,   1.0f,  1.0f, 1.0f, 1.0f },  // cello section
  { INS_FULL_STRINGS,   -1,            -1,           -1, 1.0f, 1.0f,  0.0f, 0,   1.0f,  1.0f, 1.0f, 1.0f },  // full strings
  { INS_SOLO_VIOLIN,    -1,            -1,           -1, 1.9f, 1.15f, 0.0f, 0,   1.0f,  1.0f, 1.0f, 1.0f },  // molto vibrato violin
  { INS_SOLO_CELLO,     ART_LEGATO,    -1,           -1, 1.7f, 0.8f,  0.6f, 0,   1.0f,  1.0f, 1.0f, 1.0f },  // romantic cello
  { INS_VIOLIN_SECTION, ART_DETACHE,   -1,            1, 0.5f, 1.0f,  0.0f, 0,   1.0f,  0.5f, 1.0f, 1.0f },  // tremolo section
  { INS_VIOLIN_SECTION, ART_LEGATO,    -1,           -1, 0.3f, 1.0f,  0.0f, 2,   1.0f,  0.5f, 1.0f, 1.0f },  // trill section (whole tone)
  { INS_LOW_STRINGS,    ART_SPICCATO,  LAY_OCTAVES,   0, 0.0f, 1.0f,  0.0f, 0,   3.5f,  0.2f, 0.25f, 2.4f },  // epic ostinato
  { INS_VIOLIN_SECTION, ART_PONTICELLO,LAY_CLUSTER,   1, 0.2f, 1.0f,  0.0f, 0,   1.0f,  0.5f, 1.0f, 1.0f },  // suspense
  { INS_FULL_STRINGS,   ART_MARTELE,   LAY_OCTAVES,  -1, 1.6f, 1.1f,  0.0f, 0,   1.4f,  0.6f, 1.0f, 1.0f },  // heroic theme
  { INS_FULL_STRINGS,   ART_LEGATO,    -1,           -1, 1.4f, 0.95f, 1.6f, 0,   1.0f,  1.0f, 1.0f, 1.0f },  // emotional swell
  { INS_VIOLIN_SECTION, ART_TASTO,     LAY_OCTAVES,  -1, 0.7f, 0.75f, 2.2f, 0,   1.0f,  1.0f, 1.0f, 1.0f },  // shimmer
};

// noise transient per articulation: bow scratch at the start of a stroke,
// finger snap (pizzicato), wood click (col legno)
static const float STR_ARTIC_SCRATCH[ART_COUNT] = {
  0.35f,   // legato
  0.65f,   // detache
  1.00f,   // martele
  0.80f,   // staccato
  0.90f,   // spiccato
  0.25f,   // sul tasto
  0.75f,   // sul ponticello
  1.20f,   // col legno
  0.30f,   // flageolet
  0.80f,   // pizzicato
  0.35f,   // portamento
};

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
  float vibRateMul, vibDepthMul;  // this player's character
  float tremPh, tremInc;          // tremolo bow strokes
  float speedScale, pressOffs;    // this player's bow
  float detune;                   // static intonation (ratio)
  float drift, driftTarget;       // slow intonation drift (cents)
  float gain;
  uint32_t onset;                 // samples until this player starts / stops
  uint32_t pluckN, pluckLen;      // pizzicato / col legno pulse
  float pluckAmp;
  uint32_t strokeN;               // samples since the stroke started
  float dir;                      // bow direction +1 / -1
  float accent;                   // stroke accent (decays)
  float swell;                    // note swell (0 ... 1)
  float tran;                     // noise transient envelope
  float tranHp;                   // highpass state for the radiated transient
  float tremPrev;                 // previous tremolo stroke sign
  float vel;                      // this note's velocity (from the dynamic gate)
  float prevB;                    // previous bridge output (brightness tilt)
  uint32_t nrng;                  // own noise generator (bow / rosin noise)
  float ringLoss;                 // string loss per round trip once the bow is off
  float hz;                       // this player's note (for the square outputs)
  uint8_t family;
  bool active, gate, pluck;
};

struct StrMode { float a1, a2, g, y1, y2; };

struct StringsEngine {
  StrVoice v[STR_VOICES];
  StrMode body[STR_FAMILIES][STR_BODY_MODES];      // divisi players (2 ... 6)
  StrMode bodyLead[STR_FAMILIES][STR_BODY_MODES];  // player 1 (same resonances, own state)
  float famLpLead[STR_FAMILIES];
  float outDivisi;                // last sample of the divisi-only output
  float dc2x, dc2y;
  float bodyDirect[STR_FAMILIES];
  // control (written from loop)
  float rootHz;
  float speed;                    // bow speed 0 ... 1
  float pressure;                 // bow pressure 0 ... 1
  float beta;                     // bow position: 0.04 ponticello ... 0.3 tasto
  float speedCV;                  // expression CV multiplier (1 = none)
  bool  pizz, vibrato, tremolo;   // pizz: kept for compatibility (= ART_PIZZICATO)
  bool  mute;                     // con sordino
  uint8_t artic;
  uint8_t maxVoices;              // CPU guard: the firmware lowers this when the audio interrupt gets too slow
  float trillPh, trillInc;        // trill (alternating upper note)
  StrModelDef model;              // current model (copied by stringsControl)
  uint8_t articEff;               // articulation in use (model or knob)
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
  float famLp[STR_FAMILIES];       // output lowpass per family (top end roll-off)
  float famLpK[STR_FAMILIES];
  float bowNoise;                 // rosin noise amount
  float transient;                // noise transient amount (1 = normal)
  float velocity;                 // dynamic gate level (1 = normal), sampled per note
  // tempo from the gates: vibrato and tremolo lock to it
  uint32_t sinceGate;             // samples since the last gate start
  uint32_t clockPeriod;           // samples per gate (0 = no steady clock)
  uint32_t lastPeriod;
  float vibHz, tremHz;            // current vibrato rate, tremolo cycle rate
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
  //  A0 (air)          B1- (wood)          B1+ (wood)          mid               bridge hill        upper
  { { 275, 15, 0.9f }, { 470, 25, 1.3f }, { 550, 25, 1.2f }, { 1000, 6, 0.5f }, { 2500, 2.5f, 1.0f }, { 3300, 5, 0.4f } },  // violin
  { { 230, 14, 0.9f }, { 350, 22, 1.3f }, { 430, 22, 1.2f }, {  850, 6, 0.5f }, { 2000, 2.5f, 0.9f }, { 2700, 5, 0.35f } }, // viola
  { { 100, 12, 1.0f }, { 145, 20, 1.3f }, { 205, 20, 1.2f }, {  400, 6, 0.5f }, { 1300, 2.5f, 0.7f }, { 2000, 5, 0.3f } },  // cello
  { {  58, 10, 1.1f }, {  95, 16, 1.2f }, { 125, 16, 1.0f }, {  250, 6, 0.5f }, {  800, 2.5f, 0.5f }, { 1300, 5, 0.2f } },  // bass
};
static const float STR_BODY_DIRECT[STR_FAMILIES] = { 0.35f, 0.4f, 0.45f, 0.5f };
// top end per family: the bodies radiate little above these (Hz)
static const float STR_FAMILY_TOP[STR_FAMILIES] = { 5500.0f, 4500.0f, 3500.0f, 2000.0f };
// vibrato per family: rate (Hz) and depth (relative pitch, 0.010 ~ +-17 cents)
static const float STR_VIB_RATE[STR_FAMILIES]  = { 5.6f, 5.4f, 5.0f, 4.6f };
static const float STR_VIB_DEPTH[STR_FAMILIES] = { 0.010f, 0.011f, 0.012f, 0.010f };

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
  { 0, 1, 2, -1, 3, -2 },        // cluster (semitones)
};
static const int8_t STR_FULL_LAYOUT[LAY_COUNT][STR_VOICES] = {
  { 0, 0, 0, -12, -12, -24 },    // unison: everybody on the note, in their register
  { 12, 0, 0, -12, -12, -24 },   // octaves
  { 7, 0, -5, -12, -17, -24 },   // fifths
  { 16, 7, 4, -12, -5, -24 },    // major
  { 15, 7, 3, -12, -5, -24 },    // minor
  { 0, 0, 0, -12, -12, -24 },    // divisi: register adds chord tones on top
  { 0, 0, 0, -12, -12, -24 },
  { 1, 0, -1, -12, -11, -24 },   // cluster
};
static const uint8_t STR_FULL_FAMILY[STR_VOICES] = { FAM_VIOLIN, FAM_VIOLIN, FAM_VIOLA, FAM_CELLO, FAM_CELLO, FAM_BASS };
// low strings: violas, cellos and basses in octaves
static const int8_t STR_LOW_LAYOUT[LAY_COUNT][STR_VOICES] = {
  { 0, 0, -12, -12, -24, -24 },  // unison
  { 0, 12, -12, 0, -24, -12 },   // octaves
  { 7, 0, -5, -12, -17, -24 },   // fifths
  { 4, 7, -12, -5, -24, -12 },   // major
  { 3, 7, -12, -5, -24, -12 },   // minor
  { 0, 0, -12, -12, -24, -24 },  // divisi
  { 0, 0, -12, -12, -24, -24 },
  { 0, 1, -12, -11, -24, -23 },  // cluster
};
static const uint8_t STR_LOW_FAMILY[STR_VOICES] = { FAM_VIOLA, FAM_VIOLA, FAM_CELLO, FAM_CELLO, FAM_BASS, FAM_BASS };

static const int8_t STR_DIVISI_MAJOR[8] = { 0, 4, 7, 12, 16, 19, -5, 24 };
static const int8_t STR_DIVISI_MINOR[8] = { 0, 3, 7, 12, 15, 19, -5, 24 };

// ------------------------------------------------------------------ init
static inline void stringsInit(StringsEngine &s) {
  s.rng = 0x2545F491u;
  for (int f = 0; f < STR_FAMILIES; f++) {
    for (int m = 0; m < STR_BODY_MODES; m++) {
      strMode(s.body[f][m], STR_BODY[f][m][0], STR_BODY[f][m][1], STR_BODY[f][m][2]);
      s.body[f][m].y1 = s.body[f][m].y2 = 0;
      s.bodyLead[f][m] = s.body[f][m];
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
    {                                                         // vibrato starts at a random phase
      const float ph = 3.14159265f * strRand(s);
      v.vs = sinf(ph); v.vc = cosf(ph);
    }
    // every player gets their own vibrato rate, depth, bow and intonation
    v.vibRateMul = 0.9f + 0.2f * (0.5f + 0.5f * strRand(s));           // own rate, +-10%
    v.vibK = 6.2831853f * 5.5f * v.vibRateMul / STR_SR;
    v.vibAmt = 0;
    v.vibDepthMul = 0.75f + 0.5f * (0.5f + 0.5f * strRand(s));          // own depth
    v.vibDepth = 0.010f * v.vibDepthMul;
    v.tremPh = 0.5f * (0.5f + 0.5f * strRand(s));
    v.tremInc = (12.0f + 2.0f * strRand(s)) / STR_SR;
    v.speedScale = 1.0f + 0.10f * strRand(s);
    v.pressOffs = 0.06f * strRand(s);
    v.detune = exp2f((6.0f * strRand(s)) / 1200.0f);                    // +-6 cents
    v.drift = v.driftTarget = 0;
    v.gain = 1.0f;
    v.onset = 0; v.pluckN = v.pluckLen = 0; v.pluckAmp = 0.5f;
    v.strokeN = 0; v.dir = 1.0f; v.accent = 0.0f; v.swell = 1.0f;
    v.tran = 0.0f; v.tranHp = 0.0f; v.tremPrev = 1.0f; v.vel = 1.0f; v.prevB = 0.0f;
    v.nrng = 0x9E3779B9u * (uint32_t)(i + 1);
    v.ringLoss = 0.99f;
    v.hz = 220.0f;
    v.family = FAM_VIOLIN;
    v.active = (i == 0); v.gate = false; v.pluck = false;
  }
  s.rootHz = 220.0f; s.speed = 0.5f; s.pressure = 0.5f; s.beta = 0.12f; s.speedCV = 1.0f;
  s.pizz = false; s.vibrato = true; s.tremolo = false; s.mute = false; s.artic = ART_LEGATO;
  for (int f = 0; f < STR_FAMILIES; f++) { s.famLp[f] = 0; s.famLpLead[f] = 0; s.famLpK[f] = 0.5f; }
  s.outDivisi = 0; s.dc2x = s.dc2y = 0;
  s.bowNoise = 0.0f;
  s.transient = 1.0f;
  s.velocity = 1.0f;
  s.sinceGate = 0; s.clockPeriod = 0; s.lastPeriod = 0; s.vibHz = 0; s.tremHz = 6.5f;
  s.instrument = MDL_SOLO_VIOLIN; s.layout = LAY_UNISON; s.maxVoices = STR_VOICES;
  s.trillPh = 0; s.trillInc = 9.0f / STR_SR;
  s.model = STR_MODELS[0]; s.articEff = ART_LEGATO;
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
  const StrModelDef &M = STR_MODELS[s.instrument < MDL_COUNT ? s.instrument : 0];
  const uint8_t ins = M.ins;
  const uint8_t lay = M.layout >= 0 ? (uint8_t)M.layout : (s.layout < LAY_COUNT ? s.layout : 0);
  const bool solo = (ins == INS_SOLO_VIOLIN || ins == INS_SOLO_CELLO);
  const bool full = (ins == INS_FULL_STRINGS);
  const bool low  = (ins == INS_LOW_STRINGS);
  const bool divisi = (lay == LAY_DIVISI_MAJOR || lay == LAY_DIVISI_MINOR);
  const int8_t *pool = (lay == LAY_DIVISI_MAJOR) ? STR_DIVISI_MAJOR : STR_DIVISI_MINOR;

  const uint8_t art = s.pizz ? (uint8_t)ART_PIZZICATO
                  : (M.artic >= 0 ? (uint8_t)M.artic : (s.artic < ART_COUNT ? s.artic : 0));
  s.articEff = art;
  const StrArticDef &A = STR_ARTIC[art];
  s.beta = A.beta;
  // bow: pressure sets the friction curve (higher pressure = longer sticking,
  // more grit)
  const float press = strClamp(s.pressure * A.press, 0.0f, 1.0f);
  s.slope   = 5.0f - 4.0f * press;
  s.loss    = 0.95f;
  // string damping follows the bow position: near the bridge (ponticello) the
  // tone is glassy and bright, over the fingerboard (tasto) soft and dark
  const float b = strClamp(s.beta, 0.03f, 0.30f);
  s.bridgeK = (A.mode == MODE_PLUCK ? 0.55f : (A.mode == MODE_STRIKE ? 0.75f : 0.90f)) - 1.9f * (b - 0.04f);
  if (s.bridgeK > 0.95f) s.bridgeK = 0.95f;
  // rosin noise: more with pressure and close to the bridge
  s.bowNoise = 0.015f + 0.025f * press + 0.25f * (0.12f - strClamp(b, 0.03f, 0.12f));
  // top end per family, con sordino (mute) takes off more
  for (int f = 0; f < STR_FAMILIES; f++) {
    const float fc = STR_FAMILY_TOP[f] * (s.mute ? 0.45f : 1.0f);
    s.famLpK[f] = 1.0f - expf(-6.2831853f * fc / STR_SR);
  }

  const float vibRateM = M.vibRate;
  // ---- trill: ~9 alternations per second, on the tempo when there is a clock
  {
    float th = 9.0f;
    if (s.clockPeriod) {
      const float bh = STR_SR / (float)s.clockPeriod;
      static const float tm[6] = { 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f };
      float best = 9.0f, d = 1e9f;
      for (int k = 0; k < 6; k++) { const float e = fabsf(logf(bh * tm[k] / 9.0f)); if (e < d) { d = e; best = bh * tm[k]; } }
      if (d < 0.6f) th = best;
    }
    s.trillInc = 0.5f * th / STR_SR;                          // one cycle = two notes
  }
  // ---- tempo sync: pick the beat subdivision nearest to the natural rate
  //      vibrato ~5.5 Hz, tremolo ~13 bow strokes per second
  const bool synced = s.clockPeriod > 0;
  float beatHz = synced ? STR_SR / (float)s.clockPeriod : 0.0f;
  if (synced) {
    static const float mult[12] = { 0.25f, 1.0f / 3, 0.5f, 2.0f / 3, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f };
    float bestV = 0, bestT = 0, dV = 1e9f, dT = 1e9f;
    for (int k = 0; k < 12; k++) {
      const float r = beatHz * mult[k];
      const float ev = fabsf(logf(r / (5.5f * vibRateM))), et = fabsf(logf(r / 13.0f));
      if (ev < dV) { dV = ev; bestV = r; }
      if (et < dT) { dT = et; bestT = r; }
    }
    s.vibHz  = (dV < 0.45f) ? bestV : 0.0f;                 // within ~-35% ... +55%
    s.tremHz = (dT < 0.6f) ? bestT * 0.5f : 6.5f;           // one cycle = two strokes
  } else {
    s.vibHz = 0.0f;
    s.tremHz = 6.5f;
  }

  int nActive = 0;
  for (int i = 0; i < STR_VOICES; i++) {
    StrVoice &v = s.v[i];
    bool on = solo ? (i == 0) : true;
    if (divisi && !solo) on = (i == 0) || ((s.divisiMask >> i) & 1);
    if (i >= s.maxVoices) on = false;
    v.active = on;
    if (!on) continue;
    nActive++;

    uint8_t fam;
    int semis;
    if (solo)      { fam = (ins == INS_SOLO_CELLO) ? FAM_CELLO : FAM_VIOLIN; semis = 0; }
    else if (full) { fam = STR_FULL_FAMILY[i]; semis = STR_FULL_LAYOUT[lay][i]; }
    else if (low)  { fam = STR_LOW_FAMILY[i];  semis = STR_LOW_LAYOUT[lay][i]; }
    else           { fam = (ins == INS_CELLO_SECTION) ? FAM_CELLO : FAM_VIOLIN; semis = STR_SECTION_LAYOUT[lay][i]; }
    if (divisi && !solo && i > 0) semis += pool[s.divisiTone[i] & 7];
    semis += A.semis;
    v.family = fam;
    // vibrato rate: on the tempo when there is a steady clock, otherwise free
    // (every player a little different)
    v.vibK = 6.2831853f * (s.vibHz > 0 ? s.vibHz : STR_VIB_RATE[fam] * v.vibRateMul * M.vibRate) / STR_SR;
    // tremolo: exactly on the tempo when synced, otherwise every player a bit different
    v.tremInc = s.tremHz * (synced ? 1.0f : v.vibRateMul) / STR_SR;
    v.vibDepth = STR_VIB_DEPTH[fam] * v.vibDepthMul * M.vibDepth;

    // slow intonation drift: every player wanders a few cents
    if ((s.rng & 0xFF) < 2) v.driftTarget = 4.0f * strRand(s);
    strRand(s);
    v.drift += (v.driftTarget - v.drift) * 0.002f;

    float hz = s.rootHz * exp2f((float)semis / 12.0f);
    while (hz < STR_FAMILY_LOW[fam]) hz *= 2.0f;            // keep it on the instrument
    if (hz > 3500.0f) hz = 3500.0f;
    if (!solo) hz *= v.detune;
    hz *= exp2f(v.drift / 1200.0f);
    v.hz = hz;
    // loop length minus the delay of the bridge filter and the junction
    float len = STR_SR / hz - strLpDelay(s.bridgeK, hz) - s.lenComp;
    // ring-out after the bow / pluck: the same time for every note
    v.ringLoss = powf(0.001f, 1.0f / (A.ring * M.ring * (s.mute ? 0.6f : 1.0f) * hz));
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
  if (s.mute) s.level *= 0.75f;
  s.level *= M.gain;
  // which instrument bodies are in use
  uint8_t used = 0;
  for (int i = 1; i < STR_VOICES; i++) if (s.v[i].active) used |= (uint8_t)(1 << s.v[i].family);   // divisi bodies
  s.famUsed = used;
  s.model = M;
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
  const StrModelDef &M = s.model;
  const bool solo = (M.ins == INS_SOLO_VIOLIN || M.ins == INS_SOLO_CELLO);
  const bool trem = M.trem > 0 ? true : (M.trem == 0 ? false : s.tremolo);
  const float swellK = M.swell > 0 ? 1.0f / (M.swell * STR_SR) : 1.0f;
  // trill: the upper note shortens the string
  bool trillUp = false;
  if (M.trill) {
    s.trillPh += s.trillInc;
    if (s.trillPh >= 1.0f) s.trillPh -= 1.0f;
    trillUp = s.trillPh >= 0.5f;
  }
  const float trillRatio = M.trill == 2 ? 0.8908987f : (M.trill == 1 ? 0.9438743f : 1.0f);
  // ---- tempo: time between gate starts (10 ms ... 4 s), must repeat
  if (s.sinceGate < 0xFFFFFFF0u) s.sinceGate++;
  if (gate && !s.gatePrev) {
    const uint32_t per = s.sinceGate;
    s.sinceGate = 0;
    if (per > (uint32_t)(0.01f * STR_SR) && per < (uint32_t)(4.0f * STR_SR)) {
      const float r = (float)per / (float)(s.lastPeriod ? s.lastPeriod : 1);
      if (r > 0.85f && r < 1.18f)                            // steady: follow it smoothly
        s.clockPeriod = s.clockPeriod ? (s.clockPeriod * 3 + per) / 4 : per;
      s.lastPeriod = per;
    }
    // tremolo strokes start on the beat (tiny per-player spread)
    for (int i = 0; i < STR_VOICES; i++)
      s.v[i].tremPh = 0.25f + 0.01f * (float)i;
  }
  if (s.sinceGate > (uint32_t)(4.0f * STR_SR)) s.clockPeriod = 0;   // clock stopped

  if (gate != s.gatePrev) {
    for (int i = 0; i < STR_VOICES; i++) {
      StrVoice &v = s.v[i];
      const float r = 0.5f + 0.5f * strRand(s);
      v.onset = solo ? 0 : (uint32_t)(r * (gate ? 0.035f : 0.015f) * M.tight * STR_SR);
    }
    s.gatePrev = gate;
  }

  const float bowMax = (0.03f + 0.20f * s.speed) * s.speedCV;
  const StrArticDef &A = STR_ARTIC[s.articEff < ART_COUNT ? s.articEff : 0];
  const uint32_t strokeMax = (uint32_t)(A.stroke * STR_SR);
  float famBus[STR_FAMILIES]  = { 0, 0, 0, 0 };   // divisi players
  float leadBus = 0.0f;                              // player 1
  uint8_t leadFam = s.v[0].family;

  for (int i = 0; i < STR_VOICES; i++) {
    StrVoice &v = s.v[i];
    if (!v.active) continue;

    // this player's gate (after the onset delay)
    if (v.onset) v.onset--;
    else if (v.gate != gate) {
      v.gate = gate;
      if (!gate && v.env > 0.1f && A.mode == MODE_BOW) v.tran += 0.12f;   // bow lifts off
      if (gate) {
        const bool continuing = A.legato && v.env > 0.05f;    // legato: same bow
        if (continuing) v.tran += 0.15f;                      // finger / bow change noise
        if (!continuing) {
          v.strokeN = 0;
          v.vel = strClamp(s.velocity, 0.0f, 1.3f);             // dynamic gate: how hard this note
          v.accent = A.accent * M.accent * (0.4f + 0.6f * v.vel);
          v.swell = M.swell > 0 ? 0.15f : 1.0f;               // swell: start soft
          // noise transient: scratch / snap / click, a bit different every time
          v.tran = STR_ARTIC_SCRATCH[s.articEff < ART_COUNT ? s.articEff : 0]
                 * (0.7f + 0.3f * (0.5f + 0.5f * strRand(s))) * (0.6f + 0.6f * s.pressure)
                 * (0.3f + 0.7f * v.vel);
          if (A.alternate) v.dir = -v.dir;                    // up-bow / down-bow
          if (A.glide > 0.0005f || v.env < 0.05f) v.len = v.lenTarget;   // new note: no slide
          v.vibAmt = 0;                                       // vibrato starts later
          if (A.mode == MODE_PLUCK) {                         // pizzicato
            v.pluck = true; v.pluckN = 0;
            // harder pluck = shorter, brighter finger contact
            v.pluckLen = (uint32_t)(v.len * 0.25f * (1.35f - 0.55f * strClamp(s.velocity, 0.0f, 1.3f))) + 2;
            v.pluckAmp = 0.5f;
          } else if (A.mode == MODE_STRIKE) {                 // col legno: wood tap
            v.pluck = true; v.pluckN = 0;
            v.pluckLen = 12; v.pluckAmp = 1.2f;
          }
        }
      }
    }
    v.strokeN++;

    // pitch glide: legato ~30 ms, portamento ~0.4 s
    v.len += (v.lenTarget - v.len) * A.glide;

    // bow envelope: attack per articulation, release ~60 ms; staccato and
    // spiccato lift the bow after a fixed stroke even if the gate stays high
    bool bowing = v.gate && A.mode == MODE_BOW;
    if (bowing && strokeMax && v.strokeN > strokeMax) bowing = false;
    if (bowing) v.env += (1.0f - v.env) * A.attack;
    else        v.env -= v.env * (strokeMax ? 0.004f : 0.00035f);   // short strokes lift fast
    v.accent *= A.accentDecay;
    v.swell += (1.0f - v.swell) * swellK;

    // vibrato: own oscillator, fades in ~0.3 s after the note starts and
    // grows with the bow speed
    v.vs += v.vibK * v.vc;
    v.vc -= v.vibK * v.vs;
    const float vibTarget = (s.vibrato && v.gate && A.mode == MODE_BOW)
                          ? v.vibDepth * (0.6f + 0.4f * s.speed) * (0.75f + 0.25f * v.vel) : 0.0f;
    v.vibAmt += (vibTarget - v.vibAmt) * 0.00007f;
    const float L = v.len * (1.0f + v.vibAmt * v.vs) * (trillUp ? trillRatio : 1.0f);

    // bow position splits the string: bridge side beta, nut side 1 - beta
    float beta = s.beta;
    float lb = L * beta;
    if (lb > (float)(STR_BRIDGE_LEN - 4)) lb = (float)(STR_BRIDGE_LEN - 4);
    if (lb < 1.5f) lb = 1.5f;
    const float ln = L - lb;

    const float bridgeOut = strRead(v.bridge, STR_BRIDGE_LEN - 1, v.bw, lb);
    const float neckOut   = strRead(v.neck,   STR_NECK_LEN - 1,   v.nw, ln);

    // reflections: lossy, inverting bridge; rigid, inverting nut. Once the
    // bow is off the string it rings on a little (less loss)
    v.lp += (bridgeOut - v.lp) * s.bridgeK;
    const float loss = (v.env > 0.02f) ? s.loss : v.ringLoss;
    const float bridgeRefl = -v.lp * loss;
    const float nutRefl    = -neckOut;
    const float stringVel  = bridgeRefl + nutRefl;

    float newVel = 0.0f;
    if (bowing || v.env > 0.001f) {
      // bow speed: direction, articulation accent; tremolo = fast strokes
      float bv = bowMax * A.speed * v.speedScale * v.env * v.swell * (1.0f + v.accent) * v.dir;
      if (trem) {
        v.tremPh += v.tremInc;
        if (v.tremPh >= 1.0f) v.tremPh -= 1.0f;
        float tri = 4.0f * (v.tremPh < 0.5f ? v.tremPh : 1.0f - v.tremPh) - 1.0f;
        tri *= 3.0f;
        tri = strClamp(tri, -1.0f, 1.0f);
        const float sg = tri >= 0 ? 1.0f : -1.0f;
        if (sg != v.tremPrev) { v.tran += 0.35f; v.tremPrev = sg; }   // bow change scratch
        bv *= tri;
      }
      // rosin noise on the bow
      v.nrng = v.nrng * 1664525u + 1013904223u;
      const float nz = (float)(int32_t)v.nrng * (1.0f / 2147483648.0f);
      // friction: stick while the velocity difference is small, slip above.
      // The accent bites in with extra pressure (martele).
      const float dv = bv - stringVel + nz * s.bowNoise * (bv < 0 ? -bv : bv);
      float sl = s.slope + v.pressOffs * 4.0f - 2.5f * v.accent * A.bite - 1.2f * (v.vel - 1.0f);
      if (sl < 0.6f) sl = 0.6f;
      float t = dv * sl + 0.001f;
      t = (t < 0 ? -t : t) + 0.75f;
      t = t * t; t = t * t;                                  // ^4
      float f = 1.0f / t;
      if (f > 1.0f) f = 1.0f;
      newVel = dv * f;
    }
    if (v.pluck) {                                           // pizzicato / col legno pulse
      if (v.pluckN < v.pluckLen) {
        const float x = (float)v.pluckN / (float)v.pluckLen;
        newVel += v.pluckAmp * (0.3f + 0.7f * v.vel) * (1.0f - x) * (x < 0.15f ? x / 0.15f : 1.0f);
        v.pluckN++;
      } else v.pluck = false;
    }

    // noise transient: ~12 ms, part goes into the string, part is heard directly
    float tr = 0.0f;
    if (v.tran > 0.002f) {
      v.nrng = v.nrng * 1664525u + 1013904223u;
      const float nz = (float)(int32_t)v.nrng * (1.0f / 2147483648.0f);
      tr = nz * v.tran * s.transient;
      v.tran *= (A.mode == MODE_STRIKE) ? 0.990f : 0.9982f;  // wood click shorter
      newVel += tr * 0.04f;
    }
    v.neck[v.nw]   = bridgeRefl + newVel;
    v.bridge[v.bw] = nutRefl + newVel;
    v.nw = (v.nw + 1) & (STR_NECK_LEN - 1);
    v.bw = (v.bw + 1) & (STR_BRIDGE_LEN - 1);

    // radiated transient: highpassed so it is a scratch, not a thump
    const float trHp = tr - v.tranHp;
    v.tranHp += (tr - v.tranHp) * 0.15f;
    // louder notes sound brighter: tilt the radiated sound with the velocity
    const float tilt = 0.8f * (v.vel - 0.7f);
    const float radiated = bridgeOut + (bridgeOut - v.prevB) * tilt + trHp * 0.10f;
    v.prevB = bridgeOut;
    if (i == 0) leadBus = radiated * v.gain;                 // force on the bridge
    else        famBus[v.family] += radiated * v.gain;
  }

  // ---- bodies: player 1 through its own body, the divisi players through
  //      one body per family that is playing. Bodies are linear, so
  //      full mix = lead + divisi.
  float lead;
  {
    StrMode *m = s.bodyLead[leadFam];
    float y = leadBus * s.bodyDirect[leadFam];
    for (int k = 0; k < STR_BODY_MODES; k++) {
      const float yy = m[k].a1 * m[k].y1 - m[k].a2 * m[k].y2 + m[k].g * leadBus;
      m[k].y2 = m[k].y1;
      m[k].y1 = yy;
      y += yy;
    }
    s.famLpLead[leadFam] += (y - s.famLpLead[leadFam]) * s.famLpK[leadFam];
    lead = s.famLpLead[leadFam];
  }
  float div = 0.0f;
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
    s.famLp[f] += (y - s.famLp[f]) * s.famLpK[f];            // top end roll-off
    div += s.famLp[f];
  }
  lead *= s.level;
  div  *= s.level;

  // divisi-only output (second DAC): dc blocker + soft limit
  s.dc2y = div - s.dc2x + 0.9995f * s.dc2y;
  s.dc2x = div;
  float o2 = strClamp(s.dc2y * 1.4f, -1.5f, 1.5f);
  s.outDivisi = o2 - o2 * o2 * o2 * (1.0f / 6.75f);

  // full mix (SAMPLE OUT)
  const float out = lead + div;
  s.dcy = out - s.dcx + 0.9995f * s.dcy;
  s.dcx = out;
  float o = s.dcy * 1.4f;
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
  for (int f = 0; f < STR_FAMILIES; f++) {
    for (int m = 0; m < STR_BODY_MODES; m++) {
      s.body[f][m].y1 = s.body[f][m].y2 = 0;
      s.bodyLead[f][m].y1 = s.bodyLead[f][m].y2 = 0;
    }
    s.famLp[f] = s.famLpLead[f] = 0;
  }
  s.dcx = s.dcy = 0; s.dc2x = s.dc2y = 0; s.outDivisi = 0;
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
