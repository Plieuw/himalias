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

#define STR_BEAT_TARGET (60.0f / 128.0f)   // preferred beat: the tempo lands nearest 128 BPM
// v1.36 velocity: a wide dynamic range (pp ~ -19 dB against 1.0) and a
// strong accent for hard notes; 1.0 (no CV) is the reference
#define STR_VEL_LEV(x) (0.11f + 0.89f * strClampV(x))
#define STR_VEL_ACC(x) (0.10f + 0.90f * strClampV(x) * strClampV(x) + 0.0001f)
#define STR_SLIDE_MAX 12.0f  // v1.35: longest slide on one string (semitones)
#define STR_VEL_WIN 0.025f  // v1.34.1: velocity = peak of the first 25 ms of a note
#define STR_ONSETS 33       // gate starts kept for the rhythm analysis (32 intervals)
#define STR_VOICES     6
#define STR_NECK_LEN   2048          // nut side segment (lowest note ~ 23 Hz)
#define STR_BRIDGE_LEN 512           // bridge side segment (bow position <= 0.3)
#define STR_FAMILIES   4
#define STR_BODY_MODES 6
#ifndef STR_OVR_GRIP
#define STR_OVR_GRIP 1.0f      // overpressure: bow speed boost on the driven period
#define STR_OVR_FREE 0.85f     // ... and cut on the others (octave down)
#endif
#ifndef STR_OVR_FREE3
#define STR_OVR_FREE3 0.95f    // ... (octave + fifth down)
#endif
#ifndef STR_SL_MIN
#define STR_SL_MIN 0.6f
#endif
#define STR_AIR_DEFAULT 0.28f   // listener perspective: restrained upper-air layer
#ifndef STR_OUT_GAIN
#define STR_OUT_GAIN 0.62f      // normal playing peaks ~1.5 ... 3.7 V, firm ~2.5 ... 4.5 V
#endif
#ifndef STR_DYN_COMP
#define STR_DYN_COMP 0.40f      // dynamic curve: above normal the level grows with exponent 0.6
#endif
#ifndef STR_LIM_CEIL
#define STR_LIM_CEIL 0.996f     // safety net only: max +-4.98 V (DAC code 4029 of 4095), never clips
#endif
#define STR_LIM_LOOK 64         // look-ahead (samples, 1.3 ms); never clips, only moves the gain
#define STR_SYM_STRINGS 5       // sympathetic resonance: up to 5 open strings (5-string bass)
#define STR_SYM_LEN 1600        // delay per open string (bass B0 ~ 1540 samples)
#ifndef STR_SYM_LEVEL
#define STR_SYM_LEVEL 0.30f
#endif

enum StrFamily     : uint8_t { FAM_VIOLIN = 0, FAM_VIOLA, FAM_CELLO, FAM_BASS };
// base ensembles
enum StrInstrument : uint8_t { INS_SOLO_VIOLIN = 0, INS_SOLO_CELLO, INS_VIOLIN_SECTION,
                               INS_CELLO_SECTION, INS_FULL_STRINGS, INS_LOW_STRINGS,
                               INS_SOLO_VIOLA, INS_VIOLA_SECTION, INS_BASE_COUNT };
enum StrLayout     : uint8_t { LAY_UNISON = 0, LAY_OCTAVES, LAY_FIFTHS, LAY_MAJOR, LAY_MINOR,
                               LAY_DIVISI_MAJOR, LAY_DIVISI_MINOR, LAY_CLUSTER, LAY_COUNT };
// models on the RATCHET knob (v1.35): 7 instruments x 3 playing characters.
// Turn a little = another character, turn further = another instrument.
// The character is how the player interprets the phrase (adaptive playing):
//   classical = clean and controlled, romantic = singing, more vibrato and
//   slides, film = broad, swelling long notes, weight on the heavy beats
enum StrModel : uint8_t {
  MDL_SOLO_VIOLIN = 0, MDL_SOLO_VIOLIN_ROM, MDL_SOLO_VIOLIN_FILM,
  MDL_SOLO_CELLO,      MDL_SOLO_CELLO_ROM,  MDL_SOLO_CELLO_FILM,
  MDL_VIOLIN_SECTION,  MDL_VIOLIN_SECTION_ROM, MDL_VIOLIN_SECTION_FILM,
  MDL_VIOLA_SECTION,   MDL_VIOLA_SECTION_ROM,  MDL_VIOLA_SECTION_FILM,
  MDL_CELLO_SECTION,   MDL_CELLO_SECTION_ROM,  MDL_CELLO_SECTION_FILM,
  MDL_LOW_STRINGS,     MDL_LOW_STRINGS_ROM,    MDL_LOW_STRINGS_FILM,
  MDL_FULL_STRINGS,    MDL_FULL_STRINGS_ROM,   MDL_FULL_STRINGS_FILM,
  MDL_COUNT
};
enum StrCharacter : uint8_t { CHR_CLASSICAL = 0, CHR_ROMANTIC, CHR_FILM, CHR_COUNT };
struct StrCharDef {
  float expr;     // phrase shaping depth (energy, attack, vibrato intention)
  float arc;      // depth of the long arc over 8 phrases
  float glide;    // shifts / portamento time (> 1 = slower, more audible)
  float attack;   // attack placement (> 1 = gentler)
  float beat;     // extra weight on heavy beats
};
static const StrCharDef STR_CHAR[CHR_COUNT] = {
  { 0.80f, 0.6f, 0.8f, 0.95f, 1.0f },   // classical
  { 1.40f, 1.3f, 1.7f, 1.15f, 0.8f },   // romantic
  { 1.20f, 1.8f, 1.1f, 1.05f, 2.0f },   // film
};
#define INS_COUNT MDL_COUNT        // the RATCHET knob selects one of the models

// Articulations (SQUARES MAIN TUNE knob + CV), in playing order (v1.34):
// AUTO first (the gate length decides), then connected -> short -> plucked,
// then the tone colours from soft to harsh
enum StrArtic : uint8_t { ART_PORTAMENTO = 0, ART_LEGATO, ART_DETACHE, ART_MARTELE, ART_STACCATO,
                          ART_SPICCATO, ART_PIZZICATO, ART_FLAGEOLET, ART_TASTO, ART_PONTICELLO,
                          ART_COL_LEGNO, ART_COUNT };
#define ART_AUTO    ART_COUNT       // knob position 1: articulation from the gate length
#define ART_KNOB_STEPS (ART_COUNT + 1)
enum StrExcite : uint8_t { MODE_BOW = 0, MODE_PLUCK, MODE_STRIKE };

struct StrArticDef {
  uint8_t mode;        // bow / pluck (pizzicato) / strike (col legno)
  float beta;          // bow position (bridge = 0)
  float press;         // pressure multiplier
  float speed;         // bow speed multiplier
  float attack;        // articulation attack character (shapes natural attack time)
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
  { MODE_BOW,    0.11f, 1.0f, 1.0f, 0.0010f, 0.0f, 0.9990f,  0.0f, 0.0f,  0.00012f, 0.60f, 0, false, true  },  // portamento
  { MODE_BOW,    0.11f, 1.0f, 1.0f, 0.0015f, 0.0f, 0.9990f,  0.0f, 0.0f,  0.00045f,0.60f, 0, false, true  },  // legato: ~47 ms finger travel
  { MODE_BOW,    0.11f, 1.0f, 1.0f, 0.0030f, 0.35f,0.9993f,  0.3f, 0.0f,  0.0030f, 0.60f, 0, true,  false },  // detache
  { MODE_BOW,    0.10f, 1.1f, 1.0f, 0.0060f, 1.0f, 0.99965f, 1.0f, 0.0f,  0.0030f, 0.70f, 0, true,  false },  // martele / marcato
  { MODE_BOW,    0.11f, 1.0f, 1.0f, 0.0050f, 0.5f, 0.9993f,  0.5f, 0.12f, 0.0030f, 0.40f, 0, true,  false },  // staccato
  { MODE_BOW,    0.13f, 0.6f, 1.2f, 0.0080f, 0.6f, 0.9985f,  0.2f, 0.05f, 0.0030f, 0.30f, 0, true,  false },  // spiccato
  { MODE_PLUCK,  0.11f, 1.0f, 1.0f, 0.0f,    0.0f, 0.9990f,  0.0f, 0.0f,  0.0030f, 1.80f, 0, false, false },  // pizzicato: longer natural ring-out
  { MODE_BOW,    0.08f, 0.35f,0.9f, 0.0010f, 0.0f, 0.9990f,  0.0f, 0.0f,  0.0007f, 1.00f, 12, false, true },  // flageolet (harmonics)
  { MODE_BOW,    0.28f, 0.5f, 0.8f, 0.0008f, 0.0f, 0.9990f,  0.0f, 0.0f,  0.00045f,0.60f, 0, false, true  },  // sul tasto / flautando
  { MODE_BOW,    0.035f,1.1f, 1.0f, 0.0015f, 0.0f, 0.9990f,  0.0f, 0.0f,  0.00045f,0.60f, 0, false, true  },  // sul ponticello
  { MODE_STRIKE, 0.11f, 1.0f, 1.0f, 0.0f,    0.0f, 0.9990f,  0.0f, 0.0f,  0.0030f, 0.12f, 0, false, false },  // col legno battuto
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
  float   over;       // overpressure (0 = off): crunch, PRESSURE knob picks subharmonics
  float   swarm;      // Threnody swarm: players glide apart into a quarter-tone cluster (semitones, 0 = off)
  uint8_t chr;        // playing character (adaptive playing)
};
static const StrModelDef STR_MODELS[MDL_COUNT] = {
  //  base               art layout trem vibD   vibR   swell trill accent tight  ring  gain    over  swarm character
  { INS_SOLO_VIOLIN,     -1, -1,   -1, 0.85f, 1.00f, 0.0f, 0,  1.00f, 0.80f, 1.0f, 0.850f, 0.0f, 0.0f, CHR_CLASSICAL },  // solo violin, classical
  { INS_SOLO_VIOLIN,     -1, -1,   -1, 1.35f, 0.92f, 0.4f, 0,  0.85f, 1.15f, 1.0f, 1.091f, 0.0f, 0.0f, CHR_ROMANTIC },  // solo violin, romantic
  { INS_SOLO_VIOLIN,     -1, -1,   -1, 1.10f, 1.00f, 0.9f, 0,  1.30f, 1.00f, 1.0f, 1.074f, 0.0f, 0.0f, CHR_FILM },  // solo violin, film
  { INS_SOLO_CELLO,      -1, -1,   -1, 0.85f, 1.00f, 0.0f, 0,  1.00f, 0.80f, 1.0f, 1.071f, 0.0f, 0.0f, CHR_CLASSICAL },  // solo cello, classical
  { INS_SOLO_CELLO,      -1, -1,   -1, 1.35f, 0.92f, 0.4f, 0,  0.85f, 1.15f, 1.0f, 1.378f, 0.0f, 0.0f, CHR_ROMANTIC },  // solo cello, romantic
  { INS_SOLO_CELLO,      -1, -1,   -1, 1.10f, 1.00f, 0.9f, 0,  1.30f, 1.00f, 1.0f, 1.285f, 0.0f, 0.0f, CHR_FILM },  // solo cello, film
  { INS_VIOLIN_SECTION,  -1, -1,   -1, 0.85f, 1.00f, 0.0f, 0,  1.00f, 0.80f, 1.0f, 0.981f, 0.0f, 0.0f, CHR_CLASSICAL },  // violin section, classical
  { INS_VIOLIN_SECTION,  -1, -1,   -1, 1.35f, 0.92f, 0.4f, 0,  0.85f, 1.15f, 1.0f, 1.190f, 0.0f, 0.0f, CHR_ROMANTIC },  // violin section, romantic
  { INS_VIOLIN_SECTION,  -1, -1,   -1, 1.10f, 1.00f, 0.9f, 0,  1.30f, 1.00f, 1.0f, 1.193f, 0.0f, 0.0f, CHR_FILM },  // violin section, film
  { INS_VIOLA_SECTION,   -1, -1,   -1, 0.85f, 1.00f, 0.0f, 0,  1.00f, 0.80f, 1.0f, 1.052f, 0.0f, 0.0f, CHR_CLASSICAL },  // viola section, classical
  { INS_VIOLA_SECTION,   -1, -1,   -1, 1.35f, 0.92f, 0.4f, 0,  0.85f, 1.15f, 1.0f, 1.093f, 0.0f, 0.0f, CHR_ROMANTIC },  // viola section, romantic
  { INS_VIOLA_SECTION,   -1, -1,   -1, 1.10f, 1.00f, 0.9f, 0,  1.30f, 1.00f, 1.0f, 1.229f, 0.0f, 0.0f, CHR_FILM },  // viola section, film
  { INS_CELLO_SECTION,   -1, -1,   -1, 0.85f, 1.00f, 0.0f, 0,  1.00f, 0.80f, 1.0f, 1.616f, 0.0f, 0.0f, CHR_CLASSICAL },  // cello section, classical
  { INS_CELLO_SECTION,   -1, -1,   -1, 1.35f, 0.92f, 0.4f, 0,  0.85f, 1.15f, 1.0f, 2.008f, 0.0f, 0.0f, CHR_ROMANTIC },  // cello section, romantic
  { INS_CELLO_SECTION,   -1, -1,   -1, 1.10f, 1.00f, 0.9f, 0,  1.30f, 1.00f, 1.0f, 1.922f, 0.0f, 0.0f, CHR_FILM },  // cello section, film
  { INS_LOW_STRINGS,     -1, -1,   -1, 0.85f, 1.00f, 0.0f, 0,  1.00f, 0.80f, 1.0f, 1.329f, 0.0f, 0.0f, CHR_CLASSICAL },  // celli + basses, classical
  { INS_LOW_STRINGS,     -1, -1,   -1, 1.35f, 0.92f, 0.4f, 0,  0.85f, 1.15f, 1.0f, 1.726f, 0.0f, 0.0f, CHR_ROMANTIC },  // celli + basses, romantic
  { INS_LOW_STRINGS,     -1, -1,   -1, 1.10f, 1.00f, 0.9f, 0,  1.30f, 1.00f, 1.0f, 1.727f, 0.0f, 0.0f, CHR_FILM },  // celli + basses, film
  { INS_FULL_STRINGS,    -1, -1,   -1, 0.85f, 1.00f, 0.0f, 0,  1.00f, 0.80f, 1.0f, 0.945f, 0.0f, 0.0f, CHR_CLASSICAL },  // full strings, classical
  { INS_FULL_STRINGS,    -1, -1,   -1, 1.35f, 0.92f, 0.4f, 0,  0.85f, 1.15f, 1.0f, 1.235f, 0.0f, 0.0f, CHR_ROMANTIC },  // full strings, romantic
  { INS_FULL_STRINGS,    -1, -1,   -1, 1.10f, 1.00f, 0.9f, 0,  1.30f, 1.00f, 1.0f, 1.383f, 0.0f, 0.0f, CHR_FILM },  // full strings, film
};

// noise transient per articulation: bow scratch at the start of a stroke,
// finger snap (pizzicato), wood click (col legno)
static const float STR_ARTIC_SCRATCH[ART_COUNT] = {
  0.35f,   // portamento
  0.35f,   // legato
  0.65f,   // detache
  1.00f,   // martele
  0.80f,   // staccato
  0.90f,   // spiccato
  0.80f,   // pizzicato
  0.30f,   // flageolet
  0.25f,   // sul tasto
  0.75f,   // sul ponticello
  1.20f,   // col legno
};

// lowest open string per family (Hz): notes below are played an octave up
static const float STR_FAMILY_LOW[STR_FAMILIES] = { 196.0f, 130.8f, 65.4f, 41.2f };
// highest notes in normal orchestral playing: violin G7, viola F#6, cello C6, bass G4
static const float STR_FAMILY_HIGH[STR_FAMILIES] = { 3136.0f, 1480.0f, 1046.5f, 392.0f };

// The SAMPLE TUNE knob spans the real range of the instrument (lead player's
// family): fully left = its lowest open string, fully right = its top note;
// the pitch CV adds 1 V/oct on top. Returns the lowest note and the octaves.
static inline void stringsPitchRange(uint8_t fam, float &lowHz, float &octaves) {
  if (fam >= STR_FAMILIES) fam = 0;
  lowHz = STR_FAMILY_LOW[fam];
  octaves = log2f(STR_FAMILY_HIGH[fam] / STR_FAMILY_LOW[fam]);
}

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
  float    vibGrowK;              // v1.37: vibrato growth speed (follows the note length)
  float vibWarm;                  // rate / width settle after the start (0.75 ... 1)
  float jitR, jitRT, jitD, jitDT; // rate / width wobble (current, target)
  float vibPrevS;                 // previous vibrato sine (cycle start detection)
  float fingerLen;                // stopped string length the finger is on
  float tremPh, tremInc;          // tremolo bow strokes
  float speedScale, pressOffs;    // this player's bow
  float detune;                   // static intonation (ratio)
  float drift, driftTarget;       // slow intonation drift (cents)
  // v1.38 humanize (ensembles): every player has a personality
  float hTime;                    // timing tendency: 0 = right with the leader, 1 = late
  float hRel;                     // release tendency: lets go early / late
  float hLev;                     // own level (a stronger / softer player)
  float hNote, hNoteT;            // this note's level (current, target)
  float hDrift, hDriftT;          // slow level drift: players lean in and back
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
  float slMul;                    // grip for this note's bow speed
  float bowUsed;                  // part of the bow used (1 = at the tip / frog)
  float bowLen;                   // this player's bow (seconds at normal speed)
  float bowLenInv;                // 1 / (bowLen * sample rate)
  uint32_t turn;                  // samples left in a bow change
  float slip;                     // last friction state (0 = stick, 1 = slip)
  float alfT;                     // overpressure: time in string periods (0 ... div)
  float swarmT;                   // this player's place in the swarm cluster (-1 ... 1)
  // note off: the released note keeps its pitch; the bow stops on the string
  // (short notes) or lifts off (long notes); later the finger lifts
  float lenNext, ringNext, openNext;   // targets for the next note (from control)
  float openLen;                  // open string under this note (finger lifted)
  float slideSemis;               // v1.35: size of the current finger move (semitones)
  float slideOpen;                // v1.35: the string the finger is on (its open length, 0 = lowest)
  float attackK;                  // smooth, instrument-aware bow attack coefficient
  float relRamp;                  // release: the bow starts to slow down gently (0 -> 1)
  float glideK;                   // finger travel speed (computed at control rate)
  float pe, peT;                  // phrase bow energy (smoothed, target)
  float vibIntent;                // vibrato intention for this note
  float vibGrow;                  // vibrato grows while the note lasts (0 -> 1)
  float levComp;                  // dynamic curve: level growth above normal is gentler
  float swK;                      // v1.35: this note's swell speed (follows the note length)
  uint16_t velWin;                // v1.34.1: samples left to catch this note's velocity peak
  float at, atSl, atVib, atLev;   // aftertouch (v1.34): CV IN RATE change since the note start
  float relK;                     // smooth release speed of the bow
  uint32_t relN;                  // samples since the release
  bool relStop;                   // bow stops on the string (damped) instead of lifting
  bool fingerOff;
  float tBp1, tBp2;               // band-pass state for the radiated scratch
  uint32_t nrng;                  // own noise generator (bow / rosin noise)
  float ringLoss;                 // string loss per round trip once the bow is off
  float hz;                       // this player's note (for the square outputs)
  uint8_t family;
  bool active, gate, pluck;
};

struct StrMode { float a1, a2, g, y1, y2; };

// "air": the top octave a mix engineer would lift with a high shelf. The
// body filters it away, so it is taken from the strings themselves (bow hair,
// rosin, top harmonics): 4th order highpass at 9.5 kHz, added after the body
struct StrAir {
  float b0, b1, b2, a1, a2;
  float z[2][2][2];               // states (lead / divisi, stage 1 / 2)
};

// Sympathetic resonance: the open strings that are not played ring along
// through the bridge when the played note (or one of its overtones) fits
// them. One set for player 1, one for the divisi players (their main family).
struct StrSym {
  float buf[STR_SYM_STRINGS][STR_SYM_LEN];
  float len[STR_SYM_STRINGS];     // loop length (samples)
  float g[STR_SYM_STRINGS];       // loop gain (ring time)
  float lp[STR_SYM_STRINGS];      // loop lowpass state (highs die faster)
  float in, out;                  // input coupling, output level for this family
  uint16_t w;
  uint8_t fam;
  uint8_t n;                      // strings this instrument has
  uint32_t idle;                  // samples without input (rung out -> skipped)
};

// ------------------------------------------------------------------ HAESE natural default
// Control Interpreter + Adaptive Playing Brain. Kept apart from the string
// physics: it only reads the gate / pitch history and hands per-note intentions
// to the players. Deterministic (no randomness): the same CV / gate phrase
// gives recognisably the same performance; the players' own small differences
// stay where they were.
struct StrBrain {
  // control interpreter: note history
  float    prevSemis;             // pitch of the previous note (semitones from A440)
  float    ioiAvg;                // typical time between notes (s)
  uint32_t onNow, offNow;         // sample time of the last gate start / end
  bool     haveOff;
  // phrase analysis
  uint8_t  idx;                   // note number in the phrase (0 = first)
  float    peak;                  // highest note so far in the phrase
  // intentions for the next note (read by the players at their stroke start)
  float    energy;                // bow energy (1 = neutral)
  float    attackMul;             // attack time (1 = neutral, > 1 = more gentle)
  float    vibIntent;             // vibrato intention (1 = neutral)
  bool     phraseStart;           // new phrase: down-bow, gentle placement
  bool     connected;             // tied to the previous note (no new bow)
  // v1.34: gate length, look-ahead, long-term arc, bow planning
  uint8_t  nHist;                 // notes with a known gate length (0 = none yet)
  float    dutyAvg;               // typical gate length / note interval
  float    predIoi, predGate;     // expected length of this note and of its gate (s)
  uint8_t  lenClass;              // 0 long (tied), 1 normal, 2 short, 3 fast/very short, 4 accented
  float    gapAvg;                // v1.37: usual silence between notes (s)
  uint8_t  gapN;
  bool     phraseEnd;             // v1.37: a rest follows this note (pattern look-ahead)
  uint8_t  phraseN;               // phrase number in the long arc (8 phrases)
  bool     downBow;               // this note falls on a heavy beat: down-bow
  float    velAvg;                // v1.36: typical velocity in this music
  float    accent;                // v1.36: accent intention for this note (1 = neutral)
  uint8_t  velN;                  // v1.36: notes in velAvg
};

struct StringsEngine {
  StrVoice v[STR_VOICES];
  StrMode body[STR_FAMILIES][STR_BODY_MODES];      // divisi players (2 ... 6)
  StrMode bodyLead[STR_FAMILIES][STR_BODY_MODES];  // player 1 (same resonances, own state)
  float famLpLead[STR_FAMILIES];
  float outDivisi;                // last sample of the divisi-only output
  float dc2x, dc2y;
  float bodyDirect[STR_FAMILIES];
  StrAir airF;
  StrSym sym[2];                  // sympathetic strings: player 1, divisi
  StrBrain brain;                 // HAESE: interpretation of the gate / pitch phrase
  // knobs a model does not need get a model-specific job (v1.33)
  float knobArt;                  // SQUARES MAIN TUNE knob + CV, 0 ... 1
  float knobSpread;               // SPREAD knob + CV, 0 ... 1
  float expr;                     // expression depth: 0 neutral, 1 natural, 2 very expressive
  float swarmW;                   // Threnody swarm width (semitones)
  float clusterW;                 // suspense: cluster width factor (0 = off)
  float glideMul, tremMul, strokeMul, accentMul, swellMul;
  uint8_t trillMode;              // 1 = semitone, 2 = whole tone, 3 = minor third
  struct Lim { float d[STR_LIM_LOOK]; float pk, g; uint8_t w, hold; } lim[2];   // safety net
  float tuneRatio;                // how far the pitch CV sits from A = 440 (the open strings follow)
  // control (written from loop)
  float rootHz;
  float speed;                    // bow speed 0 ... 1
  float pressure;                 // bow pressure 0 ... 1
  float beta;                     // bow position: 0.04 ponticello ... 0.3 tasto
  float speedCV;                  // expression CV multiplier (1 = none)
  float bowBase;                  // bow speed from the knob (log scale)
  float swellK;                   // note swell speed (from the model)
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
  float arcMul, attackChr, beatW;  // v1.35: playing character (long arc, attack, heavy-beat weight)
  float noteAhead;                // v1.34: this note's expected length (rhythm pattern look-ahead)
  bool  articLocked;              // v1.34: ARTIC CV patched -> no gate-length articulation
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
  uint32_t onG[STR_ONSETS];       // v1.34: gate length per onset (samples)
  uint32_t beatAnchor;            // v1.34: time of a heavy beat (bar start / accent)
  uint8_t  barPulses;             // v1.34: heavy-beat spacing in pulses (0 = unknown)
  float vibHz, tremHz;            // current vibrato rate, tremolo cycle rate
  float dcx, dcy, level;
  uint8_t famUsed;                // bodies with players
  uint32_t rng;
};

// ------------------------------------------------------------------ helpers
static inline float strClampV(float x) { return x < 0.0f ? 0.0f : (x > 1.3f ? 1.3f : x); }
static inline float strRand(StringsEngine &s) {            // -1 ... 1
  s.rng ^= s.rng << 13; s.rng ^= s.rng >> 17; s.rng ^= s.rng << 5;
  return (float)(int32_t)s.rng * (1.0f / 2147483648.0f);
}
static inline float strNoise(uint32_t &state) {
  // Xorshift32: avoids the short-pattern spectral artefacts of the old LCG.
  state ^= state << 13; state ^= state >> 17; state ^= state << 5;
  return (float)(int32_t)state * (1.0f / 2147483648.0f);
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
  { { 275, 12, 0.78f }, { 470, 18, 1.02f }, { 550, 18, 0.96f }, { 1000, 6, 0.48f }, { 2500, 2.5f, 0.78f }, { 3300, 5, 0.32f } },  // violin
  { { 230, 11, 0.78f }, { 350, 17, 1.02f }, { 430, 17, 0.96f }, {  850, 6, 0.48f }, { 2000, 2.5f, 0.74f }, { 2700, 5, 0.30f } }, // viola
  { { 100, 8, 0.72f }, { 145, 14, 0.94f }, { 205, 14, 0.90f }, {  400, 6, 0.46f }, { 1300, 2.5f, 0.58f }, { 2000, 5, 0.25f } },  // cello
  { {  58, 7, 0.58f }, {  95, 11, 0.78f }, { 125, 11, 0.70f }, {  250, 5, 0.42f }, {  800, 2.5f, 0.42f }, { 1300, 5, 0.18f } },  // bass
};
static const float STR_BODY_DIRECT[STR_FAMILIES] = { 0.35f, 0.4f, 0.45f, 0.5f };
// open strings per family (Hz): violin G D A E, viola C G D A, cello C G D A,
// bass E A D G; how long they ring on their own (s) and how strongly the
// bridge couples them (lower instruments ring longer and more)
static const float STR_OPEN[STR_FAMILIES][STR_SYM_STRINGS] = {
  { 196.0f, 293.66f, 440.0f, 659.26f, 0 }, { 130.81f, 196.0f, 293.66f, 440.0f, 0 },
  { 65.41f, 98.0f, 146.83f, 220.0f, 0 },   { 30.87f, 41.2f, 55.0f, 73.42f, 98.0f } };
static const uint8_t STR_SYM_COUNT[STR_FAMILIES] = { 4, 4, 4, 5 };   // orchestral bass: 5 strings (low B)
static const float STR_SYM_RING[STR_FAMILIES]  = { 1.8f, 2.2f, 3.0f, 3.5f };
static const float STR_SYM_GAIN[STR_FAMILIES]  = { 1.0f, 1.1f, 1.3f, 1.4f };
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
// v1.38: the section as a whole keeps its level: the players' own levels,
// note levels and drifts are spread around 1 (geometric mean = 1)
static inline void strHumanizeMean(StringsEngine &s, int which) {
  float lg = 0.0f;
  for (int i = 0; i < STR_VOICES; i++) {
    const StrVoice &v = s.v[i];
    lg += log2f(which == 0 ? v.hLev : (which == 1 ? v.hNoteT : v.hDriftT));
  }
  const float k = exp2f(-lg / STR_VOICES);
  for (int i = 0; i < STR_VOICES; i++) {
    StrVoice &v = s.v[i];
    if (which == 0) v.hLev *= k; else if (which == 1) v.hNoteT *= k; else v.hDriftT *= k;
  }
}

// v1.38: give the players their personalities (deterministic from power-up)
static inline void strHumanizeInit(StringsEngine &s) {
  for (int i = 0; i < STR_VOICES; i++) {
    StrVoice &v = s.v[i];
    v.hTime = i == 0 ? 0.0f : 0.25f + 0.75f * (0.5f + 0.5f * strRand(s));   // player 1 leads
    v.hRel  = 0.5f + 0.5f * strRand(s);
    v.hLev  = exp2f(0.25f * strRand(s));                     // +-1.5 dB
  }
  strHumanizeMean(s, 0);
}

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
    v.hTime = v.hRel = 0.5f; v.hLev = v.hNote = v.hNoteT = v.hDrift = v.hDriftT = 1.0f;
    v.gain = 1.0f;
    v.onset = 0; v.pluckN = v.pluckLen = 0; v.pluckAmp = 0.5f;
    v.strokeN = 0; v.dir = 1.0f; v.accent = 0.0f; v.swell = 1.0f;
    v.tran = 0.0f; v.tranHp = 0.0f; v.tremPrev = 1.0f; v.vel = 1.0f; v.prevB = 0.0f;
    v.vibDelay = 0; v.vibWarm = 1.0f; v.jitR = v.jitRT = v.jitD = v.jitDT = 1.0f;
    v.vibPrevS = 0.0f; v.fingerLen = v.len;
    v.bowMul = 1.0f; v.bowGain = 1.0f; v.slMul = 1.0f; v.bowUsed = 0.0f; v.bowLen = 3.0f * (0.85f + 0.3f * (0.5f + 0.5f * strRand(s)));
    v.bowLenInv = 1.0f / (v.bowLen * STR_SR);
    v.turn = 0; v.slip = 0.0f; v.tBp1 = v.tBp2 = 0.0f;
    v.alfT = 0.0f;
    v.lenNext = v.len; v.ringNext = 0.99f; v.openNext = v.openLen = v.slideOpen = 0.0f; v.slideSemis = 0.0f;
    v.attackK = 1.0f / (0.075f * STR_SR);
    v.relRamp = 1.0f; v.glideK = 0.0005f; v.pe = v.peT = 1.0f; v.vibIntent = 1.0f; v.vibGrow = 1.0f; v.vibGrowK = 1.0f / (1.4f * STR_SR); v.levComp = 1.0f; v.swK = 1.0f; v.velWin = 0; v.at = 0.0f; v.atSl = 1.0f; v.atVib = 1.0f; v.atLev = 1.0f;
    v.relK = 1.0f / (0.18f * STR_SR); v.relN = 0; v.relStop = false; v.fingerOff = false;
    {                                                        // quarter-tone places, spread over the players
      static const float places[STR_VOICES] = { 0.125f, -0.5f, 0.75f, -1.0f, 0.375f, -0.25f };
      v.swarmT = places[i];
    }
    v.nrng = 0x9E3779B9u * (uint32_t)(i + 1);
    v.ringLoss = 0.99f;
    v.hz = 220.0f;
    v.family = FAM_VIOLIN;
    v.active = (i == 0); v.gate = false; v.pluck = false;
  }
  s.rootHz = 220.0f; s.speed = 0.5f; s.pressure = 0.5f; s.beta = 0.12f; s.speedCV = 1.0f; s.bowBase = 0.113f; s.swellK = 1.0f;
  s.pizz = false; s.vibrato = true; s.tremolo = false; s.mute = false; s.artic = ART_LEGATO;
  for (int f = 0; f < STR_FAMILIES; f++) { s.famLp[f] = 0; s.famLpLead[f] = 0; s.famLpK[f] = 0.5f; }
  s.outDivisi = 0; s.dc2x = s.dc2y = 0;
  for (int k = 0; k < 2; k++) { for (int n = 0; n < STR_LIM_LOOK; n++) s.lim[k].d[n] = 0.0f; s.lim[k].pk = 0.0f; s.lim[k].g = 1.0f; s.lim[k].w = 0; s.lim[k].hold = 0; }
  {
    StrBrain &b = s.brain;
    b.prevSemis = 0.0f; b.ioiAvg = 0.6f; b.onNow = b.offNow = 0; b.haveOff = false;
    b.idx = 0; b.peak = -99.0f;
    b.energy = 1.0f; b.accent = 1.0f; b.velAvg = 1.0f; b.velN = 0; b.gapAvg = 0.0f; b.gapN = 0; b.phraseEnd = false; b.attackMul = 1.0f; b.vibIntent = 1.0f; b.phraseStart = true; b.connected = false;
  }
  s.bowNoise = 0.0f;
  s.transient = 1.0f;
  s.air = STR_AIR_DEFAULT;
  s.velocity = 1.0f;
  s.noteDur = 0.6f;                 // free playing: a normal bow
  s.sinceGate = (uint32_t)(5.0f * STR_SR); s.clockPeriod = 0;   // the first gate counts too
  s.now = 0; s.onW = 0; s.onN = 0; s.onCount = 0; s.onSeen = 0;
  s.noteAhead = 0.6f; s.articLocked = false; s.arcMul = s.attackChr = s.beatW = 1.0f; s.beatAnchor = 0; s.barPulses = 0;
  for (int i = 0; i < STR_ONSETS; i++) s.onG[i] = 0;
  s.pulse = 0; s.patLen = 0; s.patPulses = 0; s.beatCand = 0;
  strHumanizeInit(s); s.vibHz = 0; s.tremHz = 6.5f;
  s.instrument = MDL_SOLO_VIOLIN; s.layout = LAY_UNISON; s.maxVoices = STR_VOICES;
  s.trillPh = 0; s.trillInc = 9.0f / STR_SR; s.trillPrev = false;
  s.model = STR_MODELS[0]; s.articEff = ART_LEGATO;
  s.knobArt = 0.5f; s.knobSpread = 0.5f; s.expr = 1.0f; s.swarmW = 0.0f; s.clusterW = 0.0f;
  s.glideMul = s.tremMul = s.strokeMul = s.accentMul = s.swellMul = 1.0f; s.trillMode = 0;
  s.divisiMask = 0x3F;
  s.tuneRatio = 1.0f;
  for (int k = 0; k < 2; k++) {
    StrSym &y = s.sym[k];
    for (int j = 0; j < STR_SYM_STRINGS; j++) {
      for (int n = 0; n < STR_SYM_LEN; n++) y.buf[j][n] = 0.0f;
      y.len[j] = 200.0f; y.g[j] = 0.0f; y.lp[j] = 0.0f;
    }
    y.in = y.out = 0.0f; y.w = 0; y.fam = FAM_VIOLIN; y.n = 4; y.idle = 0;
  }
  {                                                         // air band: 4th order highpass 9.5 kHz
    StrAir &a = s.airF;
    const float w = 6.2831853f * 9500.0f / STR_SR, al = sinf(w) / (2.0f * 0.707f), cw = cosf(w);
    const float a0 = 1.0f + al;
    a.b0 = (1.0f + cw) * 0.5f / a0; a.b1 = -(1.0f + cw) / a0; a.b2 = a.b0;
    a.a1 = -2.0f * cw / a0; a.a2 = (1.0f - al) / a0;
    for (int ch = 0; ch < 2; ch++) for (int st = 0; st < 2; st++) a.z[ch][st][0] = a.z[ch][st][1] = 0.0f;
  }
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
    if (off == 2) { n = 3; s.onN = 3; s.beatAnchor = s.onT[(s.onW + STR_ONSETS - 3) % STR_ONSETS]; }
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
  bool accBar = false;                                     // v1.34: heavy beats from the accents
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
      // v1.34 bow planning: the accents are the heavy beats (down-bow)
      accBar = true;
      s.barPulses = (uint8_t)(sp > 255 ? 255 : sp);
      for (int i = n - 1; i >= 0; i--) if (acc[i]) { s.beatAnchor = s.onT[(start + i) % STR_ONSETS]; break; }
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
  if (!accBar) {
    // no accents: the bar is the ostinato (when it spans 2+ beats) or 4 beats,
    // counted from the first note of the rhythm
    const int bp = (beat && bar >= 2 * beat) ? bar : 4 * (beat ? beat : 1);
    s.barPulses = (uint8_t)(bp > 255 ? 255 : bp);
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

// v1.35: a legato pitch change while the note sounds. A finger can only
// slide along ONE string: the new note has to lie on the string the finger
// is on (at or above its open pitch) and within an octave. Otherwise the
// player crosses to the string of the new note: the pitch changes at once
// (with a soft finger click), no slide across strings.
static inline void strFingerMove(StringsEngine &s, StrVoice &v, float newLen, float newOpen) {
  const float semis = 12.0f * log2f(strClamp(v.len, 1.0f, 1e5f) / strClamp(newLen, 1.0f, 1e5f));
  if (fabsf(semis) < 0.4f) { v.lenTarget = newLen; return; }            // drift, vibrato centre: nothing to decide
  const bool onString = v.slideOpen <= 0.0f || newLen <= v.slideOpen * 1.01f;
  if (onString && fabsf(semis) <= STR_SLIDE_MAX) {
    if (fabsf(newLen - v.lenTarget) > 0.01f * newLen) v.slideSemis = fabsf(semis);   // a new move
    v.lenTarget = newLen;                                    // slide on the same string
    v.openLen = v.slideOpen > 0.0f ? v.slideOpen : newOpen;  // the finger stays on it
  } else {
    v.len = v.lenTarget = v.fingerLen = newLen;              // string crossing: no slide
    v.openLen = v.slideOpen = newOpen;
    v.tran += 0.004f * (0.5f + 0.5f * strClamp(v.vel, 0.0f, 1.0f)) * (0.9f + 0.1f * strRand(s));
  }
}

// v1.34: the articulation in use. AUTO = from the gate length; the bowed
// strokes detache ... spiccato follow the gate length around the knob's
// choice (a long gate one step more connected, a short one shorter);
// legato, portamento, pizzicato and the tone colours stay as chosen, and so
// does everything when the ARTIC CV is patched (an explicit CV wins)
static const uint8_t STR_ART_BY_LEN[5][5] = {
  //  long          normal        short         very short / fast   accented
  { ART_LEGATO,   ART_DETACHE,  ART_STACCATO, ART_SPICCATO,        ART_MARTELE  },   // AUTO
  { ART_LEGATO,   ART_DETACHE,  ART_STACCATO, ART_SPICCATO,        ART_MARTELE  },   // detache
  { ART_MARTELE,  ART_MARTELE,  ART_STACCATO, ART_STACCATO,        ART_MARTELE  },   // martele
  { ART_DETACHE,  ART_STACCATO, ART_STACCATO, ART_SPICCATO,        ART_MARTELE  },   // staccato
  { ART_STACCATO, ART_SPICCATO, ART_SPICCATO, ART_SPICCATO,        ART_SPICCATO },   // spiccato
};
static inline uint8_t strArticResolve(const StringsEngine &s) {
  const StrModelDef &M = s.model;
  if (s.pizz) return ART_PIZZICATO;
  if (M.artic >= 0) return (uint8_t)M.artic;
  const uint8_t c = s.brain.lenClass < 5 ? s.brain.lenClass : 1;
  if (s.artic >= ART_COUNT) return STR_ART_BY_LEN[0][c];                  // AUTO
  if (!s.articLocked && s.artic >= ART_DETACHE && s.artic <= ART_SPICCATO)
    return STR_ART_BY_LEN[1 + s.artic - ART_DETACHE][c];
  return s.artic;
}

static inline void stringsControl(StringsEngine &s) {
  stringsTempo(s);
  const StrModelDef &M = STR_MODELS[s.instrument < MDL_COUNT ? s.instrument : 0];
  const uint8_t ins = M.ins;
  const uint8_t lay = M.layout >= 0 ? (uint8_t)M.layout : (s.layout < LAY_COUNT ? s.layout : 0);
  const bool solo = (ins == INS_SOLO_VIOLIN || ins == INS_SOLO_VIOLA || ins == INS_SOLO_CELLO);
  const bool full = (ins == INS_FULL_STRINGS);
  const bool low  = (ins == INS_LOW_STRINGS);
  const bool divisi = (lay == LAY_DIVISI_MAJOR || lay == LAY_DIVISI_MINOR);
  const int8_t *pool = (lay == LAY_DIVISI_MAJOR) ? STR_DIVISI_MAJOR : STR_DIVISI_MINOR;

  s.model = M;                                             // (the resolver reads it)
  const uint8_t art = strArticResolve(s);
  s.articEff = art;
  const StrArticDef &A = STR_ARTIC[art];
  // ---- SPREAD: on a soloist (no layout to choose) the expression depth;
  //      the character sets how the player interprets the phrase
  {
    const StrCharDef &C = STR_CHAR[M.chr < CHR_COUNT ? M.chr : 0];
    const float sp = strClamp(s.knobSpread, 0.0f, 1.0f);
    s.expr = (solo ? 2.0f * sp : 1.0f) * C.expr;
    s.arcMul = C.arc; s.attackChr = C.attack; s.beatW = C.beat;
    s.swarmW = 0.0f; s.clusterW = 0.0f;
    s.glideMul = C.glide; s.tremMul = s.strokeMul = s.accentMul = s.swellMul = 1.0f;
    s.trillMode = M.trill;
  }
  // the PRESSURE knob is the playing zone, contact point and force together
  // like a player uses them: left = over the fingerboard with a light bow
  // (soft, dark), centre = the articulation's normal contact point, right =
  // close to the bridge with more force (loud, bright). The contact point
  // moves 2x either way from the articulation's own; the force stays inside
  // the clean (Helmholtz) window, from its light to its firm side
  const float zone = strClamp(s.pressure, 0.0f, 1.0f);
  // a fast bow cannot play right at the bridge (it needs more force than a
  // player has), so players move away from it: the closest point follows
  // the bow speed
  const float betaMin = 0.03f + 0.045f * strClamp(s.speed, 0.0f, 1.0f);
  s.beta = strClamp(A.beta * exp2f(1.0f - 2.0f * zone), betaMin, 0.30f);
  const float press = strClamp((0.35f + 0.3f * zone) * A.press, 0.0f, 1.0f);
  // the CLOCK RATE knob: bow speed on a log scale like real bow speeds,
  // ~9 cm/s (fully left) ... ~25 cm/s (centre) ... ~70 cm/s (fully right)
  s.bowBase = 0.04f * powf(8.0f, strClamp(s.speed, 0.0f, 1.0f));
  s.swellK = M.swell > 0 ? 1.0f / (M.swell * s.swellMul * STR_SR) : 1.0f;
  // the force is relative to the playable (Helmholtz) window, which moves
  // with the contact point and the bow speed like on a real instrument
  // (Schelleng): over the fingerboard a light bow, near the bridge more force
  {
    const float bb = strClamp(s.beta, 0.03f, 0.30f);
    // friction slope for a normal tone (measured): grows with the bow
    // distance, and a faster bow needs more grip (smaller slope)
    const float centre = 26.0f * powf(bb, 1.1f) * powf(s.bowBase * s.speedCV / 0.113f + 0.05f, -0.6f);
    s.slope = centre * powf(3.0f, 1.0f - 2.0f * press);
  }

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

  // Expressive modulation follows the player's actual gesture, not the detected beat.
  // Bow speed, pressure, dynamic level and note spacing shape the hand motion.
  const float adaptiveDrive = strClamp(0.45f * s.speed + 0.35f * s.pressure
                                      + 0.20f * strClamp(s.velocity / 1.25f, 0.0f, 1.0f), 0.0f, 1.0f);
  const float adaptiveNoteRate = sqrtf(strClamp(0.6f / strClamp(s.noteDur, 0.08f, 2.5f), 0.5f, 1.5f));
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
    if (solo)      { fam = (ins == INS_SOLO_CELLO) ? FAM_CELLO : (ins == INS_SOLO_VIOLA ? FAM_VIOLA : FAM_VIOLIN); semis = 0; }
    else if (full) { fam = STR_FULL_FAMILY[i]; semis = STR_FULL_LAYOUT[lay][i]; }
    else if (low)  { fam = STR_LOW_FAMILY[i];  semis = STR_LOW_LAYOUT[lay][i]; }
    else           { fam = (ins == INS_CELLO_SECTION) ? FAM_CELLO : (ins == INS_VIOLA_SECTION ? FAM_VIOLA : FAM_VIOLIN);
                     semis = STR_SECTION_LAYOUT[lay][i]; }
    if (divisi && !solo && i > 0) semis += pool[s.divisiTone[i] & 7];
    semis += A.semis;
    v.family = fam;
    // Vibrato rate follows expression and phrase length, with small player-to-player variation.
    const float vibRate = STR_VIB_RATE[fam] * M.vibRate * v.vibRateMul
                        * (0.86f + 0.24f * adaptiveDrive) * (0.96f + 0.08f * adaptiveNoteRate);
    v.vibK = 6.2831853f * vibRate / STR_SR;
    // Tremolo bow strokes adapt to bow speed, pressure, dynamics and note spacing.
    // No beat subdivision or BPM locking: the hand's motion drives the rate.
    const float tremRate = (4.5f + 3.5f * adaptiveDrive) * adaptiveNoteRate * v.vibRateMul;
    v.tremInc = tremRate * s.tremMul / STR_SR;
    v.vibDepth = STR_VIB_DEPTH[fam] * v.vibDepthMul * M.vibDepth * (0.4f + 0.6f * s.expr);
    // finger travel (was per sample): interval, phrase length and playing
    // energy shape the glide; the cello's finger has more weight
    {
      const StrArticDef &AA = STR_ARTIC[s.articEff < ART_COUNT ? s.articEff : (uint8_t)ART_LEGATO];
      // v1.35 realistic finger moves on one string (crossings jump, see
      // strFingerMove):
      //  - portamento: an audible slide, longer for bigger intervals
      //  - legato: inside the hand (up to a major third) the finger just
      //    drops, ~10 ms; a shift to another position is a short, light
      //    slide (~35-80 ms, the romantic player lets it be heard more)
      float gk = AA.glide;
      if (AA.legato && AA.glide > 0.0f) {
        const float iv = v.slideSemis;
        const float weight = (fam == FAM_CELLO) ? 1.15f : (fam == FAM_BASS ? 1.25f : 1.0f);
        float tau;
        if (art == ART_PORTAMENTO) tau = (0.10f + 0.012f * iv) * weight;                 // 0.1 ... 0.25 s
        else if (iv <= 4.0f)       tau = 0.010f;                                          // finger drop
        else                       tau = (0.030f + 0.004f * iv) * weight;                 // a shift
        if (art == ART_PORTAMENTO || iv > 4.0f) tau *= s.glideMul;                       // character: audible shifts
        gk = 1.0f / (strClamp(tau, 0.006f, 0.4f) * STR_SR);
      }
      v.glideK = strClamp(gk, 0.00005f, 0.05f);
    }
    // dynamic curve (HAESE P(D)): above normal playing the level grows ~30 %
    // as steeply (the tone still gets brighter and grippier), below normal it
    // is untouched, so soft stays soft and firm playing keeps its headroom
    {
      const float intensity = (s.bowBase * s.speedCV / 0.113f) * exp2f(2.2f * (strClamp(s.pressure, 0.0f, 1.0f) - 0.5f))
                            * STR_VEL_LEV(v.vel) * v.bowMul * v.pe;
      const float t = intensity > 1.0f ? powf(intensity, -STR_DYN_COMP) : 1.0f;
      v.levComp += (t - v.levComp) * 0.25f;
    }
    // v1.34 aftertouch: CV IN RATE pressed further than at the note start
    // = more bow weight (grip, brighter) and a wider vibrato; easing off =
    // lighter and calmer. Without CV the level never moves, so nothing changes
    {
      // only after the velocity is caught, with a small dead band against CV noise
      float d = (v.gate && !v.velWin) ? s.velocity - v.vel : 0.0f;
      d = d > 0.04f ? d - 0.04f : (d < -0.04f ? d + 0.04f : 0.0f);
      d = strClamp(d, -0.8f, 0.8f);
      v.at += (d - v.at) * 0.3f;
      // v1.36 velocity colours the tone too: hard = more grip (bright,
      // biting), soft = lighter grip (round, dark); no CV (1.0) = unchanged
      v.atSl = exp2f(-2.0f * v.at - 0.6f * (strClamp(v.vel, 0.0f, 1.3f) - 1.0f));
      v.atLev = 1.0f + 0.35f * v.at;                        // ~+1.3 dB per volt-ish of pressing
      v.atVib = 1.0f + (v.at > 0.0f ? 0.9f * v.at : 0.4f * v.at);
    }

    // slow intonation drift: every player wanders a few cents
    if ((s.rng & 0xFF) < 2) v.driftTarget = 4.0f * strRand(s);
    strRand(s);
    v.drift += (v.driftTarget - v.drift) * 0.002f;

    float swarmSemis = 0.0f;
    if (M.swarm > 0.0f) {
      // Threnody swarm: from the note the players glide apart into a
      // quarter-tone cluster and back together (cycle ~12 s from every gate)
      const float t = (float)s.sinceGate / STR_SR;
      float ph = t / 12.0f; ph -= floorf(ph);
      const float spread = 0.5f - 0.5f * cosf(6.2831853f * ph);
      swarmSemis = spread * s.swarmW * v.swarmT;
    }
    if (s.clusterW > 0.0f) swarmSemis += (float)semis * (s.clusterW - 1.0f);  // suspense: cluster width
    float hz = s.rootHz * exp2f(((float)semis + swarmSemis) / 12.0f);
    while (hz < STR_FAMILY_LOW[fam]) hz *= 2.0f;            // keep it on the instrument
    while (hz > STR_FAMILY_HIGH[fam] * 1.02f) hz *= 0.5f;
    if (!solo) hz *= v.detune;
    hz *= exp2f(v.drift / 1200.0f);
    v.hz = hz;
    // loop length minus the delay of the bridge filter and the junction
    float len = STR_SR / hz - strLpDelay(s.bridgeK, hz) - s.lenComp;
    // ring-out after the bow / pluck: the same time for every note
    v.ringNext = powf(0.001f, 1.0f / (A.ring * M.ring * (s.mute ? 0.6f : 1.0f) * hz));
    v.lenNext = strClamp(len, 8.0f, (float)(STR_NECK_LEN - 8));
    {                                                        // the open string below this note
      static const float OPEN[STR_FAMILIES][4] = {
        { 196.0f, 293.66f, 440.0f, 659.26f }, { 130.81f, 196.0f, 293.66f, 440.0f },
        { 65.41f, 98.0f, 146.83f, 220.0f },   { 41.2f, 55.0f, 73.42f, 98.0f } };
      float oh = 0.0f;
      for (int k = 0; k < 4; k++) if (OPEN[fam][k] * (solo ? 1.0f : v.detune) <= hz * 0.97f) oh = OPEN[fam][k] * (solo ? 1.0f : v.detune);
      v.openNext = oh > 0.0f ? strClamp(STR_SR / oh - strLpDelay(s.bridgeK, oh) - s.lenComp, 8.0f, (float)(STR_NECK_LEN - 8)) : 0.0f;
    }
    // while a note is released it keeps its pitch (a sequencer may already
    // be on the next note); the next gate takes the new one
    if (v.gate && v.env > 0.05f && !v.fingerOff) {
      strFingerMove(s, v, v.lenNext, v.openNext); v.ringLoss = v.ringNext;   // pitch moves under a held note
    } else if (v.gate || v.relN > (uint32_t)(3.0f * STR_SR)) {
      v.lenTarget = v.lenNext; v.ringLoss = v.ringNext; v.openLen = v.slideOpen = v.openNext;
    }

    // family balance in the full section, at the same pitch
    v.gain = (fam == FAM_BASS) ? 0.85f : (fam == FAM_VIOLA ? 0.95f : (fam == FAM_CELLO ? 0.8f : 1.0f));
    // v1.38 humanize the ensemble's level: each player's own level, a small
    // difference per note and a slow drift (players lean in and back over a
    // few seconds); looser in the romantic character, tighter in classical
    if (!solo) {
      if (strRand(s) > 0.9985f) { v.hDriftT = exp2f(0.13f * M.tight * strRand(s)); strHumanizeMean(s, 2); }   // new drift every ~2 s, +-0.8 dB
      v.hDrift += (v.hDriftT - v.hDrift) * 0.004f;
      v.hNote  += (v.hNoteT - v.hNote) * 0.2f;
      v.gain *= exp2f(log2f(v.hLev) * M.tight) * v.hNote * v.hDrift;   // hLev^tight: mean stays 1
    }
    // No register compensation: v1.20 and earlier lifted everything above 2.5x
    // the lowest open string by up to 6 dB, which made the top of the violin
    // +6 dB, the viola +7 and the cello +9 dB against their own lowest note --
    // measured with tests/strings_level_report.cpp. A real instrument does not
    // get louder as it plays higher, so the lift is gone and the small loss the
    // short loop really has (-1.7 dB on the violin's top note) stands.
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
  // sympathetic strings: tuned like the instrument, following the pitch CV's
  // offset from A = 440 (a player tunes to the ensemble)
  {
    const float semis = 12.0f * log2f(s.rootHz / 440.0f);
    const float frac = semis - floorf(semis + 0.5f);
    s.tuneRatio += (exp2f(frac / 12.0f) - s.tuneRatio) * 0.05f;
    uint8_t cnt[STR_FAMILIES] = { 0, 0, 0, 0 };
    for (int i = 1; i < STR_VOICES; i++) if (s.v[i].active) cnt[s.v[i].family]++;
    uint8_t dfam = s.v[0].family;
    for (int f = 0, best = 0; f < STR_FAMILIES; f++) if (cnt[f] > best) { best = cnt[f]; dfam = (uint8_t)f; }
    float dhz = s.v[0].hz;                                   // a divisi player's note of that family
    for (int i = 1; i < STR_VOICES; i++) if (s.v[i].active && s.v[i].family == dfam) { dhz = s.v[i].hz; break; }
    const uint8_t fams[2] = { s.v[0].family, dfam };
    const float notes[2] = { s.v[0].hz, dhz };
    for (int k = 0; k < 2; k++) {
      StrSym &y = s.sym[k];
      const uint8_t f = fams[k];
      y.fam = f;
      y.n = STR_SYM_COUNT[f];
      // the string the note is played on (highest open string at or below
      // it) is under the bow and the finger: it does not ring along
      int played = -1;
      for (int j = 0; j < y.n; j++) if (STR_OPEN[f][j] * s.tuneRatio <= notes[k] * 1.01f) played = j;
      for (int j = 0; j < y.n; j++) {
        const float hz = STR_OPEN[f][j] * s.tuneRatio;
        y.len[j] = strClamp(STR_SR / hz - 0.6f, 8.0f, (float)(STR_SYM_LEN - 4));   // minus the loop filter delay
        y.g[j] = (j == played) ? 0.9f
               : powf(0.001f, 1.0f / (STR_SYM_RING[f] * (s.mute ? 0.5f : 1.0f) * hz));
      }
      y.in = 0.02f * STR_SYM_GAIN[f];
      y.out = STR_SYM_LEVEL * (s.mute ? 0.5f : 1.0f);
    }
  }
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

// the open strings: feedback combs (all harmonics), highs damped in the loop
static inline float strSymTick(StrSym &y, float x) {
  // nothing coming in for 4 s: the open strings have rung out, skip them
  if (x == 0.0f) { if (y.idle < (uint32_t)(4.0f * STR_SR)) y.idle++; else return 0.0f; }
  else y.idle = 0;
  const float in = x * y.in;
  float out = 0.0f;
  const uint16_t w = y.w;
  for (int j = 0; j < y.n; j++) {
    float rp = (float)w - y.len[j];
    if (rp < 0.0f) rp += (float)STR_SYM_LEN;
    uint32_t i0 = (uint32_t)rp;
    const float fr = rp - (float)i0;
    const uint32_t i1 = (i0 + 1 >= STR_SYM_LEN) ? 0 : i0 + 1;
    const float r = y.buf[j][i0] + (y.buf[j][i1] - y.buf[j][i0]) * fr;
    y.lp[j] += (r - y.lp[j]) * 0.6f;
    y.buf[j][w] = in + y.g[j] * y.lp[j];
    out += r;
  }
  y.w = (uint16_t)(w + 1 >= STR_SYM_LEN ? 0 : w + 1);
  return out * y.out;
}

// HAESE natural default: interpret the new note from the phrase so far.
// Called once per gate start. All rules are small and deterministic.
static inline void strBrainNote(StringsEngine &s) {
  StrBrain &b = s.brain;
  const float semis = 12.0f * log2f(strClamp(s.rootHz, 16.0f, 8000.0f) / 440.0f);
  const float ioi = s.noteDur;                              // time since the previous gate start
  const float gap = b.haveOff ? (float)(s.now - b.offNow) / STR_SR : 9.0f;   // silence before this note
  // phrase continuity: a real rest (long against the rhythm) or a long pause
  // starts a new phrase; a gate that hardly opened is a connected note
  // v1.37: a rest is a silence clearly longer than the usual silence
  // between notes -- in staccato every note has a gap, that is no rest
  const bool rest = gap > 0.25f && gap > 0.6f * b.ioiAvg && (b.gapN < 2 || gap > 1.7f * b.gapAvg + 0.05f);
  const bool newPhrase = (b.idx == 0 && b.peak < -90.0f) || rest || ioi > 2.4f * b.ioiAvg || gap > 1.2f;
  // tied: (almost) no silence, relative to the tempo
  b.connected = !newPhrase && gap < fminf(0.04f, 0.15f * ioi);
  if (gap < 3.0f) { b.gapAvg = b.gapN ? b.gapAvg + (gap - b.gapAvg) * 0.3f : gap; if (b.gapN < 255) b.gapN++; }
  if (s.onN <= 1) b.gapN = 0;
  const float interval = newPhrase ? 0.0f : semis - b.prevSemis;
  if (newPhrase) { b.idx = 0; b.peak = semis; b.phraseN = s.onN <= 1 ? 0 : (uint8_t)(b.phraseN + 1); }

  // ---- v1.34 gate length as information: the previous gate against its
  //      interval (the duty) -> how long the gates are in this music
  const uint8_t k  = (uint8_t)((s.onW + STR_ONSETS - 1) % STR_ONSETS);   // this note
  const uint8_t kp = (uint8_t)((k + STR_ONSETS - 1) % STR_ONSETS);       // the previous note
  if (s.onN <= 1) b.nHist = 0;                                            // a new rhythm
  else if (ioi < 3.0f) {
    const float g = (b.haveOff && b.offNow > b.onNow) ? (float)(b.offNow - b.onNow) / STR_SR : ioi;
    s.onG[kp] = (uint32_t)(strClamp(g, 0.0f, ioi) * STR_SR);
    const float d = strClamp(g / (ioi > 0.01f ? ioi : 0.01f), 0.0f, 1.0f);
    b.dutyAvg = b.nHist ? b.dutyAvg + (d - b.dutyAvg) * 0.5f : d;
    if (b.nHist < 255) b.nHist++;
  }
  // look-ahead: in a recognised pattern this note will be as long as the
  // same note one pattern ago; otherwise the typical note with the typical gate
  b.predIoi = b.nHist ? b.ioiAvg : 0.6f;
  b.predGate = b.dutyAvg * b.predIoi;
  {
    const int L = s.patLen;
    if (L && b.nHist && s.onN > L + 1) {
      const uint8_t a0 = (uint8_t)((k + STR_ONSETS - L) % STR_ONSETS), a1 = (uint8_t)((a0 + 1) % STR_ONSETS);
      b.predIoi  = (float)(s.onT[a1] - s.onT[a0]) / STR_SR;
      b.predGate = (float)s.onG[a0] / STR_SR;
    }
  }
  s.noteAhead = b.nHist ? strClamp(b.predIoi, 0.05f, 3.0f) : s.noteDur;
  // length class for the articulation
  {
    const float duty = b.predGate / (b.predIoi > 0.01f ? b.predIoi : 0.01f);
    // v1.37 tempo-aware, the way a player chooses the stroke:
    //  - tied or (almost) full gates: legato, also in fast runs (slurred)
    //  - very fast notes (< 0.11 s): the bow bounces by itself (spiccato /
    //    sautille), whatever the gate does
    //  - short gates: in a fast tempo (< 0.28 s per note) spiccato, slower
    //    a stopped staccato; a short gate on a long note (< 0.18 s) too
    //  - otherwise detache
    const bool fast = b.predIoi < 0.28f, veryFast = b.predIoi < 0.11f;
    b.lenClass = !b.nHist || b.connected || duty > 0.9f ? 0
               : veryFast ? 3
               : (duty < 0.55f || b.predGate < 0.18f) ? (fast ? 3 : 2)
               : 1;
    // a rest after this note (only known inside a recognised pattern)
    const float gapPred = b.predIoi - b.predGate;
    b.phraseEnd = s.patLen && b.nHist && gapPred > 0.25f && gapPred > 0.5f * b.predIoi;
  }
  s.articEff = strArticResolve(s);                          // takes effect on this very note
  // bow energy: a phrase opens a little gentler and grows over its first
  // notes; rising lines lean in, falling lines relax; a new high point of
  // the phrase gets a little more
  // v1.36: more energetic -- a wider build-up over the first notes, lines
  // that clearly lean in / relax, a new high point that really arrives
  float e = 0.86f + 0.18f * (float)(b.idx < 4 ? b.idx : 4) / 4.0f;
  e += 0.022f * strClamp(interval, -7.0f, 7.0f);
  float vib = 1.0f;
  float att = 1.0f;
  float acc = 1.0f;
  if (!newPhrase && semis > b.peak + 0.5f) { e += 0.10f; vib += 0.20f; acc *= 1.2f; b.peak = semis; }
  // v1.36 adaptive: the player listens to how it is being played --
  //  - a note harder than the music around it is meant: more bow, a sharper
  //    attack and a real accent; a softer one is taken back further
  //  - notes coming faster than before drive the line forward; slowing
  //    down broadens it (less bow pressure, more singing vibrato)
  //    (the velocity part runs 6 ms later, in strBrainVel: the CV may
  //    arrive a moment after the gate)
  {
    if (!newPhrase && b.nHist >= 3 && ioi < 3.0f && b.ioiAvg > 0.05f) {
      const float r = b.ioiAvg / (ioi > 0.05f ? ioi : 0.05f);   // > 1: faster than before
      if (r > 1.12f) { e += 0.08f * strClamp(r - 1.0f, 0.0f, 1.0f); att *= 0.9f; }
      if (r < 0.88f) { e -= 0.04f; vib += 0.25f * strClamp(1.0f - r, 0.0f, 0.6f); }
    }
  }
  // big leaps: a more deliberate placement and an expressive arrival
  const float leap = interval < 0 ? -interval : interval;
  if (leap >= 5.0f) { att *= 1.10f; vib += 0.10f; }
  // repeated notes: alternate bows feel different (down a little heavier)
  if (!newPhrase && leap < 0.5f) e *= (b.idx & 1) ? 0.97f : 1.02f;
  if (newPhrase) att *= 1.25f;                               // the bow is placed, not thrown
  att *= s.attackChr;                                        // v1.35 character
  // vibrato intention from the note lengths: quick notes hardly any, long
  // notes the full vibrato (it then still grows while the note lasts)
  const float len = strClamp(b.nHist ? b.predIoi : b.ioiAvg, 0.05f, 3.0f);
  vib *= len < 0.22f ? 0.35f : (len < 0.5f ? 0.35f + 0.65f * (len - 0.22f) / 0.28f : 1.0f);
  // v1.34 vibrato as a reaction: a soft, long note arrived at by a leap up
  // gets the most vibrato; quick passages almost none (above)
  if (interval >= 3.0f && b.predIoi > 0.6f && s.velocity < 0.8f) vib += 0.30f;
  // v1.34 long-term arc: over 8 phrases the playing builds up and settles
  // again, so a repeated sequence sounds different the 4th time (deterministic)
  {
    static const float ARC[8] = { 0.95f, 0.98f, 1.00f, 1.03f, 1.06f, 1.08f, 1.02f, 0.97f };
    const float arc = ARC[b.phraseN & 7];
    const float am = 1.0f + (arc - 1.0f) * s.arcMul;
    e *= am; vib += 1.5f * (am - 1.0f);
  }
  // v1.34 bow planning: heavy beats (bar start / accents) on a down-bow and
  // a little weight; the notes in between alternate (with a natural retake)
  b.downBow = false;
  if (s.pulse && s.barPulses && b.nHist) {
    const float m = (float)(s.now - s.beatAnchor) / (float)s.pulse;
    const int mi = (int)(m + 0.5f);
    if (fabsf(m - (float)mi) < 0.25f && mi % s.barPulses == 0) { b.downBow = true; e *= 1.0f + 0.05f * s.beatW; acc *= 1.0f + 0.25f * s.beatW; }
  }
  // v1.37: a short or normal note on a heavy beat / phrase start, not in a
  // fast passage, gets a bitten martele stroke; a phrase-ending note eases
  if ((b.downBow || newPhrase) && b.nHist && (b.lenClass == 1 || b.lenClass == 2) && b.predIoi >= 0.2f) {
    b.lenClass = 4; s.articEff = strArticResolve(s);
  }
  if (b.phraseEnd) { e *= 0.95f; vib += 0.15f; }
  // v1.37 tempo: fast notes are attacked crisper, slow ones placed
  att *= strClamp(sqrtf((b.nHist ? b.predIoi : 0.6f) / 0.5f), 0.65f, 1.15f);
  // expression depth (SPREAD on models without a layout to choose): 0 = a
  // neutral player, 1 = natural default, 2 = twice the shaping
  const float x = s.expr;
  b.energy = strClamp(1.0f + (e - 1.0f) * x, 0.60f, 1.45f);
  b.accent = strClamp(1.0f + (acc - 1.0f) * x, 0.5f, 2.5f);
  b.attackMul = strClamp(1.0f + (att - 1.0f) * x, 0.6f, 1.6f);
  b.vibIntent = strClamp(1.0f + (vib - 1.0f) * x, 0.15f, 1.6f);
  b.phraseStart = newPhrase;
  // history
  if (ioi < 3.0f) b.ioiAvg += (ioi - b.ioiAvg) * 0.3f;
  b.prevSemis = semis;
  if (b.idx < 255) b.idx++;
  b.onNow = s.now;
}

// v1.36: the velocity part of the interpretation, 6 ms after the gate (the
// velocity CV has settled by then): a note harder than the music around it
// gets more bow and a real accent, a softer one is taken back further
static inline void strBrainVel(StringsEngine &s) {
  StrBrain &b = s.brain;
  const float vel = strClamp(s.velocity, 0.0f, 1.3f);
  const float dv = b.velN ? vel - b.velAvg : 0.0f;
  b.velAvg = b.velN ? b.velAvg + (vel - b.velAvg) * 0.25f : vel;
  if (b.velN < 255) b.velN++;
  const float x = s.expr;
  const float ev = 1.0f + 0.30f * strClamp(dv, -0.5f, 0.5f) * x;
  float acc = dv > 0.12f ? 1.3f + 2.0f * (dv - 0.12f) : (dv < -0.12f ? 0.7f : 1.0f);
  acc = strClamp(1.0f + (acc - 1.0f) * x, 0.5f, 2.5f);
  b.energy = strClamp(b.energy * ev, 0.60f, 1.50f);
  b.accent = strClamp(b.accent * acc, 0.4f, 3.0f);
  for (int i = 0; i < STR_VOICES; i++) {                    // players that already started
    StrVoice &v = s.v[i];
    if (!v.active || !v.gate || v.onset) continue;
    v.peT = b.energy;
    v.accent *= acc;
  }
}

// safety net before the DAC: a look-ahead limiter that only moves the gain
// when a peak would go past the ceiling (normally it does nothing)
template <typename L> static inline float strLimit(L &l, float x) {
  const float a = x < 0 ? -x : x;
  // hold every peak for as long as it is inside the look-ahead, then release
  if (a >= l.pk) { l.pk = a; l.hold = STR_LIM_LOOK; }
  else if (l.hold) l.hold--;
  else l.pk *= 0.99974f;
  const float gt = l.pk > STR_LIM_CEIL ? STR_LIM_CEIL / l.pk : 1.0f;
  l.g += (gt - l.g) * (gt < l.g ? 0.15f : 0.0005f);       // settles fully within the look-ahead
  const float y = l.d[l.w];
  l.d[l.w] = x;
  l.w = (uint8_t)((l.w + 1) % STR_LIM_LOOK);
  return y * l.g;
}

static inline float strAirHp(StrAir &a, int ch, float x) {
  for (int st = 0; st < 2; st++) {
    float *z = a.z[ch][st];
    const float y = a.b0 * x + z[0];
    z[0] = a.b1 * x - a.a1 * y + z[1];
    z[1] = a.b2 * x - a.a2 * y;
    x = y;
  }
  return x;
}

// gate: bow on (arco) / pluck on the rising edge (pizzicato)
static inline float stringsTick(StringsEngine &s, bool gate) {
  // ---- gate edges: every player gets a small onset delay in a section
  const StrModelDef &M = s.model;
  const bool solo = (M.ins == INS_SOLO_VIOLIN || M.ins == INS_SOLO_VIOLA || M.ins == INS_SOLO_CELLO);
  const bool trem = M.trem > 0 ? true : (M.trem == 0 ? false : s.tremolo);
  // trill: the upper note shortens the string
  bool trillUp = false;
  if (M.trill) {
    s.trillPh += s.trillInc;
    if (s.trillPh >= 1.0f) s.trillPh -= 1.0f;
    trillUp = s.trillPh >= 0.5f;
  }
  if (s.trillMode && trillUp != s.trillPrev) {                // trill: every finger hit / lift taps
    for (int i = 0; i < STR_VOICES; i++) if (s.v[i].active && s.v[i].env > 0.2f) s.v[i].tran += (trillUp ? 0.018f : 0.009f) * (0.55f + 0.45f * strClamp(s.v[i].vel, 0.0f, 1.0f));
    s.trillPrev = trillUp;
  }
  const float trillRatio = s.trillMode == 3 ? 0.8408964f : (s.trillMode == 2 ? 0.8908987f : (s.trillMode == 1 ? 0.9438743f : 1.0f));
  // ---- tempo: log every gate start (time + velocity); stringsTempo() finds
  //      the pulse, the ostinato pattern and the beat from it
  s.now++;
  if (s.sinceGate < 0xFFFFFFF0u) s.sinceGate++;
  if (gate && !s.gatePrev && s.sinceGate > (uint32_t)(0.03f * STR_SR)) {   // ignore bounces < 30 ms
    if (s.sinceGate > (uint32_t)(3.0f * STR_SR)) s.onN = 0;               // long pause: new rhythm
    // note length for the bowing: the last interval; free playing = normal bow
    s.noteDur = s.sinceGate < (uint32_t)(3.0f * STR_SR) ? (float)s.sinceGate / STR_SR : 0.6f;
    s.sinceGate = 0;
    s.onT[s.onW] = s.now; s.onV[s.onW] = s.velocity;
    s.onW = (uint8_t)((s.onW + 1) % STR_ONSETS);
    if (s.onN < STR_ONSETS) s.onN++;
    s.onCount++;
    if (s.onN == 1) s.beatAnchor = s.now;                     // the rhythm starts here (bar 1)
    // each new note gives the players a slightly staggered bow-stroke phase
    for (int i = 0; i < STR_VOICES; i++)
      s.v[i].tremPh = 0.25f + 0.01f * (float)i;
    strBrainNote(s);                                         // HAESE: how to play this note
  }
  if (!gate && s.gatePrev) { s.brain.offNow = s.now; s.brain.haveOff = true; }
  if (gate && s.sinceGate == (uint32_t)(0.006f * STR_SR)) strBrainVel(s);   // v1.36
  // the dynamic gate may still be rising at the edge: keep the peak of the first 10 ms
  if (s.sinceGate < (uint32_t)(0.01f * STR_SR) && s.onN) {
    const uint8_t last = (uint8_t)((s.onW + STR_ONSETS - 1) % STR_ONSETS);
    if (s.velocity > s.onV[last]) s.onV[last] = s.velocity;
  }
  if (s.sinceGate > (uint32_t)(4.0f * STR_SR)) { s.clockPeriod = 0; s.pulse = 0; s.patLen = 0; }   // clock stopped

  if (gate != s.gatePrev) {
    // v1.34 ensemble: player 1 (the concertmaster) leads, the others follow
    // a few ms later; at a phrase start they breathe together (much tighter)
    // v1.37: in fast passages a section plays tighter (players lock in)
    const float together = ((gate && s.brain.phraseStart) ? 0.4f : 1.0f) * strClamp(s.noteAhead / 0.4f, 0.3f, 1.0f);
    // v1.38 humanize: a player's own timing tendency plus a different
    // amount every note; the same for letting go (early / late releasers)
    for (int i = 0; i < STR_VOICES; i++) {
      StrVoice &v = s.v[i];
      const float j = 0.5f + 0.5f * strRand(s);
      const float r = gate ? 0.6f * v.hTime + 0.4f * j : 0.6f * v.hRel + 0.4f * j;
      v.onset = (solo || i == 0) ? 0
              : (uint32_t)((gate ? 0.004f + 0.030f * r * together : 0.002f + 0.020f * r) * M.tight * STR_SR);
      if (gate && !solo) v.hNoteT = exp2f(0.17f * M.tight * strRand(s));   // this note: +-1 dB
    }
    if (gate && !solo) strHumanizeMean(s, 1);
    s.gatePrev = gate;
  }

  const float bowMax = s.bowBase * s.speedCV;
  // overpressure: PRESSURE knob picks crunch / octave down / twelfth down;
  // the weakly driven periods lose level, give it back
  const int ovrDiv = M.over > 0.0f ? (s.pressure > 0.75f ? 3 : (s.pressure > 0.4f ? 2 : 1)) : 1;
  const float ovrGain = ovrDiv == 3 ? 2.0f : (ovrDiv == 2 ? 1.6f : 1.0f);
  const StrArticDef &A = STR_ARTIC[s.articEff < ART_COUNT ? s.articEff : (uint8_t)ART_LEGATO];
  const uint32_t strokeMax = (uint32_t)(A.stroke * s.strokeMul * STR_SR);
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
      if (!gate) {
        v.relN = 0;
        // short notes (fast tempo) and martele: the bow stops on the string,
        // a short bite and the string is damped; long notes: the bow lifts
        // off with a whisper, the string rings on, slower taper for longer notes
        v.relStop = A.mode == MODE_BOW && !s.brain.phraseEnd && (s.noteAhead < 0.35f || s.articEff == ART_MARTELE);
        const float releaseEnergy = 0.75f + 0.25f * strClamp(v.vel, 0.0f, 1.0f);
        // Ease the bow away; the string and body then keep their own decay.
        float tau = v.relStop ? (0.065f + 0.025f * (1.0f - releaseEnergy))
                              : strClamp(0.16f + 0.10f * s.noteAhead, 0.16f, 0.32f);
        if (s.brain.phraseEnd && !v.relStop) tau *= 1.6f;    // v1.37: the end of a phrase rings out
        if (v.family == FAM_CELLO) tau *= 1.18f;
        else if (v.family == FAM_BASS) tau *= 1.25f;
        v.relK = 1.0f / (tau * STR_SR);
        v.relRamp = 0.0f;                                     // the bow slows down gently, no kink
        if (v.env > 0.1f && A.mode == MODE_BOW) {
          const float releaseDyn = 0.45f + 0.30f * strClamp(v.vel, 0.0f, 1.0f);
          v.tran += (v.relStop ? 0.0025f : 0.0015f) * releaseDyn * (0.90f + 0.10f * strRand(s));
        }
      }
      if (gate) {
        v.fingerOff = false;
        const bool continuing = (A.legato || s.brain.connected) && v.env > 0.05f;   // same bow
        v.ringLoss = v.ringNext;
        if (continuing && !v.fingerOff) strFingerMove(s, v, v.lenNext, v.openNext);   // slide or cross
        else { v.lenTarget = v.lenNext; v.openLen = v.slideOpen = v.openNext; }
        v.peT = s.brain.energy;                                // phrase shaping, also on tied notes
        if (continuing) { v.vibGrow *= 0.7f; v.vibIntent = 0.5f * (v.vibIntent + s.brain.vibIntent); }
        if (continuing) v.tran += 0.006f * (0.55f + 0.45f * strClamp(v.vel, 0.0f, 1.0f)); // quiet finger / bow change
        if (!continuing) {
          v.strokeN = 0;
          v.vel = strClamp(s.velocity, 0.0f, 1.3f);             // dynamic gate: how hard this note
          // v1.34.1: the gate opens while the CV is still rising (a sequencer
          // step, a slewed or enveloped CV): the velocity is the peak of the
          // first 25 ms, not the level at the threshold
          v.velWin = (uint16_t)(STR_VEL_WIN * STR_SR);
          // Natural default: establish stable stick-slip gradually. Cello and
          // bass respond more slowly; velocity and articulation shape the time.
          const float familyTau = (v.family == FAM_CELLO) ? 0.090f
                                : (v.family == FAM_BASS) ? 0.105f : 0.075f;
          const float articulationTau = strClamp(0.90f - 55.0f * A.attack, 0.48f, 1.0f);
          const float dynamicTau = 1.18f - 0.32f * strClamp(v.vel, 0.0f, 1.0f);
          const float attackTau = strClamp(familyTau * articulationTau * dynamicTau * s.brain.attackMul, 0.025f, 0.16f);
          v.vibIntent = s.brain.vibIntent; v.vibGrow = 0.0f; v.relRamp = 1.0f;
          v.vibGrowK = 1.0f / (strClamp(1.1f * s.noteAhead, 0.3f, 1.4f) * STR_SR);   // v1.37: develops over the note
          v.attackK = 1.0f / (attackTau * STR_SR);
          // bow speed from the note length: short notes (fast tempo) fast bow,
          // long notes slow bow (0.6 s = normal)
          // v1.34: from the look-ahead (this note's expected length), so the
          // player divides the bow: a long note starts slower, a short one faster
          v.bowMul = strClamp(sqrtf(0.6f / strClamp(s.noteAhead, 0.05f, 4.0f)), 0.6f, 1.6f)
                   * (solo ? 1.0f : 1.0f + 0.06f * M.tight * strRand(s));   // v1.38: every player bows a bit differently
          // section players start at different points of their bow, so their
          // bow changes on long notes are spread out (player 1 starts fresh)
          v.bowUsed = (solo || i == 0) ? 0.0f : 0.35f * (0.5f + 0.5f * strRand(s)); v.turn = 0;
          // the string alone gives a slow bow more level than a player would:
          // slow ~-2 dB, fast ~+2 dB against a normal bow (measured)
          v.bowGain = v.bowMul < 1.0f ? 1.0f - 0.96f * (1.0f - v.bowMul) : 1.0f;
          v.slMul = powf(v.bowMul, -0.6f);                     // the same Schelleng scaling per note
          v.accent = A.accent * M.accent * s.accentMul * STR_VEL_ACC(v.vel) * s.brain.accent;
          // swell: long notes start soft and grow over ~1/3 of their expected
          // length (at most the model's swell time), short notes do not swell
          v.swell = (M.swell > 0 && s.noteAhead > 0.5f) ? 0.3f : 1.0f;
          v.swK = 1.0f / (strClamp(fminf(M.swell, 0.33f * s.noteAhead), 0.05f, 3.0f) * STR_SR);
          // noise transient: scratch / snap / click, a bit different every time
          const float transientDynamics = (0.28f + 0.42f * strClamp(v.vel, 0.0f, 1.0f))
                                        * (0.78f + 0.22f * s.pressure)
                                        * (0.85f + 0.30f * (0.5f + 0.5f * strRand(s)));
          v.tran = STR_ARTIC_SCRATCH[s.articEff < ART_COUNT ? s.articEff : (uint8_t)ART_LEGATO]
                 * (0.7f + 0.3f * (0.5f + 0.5f * strRand(s))) * transientDynamics;
          // The onset noise now follows note dynamics, with a lower ceiling;
          // bow attacks are especially restrained while pluck character remains.
          if (A.mode == MODE_BOW) v.tran *= 0.025f;
          if ((s.brain.phraseStart || s.brain.downBow) && A.mode == MODE_BOW) v.dir = 1.0f;   // phrase start / heavy beat: down-bow
          else if (A.alternate) v.dir = -v.dir;               // up-bow / down-bow
          // A legato finger change keeps the current string length and slides to the new pitch.
          // Detached/retriggered notes start directly at their target pitch.
          if (!continuing || v.env < 0.05f) v.len = v.lenTarget;
          v.vibAmt = 0;                                       // vibrato starts later:
          // v1.37: the straight tone before the vibrato follows the note length
          v.vibDelay = (uint32_t)(strClamp(0.22f * s.noteAhead, 0.04f, 0.18f) * (1.0f + 0.3f * strRand(s)) * STR_SR);
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
    if (v.velWin) {                                          // catch the velocity peak
      v.velWin--;
      const float vn = strClamp(s.velocity, 0.0f, 1.3f);
      if (vn > v.vel) { v.accent *= STR_VEL_ACC(vn) / STR_VEL_ACC(v.vel); v.vel = vn; }
    }

    // finger travel: speed computed at control rate (stringsControl)
    const float glideK = v.glideK;
    v.len += (v.lenTarget - v.len) * glideK;

    // bow envelope: attack per articulation, release ~60 ms; staccato and
    // spiccato lift the bow after a fixed stroke even if the gate stays high
    bool bowing = v.gate && A.mode == MODE_BOW;
    if (bowing && strokeMax && v.strokeN > strokeMax) bowing = false;
    if (bowing) {
      // Smooth, velocity-aware physical onset. attackK is calculated once
      // at the note edge; this interpolation is inexpensive in the audio ISR.
      v.env += (1.0f - v.env) * v.attackK;
    } else {
      // the bow decelerates smoothly (~20 ms ramp-in) instead of with a kink,
      // which used to break the stick-slip into a small tick
      v.relRamp += (1.0f - v.relRamp) * 0.001f;
      const float strokeRelease = (strokeMax ? 0.0022f : v.relK) * v.relRamp;
      v.env -= v.env * strokeRelease;
    }     // short strokes lift fast
    if (!v.gate) {
      if (v.relN < 0xFFFFFFF0u) v.relN++;
      // after a lifted long note the finger comes off: the string still
      // rings a little, now as the open string, with a tiny tap
      if (!v.fingerOff && !v.relStop && A.mode == MODE_BOW && v.relN == (uint32_t)(0.18f * STR_SR)
          && v.openLen > v.len * 1.03f) {
        v.fingerOff = true;
        v.len = v.lenTarget = v.fingerLen = v.openLen;
        v.tran += 0.0010f * (0.55f + 0.45f * strClamp(v.vel, 0.0f, 1.0f));
      }
    }
    v.accent *= A.accentDecay;
    v.pe += (v.peT - v.pe) * 0.0003f;                        // phrase energy glides (~70 ms)
    if (v.gate && v.vibGrow < 1.0f) v.vibGrow += v.vibGrowK;   // vibrato develops over the note (0.3 ... 1.4 s)
    v.swell += (1.0f - v.swell) * v.swK;

    // vibrato: own oscillator, fades in ~0.3 s after the note starts and
    // grows with the bow speed
    // Track the finger continuously along the string. A threshold-based
    // 'finger position' detector made larger slides feel like discrete fret jumps;
    // the legato onset above already supplies the subtle contact transient.
    v.fingerLen = v.len;
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
    const float vibTarget = (s.vibrato && (v.gate || (v.env > 0.05f && !v.fingerOff)) && A.mode == MODE_BOW && !v.vibDelay)
                          ? v.vibDepth * (0.6f + 0.4f * s.speed) * (0.55f + 0.45f * strClamp(v.vel, 0.0f, 1.25f))
                            * (0.85f + 0.15f * s.pressure) * v.jitD * (0.4f + 0.6f * v.vibWarm)
                            * v.vibIntent * (0.5f + 0.5f * v.vibGrow) * v.atVib : 0.0f;
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
    float loss = (v.env > 0.02f) ? s.loss : v.ringLoss;
    if (!v.gate && v.relStop && v.ringLoss > 0.8f) loss = (v.env > 0.3f) ? s.loss : 0.8f;   // bow resting on the string
    const float bridgeRefl = -v.lp * loss;
    const float nutRefl    = -neckOut;
    const float stringVel  = bridgeRefl + nutRefl;

    float newVel = 0.0f;
    // the bow stays on the string after a stop (it damps), and is lifted off
    // after a lift: then the contact (grip) goes away, not just the speed
    const bool resting = !v.gate && v.relStop && v.relN < (uint32_t)(0.5f * STR_SR);
    if (bowing || v.env > 0.001f || resting) {
      // bow speed: direction, articulation accent; tremolo = fast strokes
      // Up- and down-bow are not identical: a slight speed/pressure bias
      // gives each stroke a different bite without changing its pitch or timing.
      const float bowDirection = (v.dir > 0.0f) ? 1.005f : 0.995f;
      float bv = bowMax * A.speed * v.speedScale * v.env * v.swell * (1.0f + v.accent)
               * v.dir * v.bowMul * bowDirection * v.pe;
      // long notes: when the bow runs out the player changes bow: slows down,
      // turns (scratch) and speeds up again, ~90 ms. Every player has their
      // own bow length, so a section changes bow at different moments
      if (bowing && !trem && !strokeMax) {
        v.bowUsed += v.bowMul * (0.4f + 0.6f * s.speed) * v.env * v.bowLenInv;
        if (v.bowUsed >= 1.0f && !v.turn) { v.turn = (uint32_t)(0.09f * STR_SR); v.bowUsed = 0.0f; }
      }
      if (v.turn) {
        const uint32_t T = (uint32_t)(0.09f * STR_SR), half = T / 2;
        const float x = (float)v.turn / (float)T;                       // 1 -> 0
        bv *= 0.3f + 0.7f * fabsf(cosf(3.14159265f * x));               // slow down, turn, speed up
        if (v.turn == half) { v.dir = -v.dir; v.tran += 0.012f * (0.45f + 0.55f * strClamp(v.vel, 0.0f, 1.0f)); }
        v.turn--;
      }
      if (trem) {
        v.tremPh += v.tremInc;
        if (v.tremPh >= 1.0f) v.tremPh -= 1.0f;
        float tri = 4.0f * (v.tremPh < 0.5f ? v.tremPh : 1.0f - v.tremPh) - 1.0f;
        tri *= 3.0f;
        tri = strClamp(tri, -1.0f, 1.0f);
        const float sg = tri >= 0 ? 1.0f : -1.0f;
        if (sg != v.tremPrev) { v.tran += 0.025f * (0.5f + 0.5f * strClamp(v.vel, 0.0f, 1.0f)); v.tremPrev = sg; }   // bow change scratch
        bv *= tri;
      }
      // rosin noise on the bow
      const float nz = strNoise(v.nrng);
      // friction: stick while the velocity difference is small, slip above.
      // The accent bites in with extra pressure (martele).
      bv *= 1.0f + 0.04f * vibRel * v.vc;                  // the bow arm moves with the hand
      if (M.over > 0.0f) {
        // overpressure (ALF): the heavy bow creaks: it drives one of every
        // 2 or 3 string periods hard and the others weakly, locked to the
        // string, so the motion only repeats every 2 / 3 periods (an octave /
        // an octave + fifth below). PRESSURE knob: < 0.4 crunch only,
        // 0.4 ... 0.75 octave down, above 0.75 twelfth down
        const int div = ovrDiv;
        if (div > 1) {
          v.alfT += 1.0f / L;
          if (v.alfT >= (float)div) v.alfT -= (float)div;
          const float cut = div == 2 ? STR_OVR_FREE : STR_OVR_FREE3;
          bv *= (v.alfT < 1.0f) ? 1.0f + STR_OVR_GRIP * M.over : 1.0f - cut * M.over;
        }
      }
      // the noise comes with the slip: when the string slides under the
      // hair (once per period), not as constant hiss
      const float dv = bv - stringVel + nz * s.bowNoise * (0.35f + 1.6f * v.slip) * (1.0f + 2.5f * roll) * (bv < 0 ? -bv : bv);
      // the same bow force on a slower bow presses relatively harder: keep it
      // in the clean (Helmholtz) range
      const float bowPressure = (v.dir > 0.0f) ? 0.998f : 1.002f;
      float sl = (s.slope * v.slMul * v.atSl + v.pressOffs * 4.0f - 2.5f * v.accent * A.bite
                - 0.6f * (v.vel - 1.0f)) * bowPressure;
      if (M.over > 0.0f) {
        sl -= 0.6f * M.over;                                 // overpressure: heavy bow
        if (sl < 0.15f) sl = 0.15f;
      } else if (sl < STR_SL_MIN) sl = STR_SL_MIN;
      float t = dv * sl + 0.001f;
      t = (t < 0 ? -t : t) + 0.75f;
      t = t * t; t = t * t;                                  // ^4
      float f = 1.0f / t;
      if (f > 1.0f) f = 1.0f;
      newVel = dv * f;
      if (!v.gate && !v.relStop && A.mode == MODE_BOW) newVel *= strClamp(v.env * 4.0f, 0.0f, 1.0f);   // lifting off
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
      const float nz = strNoise(v.nrng);
      tr = nz * v.tran * s.transient;
      v.tran *= (A.mode == MODE_STRIKE) ? 0.990f : 0.9982f;  // wood click shorter
      newVel += tr * 0.025f;
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
    const float tilt = 0.65f * (v.vel - 0.7f) + 0.25f * (v.bowMul - 1.0f);   // fast bow a bit brighter
    const float radiated = bridgeOut + (bridgeOut - v.prevB) * tilt + trHp * 0.018f;
    v.prevB = bridgeOut;
    // dynamics: the string alone follows the bow speed unevenly from note to
    // note; a player also uses force and contact point, so the note level
    // follows the dynamic gate too (1 = no dynamic gate = unchanged)
    const float dyn = v.gain * v.bowGain * ovrGain * STR_VEL_LEV(v.vel) * v.levComp * v.atLev;
    if (i == 0) leadBus = radiated * dyn;                    // force on the bridge
    else        famBus[v.family] += radiated * dyn;
  }

  // ---- sympathetic resonance: the open strings ring along through the bridge
  {
    leadBus += strSymTick(s.sym[0], leadBus);
    float d = 0.0f;
    for (int f = 0; f < STR_FAMILIES; f++) d += famBus[f];
    famBus[s.sym[1].fam] += strSymTick(s.sym[1], d);
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
  float div = 0.0f, divIn = 0.0f;
  for (int f = 0; f < STR_FAMILIES; f++) {
    StrMode *m = s.body[f];
    const float x = famBus[f];
    divIn += x;
    // skip bodies nobody plays on, once they have rung out
    if (!((s.famUsed >> f) & 1)) {
      const float e = m[0].y1 * m[0].y1 + m[1].y1 * m[1].y1 + m[2].y1 * m[2].y1;
      if (e < 1e-12f) continue;
    }
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
  // air: the top octave of the raw strings on top of the bodies
  const float air = s.air * (s.mute ? 0.25f : 1.0f);
  lead += strAirHp(s.airF, 0, leadBus) * air;
  div  += strAirHp(s.airF, 1, divIn) * air;
  lead *= s.level;
  div  *= s.level;

  // divisi-only output (second DAC): DC blocking and fixed output calibration.
  // No limiter or automatic gain control: bow dynamics and transients pass through.
  s.dc2y = div - s.dc2x + 0.9995f * s.dc2y;
  s.dc2x = div;
  s.outDivisi = strLimit(s.lim[1], s.dc2y * STR_OUT_GAIN);

  // full mix (SAMPLE OUT)
  const float out = lead + div;
  s.dcy = out - s.dcx + 0.9995f * s.dcy;
  s.dcx = out;
  return strLimit(s.lim[0], s.dcy * STR_OUT_GAIN);
}

// clear all string and body states (instrument change)
static inline void stringsClear(StringsEngine &s) {
  for (int k = 0; k < 2; k++) for (int j = 0; j < STR_SYM_STRINGS; j++) {
    for (int n = 0; n < STR_SYM_LEN; n++) s.sym[k].buf[j][n] = 0.0f;
    s.sym[k].lp[j] = 0.0f;
  }
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
