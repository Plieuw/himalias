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
#include "strings_body_ir.h"
#include <stdint.h>

#ifndef STR_SR
#define STR_SR 47619.0f
#endif

#define STR_BEAT_TARGET (60.0f / 128.0f)   // preferred beat: the tempo lands nearest 128 BPM
#define STR_ONSETS 33       // gate starts kept for the rhythm analysis (32 intervals)
#define STR_VOICES     6
#define STR_NECK_LEN   2048          // nut side segment (lowest note ~ 23 Hz)
#define STR_BRIDGE_LEN 512           // bridge side segment (bow position <= 0.3)
#define STR_FAMILIES   4
#define STR_BODY_MODES 6
#define STR_LOW_MODES 3        // A0, B1-, B1+ stay IIR resonators (they ring longer than the IR)
// body IR convolution: uniform partitioned (overlap-save), 1024 taps = 21.5 ms
#define STR_IR_LEN   1024
#define STR_IR_BLOCK 128
#define STR_IR_FFT   256
#define STR_IR_PARTS (STR_IR_LEN / STR_IR_BLOCK)
#define STR_IR_BINS  (STR_IR_FFT / 2 + 1)
#define STR_IR_GAIN  0.95f   // average level as the old modal body
#define STR_AIR_DEFAULT 0.5f    // air band level (0 = none)

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
  // realistic vibrato: delayed start, slow-to-settle rate, cycle-to-cycle
  // wobble of rate and width, finger changes
  uint32_t vibDelay;              // samples before the vibrato starts
  float vibWarm;                  // rate / width settle after the start (0.75 ... 1)
  float jitR, jitRT, jitD, jitDT; // rate / width wobble (current, target)
  float vibPrevS;                 // previous vibrato sine (cycle start detection)
  float fingerLen;                // stopped string length the finger is on
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
  // bowing like a player: bow speed from the note length (tempo), bow
  // position and pressure follow it, long notes need bow changes
  float bowMul;                   // bow speed for this note (1 = normal)
  float bowGain;                  // level for this bow speed (slow softer, fast louder)
  float bowUsed;                  // part of the bow used (1 = at the tip / frog)
  float bowLen;                   // this player's bow (seconds at normal speed)
  uint32_t turn;                  // samples left in a bow change
  float slip;                     // last friction state (0 = stick, 1 = slip)
  float tBp1, tBp2;               // band-pass state for the radiated scratch
  uint32_t nrng;                  // own noise generator (bow / rosin noise)
  float ringLoss;                 // string loss per round trip once the bow is off
  float hz;                       // this player's note (for the square outputs)
  uint8_t family;
  bool active, gate, pluck;
};

struct StrMode { float a1, a2, g, y1, y2; };

// The body runs block-wise (128 samples): the audio interrupt only writes the
// player buses and reads the finished output, stringsBody() does the body
// (low IIR modes + IR convolution) and the output stage. On the module it
// runs in a low-priority interrupt; latency 2 blocks (5.4 ms).
struct StrConv {
  float inBuf[2][STR_IR_BLOCK][1 + STR_FAMILIES];   // lead bus, family buses
  float outBuf[2][STR_IR_BLOCK][2];                 // full mix, divisi
  float prev[STR_IR_BLOCK][2];                      // previous IR input block (lead, divisi)
  float fdl[STR_IR_PARTS][2][STR_IR_BINS * 2];      // input spectra history
  float H[STR_FAMILIES][STR_IR_PARTS][STR_IR_BINS * 2];   // IR spectra per family
  float Hdiv[STR_IR_PARTS][STR_IR_BINS * 2];        // divisi output: blend of the families playing
  float buf[STR_IR_FFT * 2];
  float lowOut[STR_IR_BLOCK][2];
  float cosT[STR_IR_FFT / 2], sinT[STR_IR_FFT / 2];
  uint8_t rev[STR_IR_FFT];
  uint16_t pos;                   // sample in the current block
  uint8_t w;                      // block the interrupt writes / reads
  volatile uint8_t ready;         // a full block waits for stringsBody()
  uint8_t head;                   // newest spectrum in fdl
  volatile uint16_t overruns;     // body too late (glitch)
  bool deferred;                  // true: the firmware calls stringsBody() itself
  uint32_t mixKey;                // which families the Hdiv blend is made of
  // "air": the top octave a mix engineer would lift with a high shelf. The
  // body filters it away, so it is taken from the strings themselves (bow
  // hair, rosin, top harmonics): 2nd order highpass, mixed in after the body
  float ab0, ab1, ab2, aa1, aa2;
  float az[2][2][2];              // highpass states (lead / divisi, stage 1 / 2)
};

struct StringsEngine {
  StrVoice v[STR_VOICES];
  StrMode body[STR_FAMILIES][STR_BODY_MODES];      // divisi players (2 ... 6)
  StrMode bodyLead[STR_FAMILIES][STR_BODY_MODES];  // player 1 (same resonances, own state)
  float famLpLead[STR_FAMILIES];
  float outDivisi;                // last sample of the divisi-only output
  float dc2x, dc2y;
  float bodyDirect[STR_FAMILIES];
  StrConv conv;
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
  bool  trillPrev;                // trill state last sample (finger taps)
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
  float air;                      // "air" band amount (top octave lift)
  float velocity;                 // dynamic gate level (1 = normal), sampled per note
  float noteDur;                  // expected note length (s): time between the last gates
  // tempo from the gates: vibrato and tremolo lock to it
  uint32_t sinceGate;             // samples since the last gate start
  uint32_t clockPeriod;           // samples per beat (0 = no clock)
  // rhythm analysis: the last gate starts (time, velocity) -> pulse, pattern, beat
  uint32_t now;                   // sample counter
  uint32_t onT[STR_ONSETS];       // gate start times (ring)
  float    onV[STR_ONSETS];       // gate start velocities (accents)
  uint8_t  onW;                   // next write position
  uint8_t  onN;                   // valid onsets (0 ... STR_ONSETS)
  uint16_t onCount;               // counts every onset (the analysis runs on news)
  uint16_t onSeen;
  uint32_t pulse;                 // smallest grid in the rhythm (samples)
  uint8_t  patLen;                // pattern length in notes (0 = none found)
  uint8_t  patPulses;             // pattern length in pulses (one bar of the ostinato)
  uint32_t beatCand;              // a new beat waits for a second opinion
  float vibHz, tremHz;            // current vibrato rate, tremolo cycle rate
  float dcx, dcy, level;
  uint8_t famUsed;                // bodies with players
  uint32_t rng;
};

static inline void strConvInit(StringsEngine &s);
static inline void strConvMix(StringsEngine &s);

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
    v.vibDelay = 0; v.vibWarm = 1.0f; v.jitR = v.jitRT = v.jitD = v.jitDT = 1.0f;
    v.vibPrevS = 0.0f; v.fingerLen = v.len;
    v.bowMul = 1.0f; v.bowGain = 1.0f; v.bowUsed = 0.0f; v.bowLen = 3.0f * (0.85f + 0.3f * (0.5f + 0.5f * strRand(s)));
    v.turn = 0; v.slip = 0.0f; v.tBp1 = v.tBp2 = 0.0f;
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
  s.air = STR_AIR_DEFAULT;
  s.velocity = 1.0f;
  s.noteDur = 1.2f;
  s.sinceGate = (uint32_t)(5.0f * STR_SR); s.clockPeriod = 0;   // the first gate counts too
  s.now = 0; s.onW = 0; s.onN = 0; s.onCount = 0; s.onSeen = 0;
  s.pulse = 0; s.patLen = 0; s.patPulses = 0; s.beatCand = 0; s.vibHz = 0; s.tremHz = 6.5f;
  s.instrument = MDL_SOLO_VIOLIN; s.layout = LAY_UNISON; s.maxVoices = STR_VOICES;
  s.trillPh = 0; s.trillInc = 9.0f / STR_SR; s.trillPrev = false;
  s.model = STR_MODELS[0]; s.articEff = ART_LEGATO;
  s.divisiMask = 0x3F;
  strConvInit(s);
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
// ------------------------------------------------------------------ rhythm analysis (loop)
// Works on irregular rhythms (ostinati like 3+3+2, long-short-short, gallops):
//  1. pulse: the largest grid that all intervals are whole multiples of
//  2. pattern: the shortest repeating group of intervals (and accents)
//  3. beat: the accent spacing when the accents are regular, otherwise the
//     division of the pattern nearest 128 bpm (or a pulse multiple)
static inline void stringsTempo(StringsEngine &s) {
  if (s.onCount == s.onSeen) return;
  s.onSeen = s.onCount;
  int n = s.onN;
  if (n < 3) return;                                       // 2 intervals needed
  // tempo change: when the newest two intervals are off the current grid,
  // forget the older notes and start over from the new ones
  if (s.pulse && n > 3) {
    int off = 0;
    for (int b = 1; b <= 2; b++) {
      const int i1 = (s.onW + STR_ONSETS - b) % STR_ONSETS, i0 = (i1 + STR_ONSETS - 1) % STR_ONSETS;
      const float m = (float)(s.onT[i1] - s.onT[i0]) / (float)s.pulse;
      const float r = floorf(m + 0.5f);
      if (r < 1.0f || r > 12.0f || fabsf(m - r) > 0.1f) off++;
    }
    if (off == 2) { n = 3; s.onN = 3; }
  }
  int ni = n - 1;
  uint32_t ioi[STR_ONSETS - 1];
  float vel[STR_ONSETS];
  const int start = (s.onW + STR_ONSETS - n) % STR_ONSETS;
  for (int i = 0; i < n; i++) vel[i] = s.onV[(start + i) % STR_ONSETS];
  for (int i = 0; i < ni; i++)
    ioi[i] = s.onT[(start + i + 1) % STR_ONSETS] - s.onT[(start + i) % STR_ONSETS];

  // 1. pulse: candidates are every interval / 1 ... 4; keep the largest that
  //    fits (nearly) all intervals as 1 ... 12 pulses
  const float minP = 0.055f * STR_SR, maxP = 2.0f * STR_SR;
  float best = 0.0f;
  for (int i = 0; i < ni; i++) {
    for (int k = 1; k <= 4; k++) {
      const float P = (float)ioi[i] / (float)k;
      if (P < minP || P > maxP || P <= best * 1.03f) continue;
      int fit = 0;
      for (int j = 0; j < ni; j++) {
        const float m = (float)ioi[j] / P;
        const float r = floorf(m + 0.5f);
        if (r >= 1.0f && r <= 12.0f && fabsf(m - r) < 0.09f * (r < 3.0f ? 1.0f : 3.0f / r) + 0.03f) fit++;
      }
      if (fit * 10 >= ni * 9) best = P;                    // 90 % of the intervals fit
    }
  }
  if (best <= 0.0f) return;                                // no grid (free playing): keep the old tempo
  // refine: least squares over the fitting intervals
  uint8_t u[STR_ONSETS - 1];
  {
    float sumI = 0, sumM = 0;
    for (int j = 0; j < ni; j++) {
      float r = floorf((float)ioi[j] / best + 0.5f);
      if (r < 1.0f) r = 1.0f;
      if (r > 12.0f) r = 12.0f;
      u[j] = (uint8_t)r;
      sumI += (float)ioi[j]; sumM += r;
    }
    best = sumI / sumM;
  }
  s.pulse = (uint32_t)best;

  // accents: only when the dynamic gate really varies
  float vmax = 0, vmin = 10, vsum = 0;
  for (int i = 0; i < n; i++) { vmax = fmaxf(vmax, vel[i]); vmin = fminf(vmin, vel[i]); vsum += vel[i]; }
  const bool accents = (vmax - vmin) > 0.15f;
  const float vThr = vmin + 0.6f * (vmax - vmin);
  bool acc[STR_ONSETS];
  for (int i = 0; i < n; i++) acc[i] = accents && vel[i] >= vThr;

  // 2. pattern: shortest L (notes) where intervals (and accents) repeat
  int L = 0;
  for (int l = 1; l <= 16 && l * 2 <= ni; l++) {
    bool ok = true;
    for (int j = l; j < ni && ok; j++) if (u[j] != u[j - l] || acc[j] != acc[j - l]) ok = false;
    if (ok) { L = l; break; }
  }
  int bar = 0;                                             // pattern length in pulses
  if (L) for (int j = ni - L; j < ni; j++) bar += u[j];
  s.patLen = (uint8_t)L; s.patPulses = (uint8_t)(bar > 255 ? 255 : bar);

  // 3. beat in pulses
  int beat = 0;
  if (accents) {                                           // regular accents = the beat
    int last = -1, sp = 0, pos = 0; bool regular = true; int cnt = 0;
    for (int i = 0; i < n; i++) {
      if (acc[i]) {
        if (last >= 0) { const int d = pos - last; if (sp && d != sp) regular = false; sp = d; cnt++; }
        last = pos;
      }
      if (i < ni) pos += u[i];
    }
    if (regular && cnt >= 2 && sp > 0) {
      // the accents mark the grid; the beat is the division of it nearest 128 bpm
      float d = 1e9f;
      for (int b = 1; b <= sp; b++) {
        if (sp % b) continue;
        const float e = fabsf(logf(b * best / (STR_BEAT_TARGET * STR_SR)));
        if (e < d) { d = e; beat = b; }
      }
    }
  }
  if (!beat) {
    // divisions of the pattern (or 1, 2, 4, 8 pulses) nearest 128 BPM
    float d = 1e9f;
    const int span = bar ? bar : 8;
    for (int b = 1; b <= span; b++) {
      if (bar ? (bar % b) != 0 : (b & (b - 1)) != 0) continue;
      const float e = fabsf(logf(b * best / (STR_BEAT_TARGET * STR_SR)));
      if (e < d) { d = e; beat = b; }
    }
  }
  const uint32_t newBeat = (uint32_t)(beat * best);
  // follow small changes smoothly, take a jump only when it is confirmed
  const uint32_t old = s.clockPeriod;
  const float ratio = old ? (float)newBeat / (float)old : 0.0f;
  if (!old || (ratio > 0.9f && ratio < 1.1f)) {
    s.clockPeriod = old ? (old * 3 + newBeat) / 4 : newBeat;
    s.beatCand = 0;
  } else if (s.beatCand && fabsf((float)newBeat / (float)s.beatCand - 1.0f) < 0.1f) {
    s.clockPeriod = newBeat; s.beatCand = 0;
  } else {
    s.beatCand = newBeat;
  }
}

static inline void stringsControl(StringsEngine &s) {
  stringsTempo(s);
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
  strConvMix(s);                                           // divisi body IR blend
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

// ------------------------------------------------------------------ body IR convolution
// complex radix-2 FFT, N = 256, in place on interleaved re / im
static inline void strFFT(StrConv &c, float *x, bool inverse) {
  const int N = STR_IR_FFT;
  for (int i = 0; i < N; i++) {
    const int j = c.rev[i];
    if (j > i) {
      float t = x[2 * i]; x[2 * i] = x[2 * j]; x[2 * j] = t;
      t = x[2 * i + 1]; x[2 * i + 1] = x[2 * j + 1]; x[2 * j + 1] = t;
    }
  }
  const float sg = inverse ? 1.0f : -1.0f;
  for (int len = 2; len <= N; len <<= 1) {
    const int half = len >> 1, step = N / len;
    for (int i = 0; i < N; i += len) {
      for (int k = 0; k < half; k++) {
        const float wr = c.cosT[k * step], wi = sg * c.sinT[k * step];
        float *a = x + 2 * (i + k), *b = x + 2 * (i + k + half);
        const float tr = b[0] * wr - b[1] * wi, ti = b[0] * wi + b[1] * wr;
        b[0] = a[0] - tr; b[1] = a[1] - ti;
        a[0] += tr;       a[1] += ti;
      }
    }
  }
  if (inverse) { const float sc = 1.0f / N; for (int i = 0; i < 2 * N; i++) x[i] *= sc; }
}

static inline void strConvInit(StringsEngine &s) {
  StrConv &c = s.conv;
  const int N = STR_IR_FFT;
  for (int k = 0; k < N / 2; k++) { c.cosT[k] = cosf(6.2831853f * k / N); c.sinT[k] = sinf(6.2831853f * k / N); }
  for (int i = 0; i < N; i++) { int r = 0; for (int b = 0; b < 8; b++) if (i & (1 << b)) r |= 1 << (7 - b); c.rev[i] = (uint8_t)r; }
  // IR partitions -> spectra (each partition zero padded to the FFT size)
  for (int f = 0; f < STR_FAMILIES; f++)
    for (int p = 0; p < STR_IR_PARTS; p++) {
      for (int i = 0; i < 2 * N; i++) c.buf[i] = 0.0f;
      for (int i = 0; i < STR_IR_BLOCK; i++) c.buf[2 * i] = STR_BODY_IR[f][p * STR_IR_BLOCK + i] * STR_IR_GAIN;
      strFFT(c, c.buf, false);
      for (int k = 0; k < STR_IR_BINS * 2; k++) c.H[f][p][k] = c.buf[k];
    }
  for (int p = 0; p < STR_IR_PARTS; p++) for (int k = 0; k < STR_IR_BINS * 2; k++) c.Hdiv[p][k] = c.H[0][p][k];
  c.mixKey = 0xFFFFFFFFu;
  {                                                         // air band: 4th order highpass 9.5 kHz
    const float w = 6.2831853f * 9500.0f / STR_SR, al = sinf(w) / (2.0f * 0.707f), cw = cosf(w);
    const float a0 = 1.0f + al;
    c.ab0 = (1.0f + cw) * 0.5f / a0; c.ab1 = -(1.0f + cw) / a0; c.ab2 = c.ab0;
    c.aa1 = -2.0f * cw / a0; c.aa2 = (1.0f - al) / a0;
    for (int ch = 0; ch < 2; ch++) for (int st = 0; st < 2; st++) c.az[ch][st][0] = c.az[ch][st][1] = 0.0f;
  }
  for (int b = 0; b < 2; b++) for (int i = 0; i < STR_IR_BLOCK; i++) {
    for (int k = 0; k < 1 + STR_FAMILIES; k++) c.inBuf[b][i][k] = 0.0f;
    c.outBuf[b][i][0] = c.outBuf[b][i][1] = 0.0f;
  }
  for (int i = 0; i < STR_IR_BLOCK; i++) c.prev[i][0] = c.prev[i][1] = 0.0f;
  for (int p = 0; p < STR_IR_PARTS; p++) for (int k = 0; k < STR_IR_BINS * 2; k++) c.fdl[p][0][k] = c.fdl[p][1][k] = 0.0f;
  c.pos = 0; c.w = 0; c.ready = 0; c.head = 0; c.overruns = 0; c.deferred = false;
}

// the divisi output gets one IR: the blend of the families that play divisi
static inline void strConvMix(StringsEngine &s) {
  StrConv &c = s.conv;
  uint8_t cnt[STR_FAMILIES] = { 0, 0, 0, 0 };
  for (int i = 1; i < STR_VOICES; i++) if (s.v[i].active) cnt[s.v[i].family]++;
  const uint32_t key = cnt[0] | (cnt[1] << 8) | (cnt[2] << 16) | ((uint32_t)cnt[3] << 24);
  if (key == c.mixKey) return;
  c.mixKey = key;
  int tot = cnt[0] + cnt[1] + cnt[2] + cnt[3];
  if (!tot) { cnt[s.v[0].family] = 1; tot = 1; }
  // the families' dense modes are unrelated, so a plain average would lose
  // level: scale the blend to the average energy of the families
  float eMix = 0.0f, eFam = 0.0f;
  for (int p = 0; p < STR_IR_PARTS; p++)
    for (int k = 0; k < STR_IR_BINS * 2; k++) {
      float h = 0.0f;
      for (int f = 0; f < STR_FAMILIES; f++) if (cnt[f]) {
        h += c.H[f][p][k] * (float)cnt[f];
        eFam += c.H[f][p][k] * c.H[f][p][k] * (float)cnt[f] / (float)tot;
      }
      h /= (float)tot;
      c.Hdiv[p][k] = h;
      eMix += h * h;
    }
  const float g = eMix > 0.0f ? sqrtf(eFam / eMix) : 1.0f;
  for (int p = 0; p < STR_IR_PARTS; p++) for (int k = 0; k < STR_IR_BINS * 2; k++) c.Hdiv[p][k] *= g;
}

static inline float strLowModes(StrMode *m, float x) {
  float y = 0.0f;
  for (int k = 0; k < STR_LOW_MODES; k++) {
    const float yy = m[k].a1 * m[k].y1 - m[k].a2 * m[k].y2 + m[k].g * x;
    m[k].y2 = m[k].y1; m[k].y1 = yy; y += yy;
  }
  return y;
}

// one block: low body modes, IR convolution (lead and divisi packed as one
// complex signal), output stage. Called when s.conv.ready is set.
static inline void stringsBody(StringsEngine &s) {
  StrConv &c = s.conv;
  if (!c.ready) return;
  const int b = c.w ^ 1;                                    // the block that is full
  const uint8_t lf = s.v[0].family;
  const int N = STR_IR_FFT, B = STR_IR_BLOCK;
  const float air = s.air * (s.mute ? 0.25f : 1.0f);
  // 1. low modes (IIR) and the top-end filter in front of the IR
  for (int n = 0; n < B; n++) {
    const float *x = c.inBuf[b][n];
    float yl = strLowModes(s.bodyLead[lf], x[0]);
    s.famLpLead[lf] += (x[0] - s.famLpLead[lf]) * s.famLpK[lf];
    float yd = 0.0f, dIn = 0.0f;
    for (int f = 0; f < STR_FAMILIES; f++) {
      StrMode *m = s.body[f];
      if (!((s.famUsed >> f) & 1)) {
        const float e = m[0].y1 * m[0].y1 + m[1].y1 * m[1].y1 + m[2].y1 * m[2].y1 + s.famLp[f] * s.famLp[f];
        if (e < 1e-12f) continue;
      }
      yd += strLowModes(m, x[1 + f]);
      s.famLp[f] += (x[1 + f] - s.famLp[f]) * s.famLpK[f];
      dIn += s.famLp[f];
    }
    // air: top octave of the raw strings (transposed direct form II)
    float sumD = 0.0f;
    for (int f = 0; f < STR_FAMILIES; f++) sumD += x[1 + f];
    const float ain[2] = { x[0], sumD };
    for (int ch = 0; ch < 2; ch++) {
      float y = ain[ch];
      for (int st = 0; st < 2; st++) {
        float *z = c.az[ch][st];
        const float xin = y;
        y = c.ab0 * xin + z[0];
        z[0] = c.ab1 * xin - c.aa1 * y + z[1];
        z[1] = c.ab2 * xin - c.aa2 * y;
      }
      if (ch == 0) yl += y * air; else yd += y * air;
    }
    c.lowOut[n][0] = yl; c.lowOut[n][1] = yd;
    // FFT frame: [previous block, this block], lead = real, divisi = imaginary
    c.buf[2 * n] = c.prev[n][0];          c.buf[2 * n + 1] = c.prev[n][1];
    c.buf[2 * (B + n)] = s.famLpLead[lf]; c.buf[2 * (B + n) + 1] = dIn;
    c.prev[n][0] = s.famLpLead[lf];       c.prev[n][1] = dIn;
  }
  // 2. spectrum of the frame, split into the lead and divisi spectra
  strFFT(c, c.buf, false);
  c.head = (uint8_t)((c.head + 1) % STR_IR_PARTS);
  float *XL = c.fdl[c.head][0], *XD = c.fdl[c.head][1];
  for (int k = 0; k < STR_IR_BINS; k++) {
    const int kn = (N - k) & (N - 1);
    const float a = c.buf[2 * k], bb = c.buf[2 * k + 1];
    const float cr = c.buf[2 * kn], ci = -c.buf[2 * kn + 1];   // conj(Z[N-k])
    XL[2 * k] = 0.5f * (a + cr);  XL[2 * k + 1] = 0.5f * (bb + ci);
    XD[2 * k] = 0.5f * (bb - ci); XD[2 * k + 1] = -0.5f * (a - cr);
  }
  // 3. multiply with the IR partitions and sum, pack lead + j divisi again
  const float *HL = &c.H[lf][0][0];
  for (int k = 0; k < STR_IR_BINS; k++) {
    float lr = 0, li = 0, dr = 0, di = 0;
    int h = c.head;
    for (int p = 0; p < STR_IR_PARTS; p++) {
      const float *xl = &c.fdl[h][0][2 * k], *xd = &c.fdl[h][1][2 * k];
      const float *hl = HL + p * STR_IR_BINS * 2 + 2 * k, *hd = &c.Hdiv[p][2 * k];
      lr += xl[0] * hl[0] - xl[1] * hl[1]; li += xl[0] * hl[1] + xl[1] * hl[0];
      dr += xd[0] * hd[0] - xd[1] * hd[1]; di += xd[0] * hd[1] + xd[1] * hd[0];
      h = h ? h - 1 : STR_IR_PARTS - 1;
    }
    c.buf[2 * k] = lr - di; c.buf[2 * k + 1] = li + dr;                 // L + jD
    if (k > 0 && k < N / 2) {
      const int kn = N - k;
      c.buf[2 * kn] = lr + di; c.buf[2 * kn + 1] = dr - li;             // conj(L) + j conj(D)
    }
  }
  strFFT(c, c.buf, true);
  // 4. output stage: level, dc blockers, soft limits
  const float lev = s.level;
  for (int n = 0; n < B; n++) {
    const float lead = (c.lowOut[n][0] + c.buf[2 * (B + n)]) * lev;
    const float div  = (c.lowOut[n][1] + c.buf[2 * (B + n) + 1]) * lev;
    s.dc2y = div - s.dc2x + 0.9995f * s.dc2y;
    s.dc2x = div;
    float o2 = strClamp(s.dc2y * 1.4f, -1.5f, 1.5f);
    c.outBuf[b][n][1] = o2 - o2 * o2 * o2 * (1.0f / 6.75f);
    const float out = lead + div;
    s.dcy = out - s.dcx + 0.9995f * s.dcy;
    s.dcx = out;
    float o = strClamp(s.dcy * 1.4f, -1.5f, 1.5f);
    c.outBuf[b][n][0] = o - o * o * o * (1.0f / 6.75f);   // smooth clip, max +-1
  }
  c.ready = 0;
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
  if (M.trill && trillUp != s.trillPrev) {                // trill: every finger hit / lift taps
    for (int i = 0; i < STR_VOICES; i++) if (s.v[i].active && s.v[i].env > 0.2f) s.v[i].tran += trillUp ? 0.07f : 0.03f;
    s.trillPrev = trillUp;
  }
  const float trillRatio = M.trill == 2 ? 0.8908987f : (M.trill == 1 ? 0.9438743f : 1.0f);
  // ---- tempo: log every gate start (time + velocity); stringsTempo() finds
  //      the pulse, the ostinato pattern and the beat from it
  s.now++;
  if (s.sinceGate < 0xFFFFFFF0u) s.sinceGate++;
  if (gate && !s.gatePrev && s.sinceGate > (uint32_t)(0.03f * STR_SR)) {   // ignore bounces < 30 ms
    if (s.sinceGate > (uint32_t)(3.0f * STR_SR)) s.onN = 0;               // long pause: new rhythm
    // note length for the bowing: the last interval; free playing = slow bow
    s.noteDur = s.sinceGate < (uint32_t)(3.0f * STR_SR) ? (float)s.sinceGate / STR_SR : 1.2f;
    s.sinceGate = 0;
    s.onT[s.onW] = s.now; s.onV[s.onW] = s.velocity;
    s.onW = (uint8_t)((s.onW + 1) % STR_ONSETS);
    if (s.onN < STR_ONSETS) s.onN++;
    s.onCount++;
    // tremolo strokes start on the beat (tiny per-player spread)
    for (int i = 0; i < STR_VOICES; i++)
      s.v[i].tremPh = 0.25f + 0.01f * (float)i;
  }
  // the dynamic gate may still be rising at the edge: keep the peak of the first 10 ms
  if (s.sinceGate < (uint32_t)(0.01f * STR_SR) && s.onN) {
    const uint8_t last = (uint8_t)((s.onW + STR_ONSETS - 1) % STR_ONSETS);
    if (s.velocity > s.onV[last]) s.onV[last] = s.velocity;
  }
  if (s.sinceGate > (uint32_t)(4.0f * STR_SR)) { s.clockPeriod = 0; s.pulse = 0; s.patLen = 0; }   // clock stopped

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
          // bow speed from the note length: short notes (fast tempo) fast bow,
          // long notes slow bow (0.6 s = normal)
          v.bowMul = strClamp(sqrtf(0.6f / strClamp(s.noteDur, 0.05f, 4.0f)), 0.6f, 1.6f);
          v.bowUsed = 0.0f; v.turn = 0;
          // the string alone gives a slow bow more level than a player would:
          // slow ~-2 dB, fast ~+2 dB against a normal bow (measured)
          v.bowGain = v.bowMul < 1.0f ? 1.0f - 0.96f * (1.0f - v.bowMul) : 1.0f;
          v.accent = A.accent * M.accent * (0.4f + 0.6f * v.vel);
          v.swell = M.swell > 0 ? 0.15f : 1.0f;               // swell: start soft
          // noise transient: scratch / snap / click, a bit different every time
          v.tran = STR_ARTIC_SCRATCH[s.articEff < ART_COUNT ? s.articEff : 0]
                 * (0.7f + 0.3f * (0.5f + 0.5f * strRand(s))) * (0.6f + 0.6f * s.pressure)
                 * (0.3f + 0.7f * v.vel);
          if (A.alternate) v.dir = -v.dir;                    // up-bow / down-bow
          if (A.glide > 0.0005f || v.env < 0.05f) v.len = v.lenTarget;   // new note: no slide
          v.vibAmt = 0;                                       // vibrato starts later:
          v.vibDelay = (uint32_t)((0.12f + 0.06f * (1.0f + strRand(s))) * STR_SR);   // ~0.12-0.24 s straight tone,
          v.vibWarm = 0.75f;                                  // then slower and narrower at first
          v.fingerLen = v.lenTarget;
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
    // finger change while the bow keeps going (legato pitch change): the
    // finger lands with a small tap and the vibrato stops and starts again
    if (fabsf(v.lenTarget - v.fingerLen) > 0.025f * v.fingerLen) {
      if (v.gate && A.mode == MODE_BOW && v.env > 0.2f) {
        v.tran += 0.10f;                                     // finger tap noise
        if (!v.pluck) { v.pluck = true; v.pluckN = 0; v.pluckLen = 10; v.pluckAmp = 0.06f; }
        v.vibAmt *= 0.3f;
        v.vibDelay = (uint32_t)(0.05f * STR_SR);
        v.vibWarm = 0.85f;
      }
      v.fingerLen = v.lenTarget;
    }
    if (v.vibDelay) v.vibDelay--;
    else v.vibWarm += (1.0f - v.vibWarm) * 0.00004f;         // settles in ~0.5 s
    // every cycle the hand wobbles a little: new rate (+-4 %) and width (+-15 %)
    if (v.vs >= 0.0f && v.vibPrevS < 0.0f) {
      v.jitRT = 1.0f + 0.04f * strRand(s);
      v.jitDT = 1.0f + 0.15f * strRand(s);
    }
    v.vibPrevS = v.vs;
    v.jitR += (v.jitRT - v.jitR) * 0.0004f;
    v.jitD += (v.jitDT - v.jitD) * 0.0004f;
    const float vk = v.vibK * v.jitR * v.vibWarm;
    v.vs += vk * v.vc;
    v.vc -= vk * v.vs;
    const float vibTarget = (s.vibrato && v.gate && A.mode == MODE_BOW && !v.vibDelay)
                          ? v.vibDepth * (0.6f + 0.4f * s.speed) * (0.75f + 0.25f * v.vel)
                            * v.jitD * (0.4f + 0.6f * v.vibWarm) : 0.0f;
    v.vibAmt += (vibTarget - v.vibAmt) * 0.00007f;
    // the finger rolls back from the note: the pitch swings mostly below it,
    // and the roll is a little asymmetric (a touch of the 2nd harmonic)
    const float vibShape = v.vs + 0.12f * (2.0f * v.vs * v.vc) + 0.3f;
    // how fast the finger moves right now (0 ... ~2): the stick-slip gets
    // rougher while the string length changes fastest
    const float vibRel = v.vibAmt * 100.0f;
    const float roll = vibRel * (v.vc < 0 ? -v.vc : v.vc);
    const float L = v.len * (1.0f + v.vibAmt * vibShape) * (trillUp ? trillRatio : 1.0f);

    // bow position splits the string: bridge side beta, nut side 1 - beta
    // slow bow plays closer to the bridge, fast bow further away
    float beta = s.beta * (0.8f + 0.2f * v.bowMul);
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
      float bv = bowMax * A.speed * v.speedScale * v.env * v.swell * (1.0f + v.accent) * v.dir * v.bowMul;
      // long notes: when the bow runs out the player changes bow: slows down,
      // turns (scratch) and speeds up again, ~90 ms. Every player has their
      // own bow length, so a section changes bow at different moments
      if (bowing && !trem && !strokeMax) {
        v.bowUsed += v.bowMul * (0.4f + 0.6f * s.speed) * v.env / (v.bowLen * STR_SR);
        if (v.bowUsed >= 1.0f && !v.turn) { v.turn = (uint32_t)(0.09f * STR_SR); v.bowUsed = 0.0f; }
      }
      if (v.turn) {
        const uint32_t T = (uint32_t)(0.09f * STR_SR), half = T / 2;
        const float x = (float)v.turn / (float)T;                       // 1 -> 0
        bv *= 0.3f + 0.7f * fabsf(cosf(3.14159265f * x));               // slow down, turn, speed up
        if (v.turn == half) { v.dir = -v.dir; v.tran += 0.22f * (0.5f + 0.5f * v.vel); }
        v.turn--;
      }
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
      bv *= 1.0f + 0.04f * vibRel * v.vc;                  // the bow arm moves with the hand
      // the noise comes with the slip: when the string slides under the
      // hair (once per period), not as constant hiss
      const float dv = bv - stringVel + nz * s.bowNoise * (0.35f + 1.6f * v.slip) * (1.0f + 2.5f * roll) * (bv < 0 ? -bv : bv);
      // the same bow force on a slower bow presses relatively harder: keep it
      // in the clean (Helmholtz) range
      float sl = s.slope + v.pressOffs * 4.0f - 2.5f * v.accent * A.bite - 1.2f * (v.vel - 1.0f)
               - 0.6f * (1.0f - v.bowMul);
      if (sl < 0.6f) sl = 0.6f;
      float t = dv * sl + 0.001f;
      t = (t < 0 ? -t : t) + 0.75f;
      t = t * t; t = t * t;                                  // ^4
      float f = 1.0f / t;
      if (f > 1.0f) f = 1.0f;
      newVel = dv * f;
      v.slip += ((1.0f - f) - v.slip) * 0.5f;
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
      newVel += tr * 0.06f;
    }
    v.neck[v.nw]   = bridgeRefl + newVel;
    v.bridge[v.bw] = nutRefl + newVel;
    v.nw = (v.nw + 1) & (STR_NECK_LEN - 1);
    v.bw = (v.bw + 1) & (STR_BRIDGE_LEN - 1);

    // radiated transient: highpassed so it is a scratch, not a thump
    // radiated scratch: band-passed (~1.5 ... 6 kHz) like hair on rosin, not hiss
    v.tranHp += (tr - v.tranHp) * 0.55f;                    // lowpass ~6 kHz
    v.tBp1 += (v.tranHp - v.tBp1) * 0.18f;                  // minus lowpass ~1.5 kHz
    const float trHp = v.tranHp - v.tBp1;
    // louder notes sound brighter: tilt the radiated sound with the velocity
    const float tilt = 0.8f * (v.vel - 0.7f) + 0.3f * (v.bowMul - 1.0f);   // fast bow a bit brighter
    const float radiated = bridgeOut + (bridgeOut - v.prevB) * tilt + trHp * 0.06f;
    v.prevB = bridgeOut;
    if (i == 0) leadBus = radiated * v.gain * v.bowGain;     // force on the bridge
    else        famBus[v.family] += radiated * v.gain * v.bowGain;
  }

  // ---- body + output stage run block-wise in stringsBody(): hand over the
  //      buses, take the finished sample (2 blocks later)
  (void)leadFam;
  StrConv &c = s.conv;
  float *in = c.inBuf[c.w][c.pos];
  in[0] = leadBus;
  for (int f = 0; f < STR_FAMILIES; f++) in[1 + f] = famBus[f];
  const float out = c.outBuf[c.w][c.pos][0];
  s.outDivisi = c.outBuf[c.w][c.pos][1];
  if (++c.pos == STR_IR_BLOCK) {
    c.pos = 0;
    c.w ^= 1;
    if (c.ready) c.overruns++;
    c.ready = 1;
    if (!c.deferred) stringsBody(s);
  }
  return out;
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
  StrConv &c = s.conv;                                       // body convolution history
  for (int p = 0; p < STR_IR_PARTS; p++) for (int k = 0; k < STR_IR_BINS * 2; k++) c.fdl[p][0][k] = c.fdl[p][1][k] = 0.0f;
  for (int i = 0; i < STR_IR_BLOCK; i++) c.prev[i][0] = c.prev[i][1] = 0.0f;
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
