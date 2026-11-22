//
//  Himalia firmware -- Strings (bowed-string physical model), beta
//
//  SAMPLE OUT: up to six bowed strings from one pitch CV (strings_engine.h)
//  The controls in playing order (v1.34): WHAT plays, HOW it is played, and
//  the PERFORMANCE that drives it.
//
//  1. WHAT plays
//   - RATCHET knob ................ model (18): the instruments (solo violin,
//                                   solo viola, solo cello, violin / viola / cello
//                                   section, full strings), the playing styles
//                                   (molto vibrato, romantic cello, tremolo, trill),
//                                   the film-score gestures from calm to driving
//                                   (shimmer, emotional swell, heroic theme, epic
//                                   ostinato, suspense), then experimental
//                                   (overpressure, Threnody swarm)
//   - SAMPLE TUNE knob + CV ....... pitch: the knob spans the instrument's real
//                                   range (left = lowest open string), CV 1 V/oct
//   - SPREAD knob + CV ............ layout: unison, octaves, fifths, major, minor,
//                                   divisi major, divisi minor, cluster; models
//                                   without a layout to choose: expression depth
//                                   (left neutral, centre natural, right very
//                                   expressive); Threnody: swarm width; suspense:
//                                   cluster width
//  2. HOW it is played
//   - SQUARES MAIN TUNE knob + CV . articulation, long -> short, then tone colour
//                                   soft -> harsh: AUTO, portamento, legato,
//                                   detache, martele, staccato, spiccato,
//                                   pizzicato, flageolet, sul tasto, sul
//                                   ponticello, col legno. AUTO: the gate length
//                                   decides (tied/long = legato, normal = detache,
//                                   short = staccato, very short = spiccato);
//                                   detache ... spiccato also follow the gate
//                                   length around the chosen stroke. CV: 1 V = one
//                                   step from the knob (a patched CV = exactly that
//                                   articulation). Models with a fixed articulation:
//                                   a gesture (slide time, tremolo speed, trill
//                                   interval, stroke length, accent, swell time)
//   - SAMPLE SELECT knob + CV ..... playing zone: contact point and force together
//                                   (left = fingerboard, soft and dark; centre =
//                                   normal; right = near the bridge, loud and
//                                   bright); CV 1 V = 10 %
//   - CLOCK RATE knob ............. bow speed, log scale ~9 ... 25 ... 70 cm/s
//  3. The PERFORMANCE
//   - TRIGGER in / button ......... gate: bow / pluck / strike. The gate length is
//                                   information too (articulation, bow division)
//   - CV IN RATE jack ............. dynamic gate / bow CV, 0 ... 10 V: the level at
//                                   the note start is how hard the note is played;
//                                   changing it during the note is aftertouch (more
//                                   = more bow weight and vibrato, less = lighter)
//   - A/B button (squares side) ... A = vibrato, B = senza vibrato
//   - BIT switch .................. tremolo
//   - A/B button (sample side) .... con sordino (mute)
//  Himalia reads every phrase as a performance (HAESE): phrase shape, a long
//  arc over 8 phrases, heavy beats on a down-bow, the bow divided over the
//  expected note length, a section that follows its leader.
//
//  Outputs and LEDs
//   - noise side DAC out .......... divisi players only (players 2 ... 6) as strings
//   - square outputs 1 ... 6 ...... every player's note as a square wave while it sounds
//   - PA22 (digital noise out) .... 5 ms trigger when the divisi changes
//   - LED CLK ..................... blinks on the detected beat
//   - diagnose: hold TRIGGER while powering up -> divisi out = CPU load
//                                   (0 ... +5 V = 0 ... 100 %), trigger out high
//                                   while the CPU guard has dropped players
//   - the digital-noise TRIG jack and the analog noise section are hardware
//     only: no microcontroller pin reads them (schematic V1.5)
//
//  Generative divisi: the Turing register of the former squares sequencer
//  (clocked by the trigger) chooses which players of the section play and
//  which chord tone each one takes. SAMPLE OUT carries the full ensemble.
//
//  Other changes against the factory firmware:
//   - linker scripts use the full 1 MB flash of the ATSAMD51J20A
//   - unused libraries removed (Wire, SdFat, Adafruit_SPIFlash)
//   - sample tune jack: correct pin (PB06) is switched to analog mode
//
#define HIMALIA_FW_VERSION "1.34.1-strings-beta"

#include "wiring_private.h"
#include "Adafruit_ZeroTimer.h"

#include "samd51_adc.h"

static void seqStep(uint16_t change_adc, bool scaleB);

#define MAX(a,b) ((a) > (b) ? a : b)
#define MIN(a,b) ((a) < (b) ? a : b)
#define RANGE(min,v,max)	  MIN(MAX(v,min),max) 
#define MAP_RANGE(v,a,b,mi,ma) RANGE(mi,map(v,a,b,mi,ma),ma)

  
// const frequency = Math.pow(2, (m - 69) / 12) * 440;
// incrementer unipolar = 1/SR * f
// incrementer bipolar = 2/SR * f 


// the audio path is built for speed; the Arduino core keeps its size optimisation
#pragma GCC optimize ("O2")
#include "strings_engine.h" // bowed strings physical model (v2)
StringsEngine strings;
volatile bool strings_cv_bow = false;     // CV IN RATE patched and above ~0.25 V
// Diagnose mode (hold the TRIGGER button while powering up): the divisi
// output gives the CPU load as a voltage (0 ... +5 V = 0 ... 100 %), the
// trigger out is high while the CPU guard has dropped players
volatile bool  diag_mode = false;
volatile float diag_load = 0.0f;           // peak load, falls back in ~0.5 s
/*

PA22 RandomOut    PWM Out ?
PA02 SQROut       DAC0
PA05 SampleOut    DAC1

PA19 SQ1 OUT  PA18 FilterCap1
PA12 SQ2 OUT  PA17 FilterCap2
PB15 SQ3 OUT  PA16 FilterCap3
PB14 SQ4 OUT  PA15 FilterCap4
PB13 SQ5 OUT  PA14 FilterCap5
PB12 SQ6 OUT  PA13 FilterCap6

PB08 SQR Tune ADC
PB09 SQR Spread ADC
PB07 Ratchet ADC
PB03 ClockSpeed ADC
PB06 Sample TUNE ADC
PB05 Sample Change ADC 

PA20 Taster A/B 
PA21 Taster LPF 
PB17 Taster BIT 
PB16 Taster Trigger

PB01 TriggerInput SamplePlay
PB02 TriggerInput Clk IN
PB04 Clk Input gesteckt 

PB00 LED CLK 
PB31 LED SampleChange 

PA23 LED SMD Inline (BootLoader)

*/



 // need to Test .... TODO: call this from setup function and uncomment return in Line 164
 void fixBOD33(){
    // A magic number in the unused area of the user page indicates that
    // the device is already updated with the current configuration.
    static const uint32_t magic = 0xa5f12945;
    const uint32_t *page        = (uint32_t *)NVMCTRL_USER;
    if (page[8] == magic)
      return;

    union {
      uint32_t data[128];
      uint8_t bytes[512];
    };

    // void V2Memory::Flash::UserPage::read(uint32_t data[128]) {
    memcpy(data, (const void *)NVMCTRL_USER, 512);

    Serial.println("current FUSES");
    Serial.println( data[0],HEX);
    Serial.println( data[1],HEX);
    Serial.println( data[2],HEX);
    Serial.println( data[3],HEX);
    Serial.println( data[4],HEX);
    Serial.println( data[5],HEX);
    Serial.println( data[6],HEX);
    Serial.println( data[7],HEX);
    Serial.println( data[8],HEX);



    // Ignore all current values; fix the fallout caused by the broken uf2
    // bootloader, which has erased the devices's factory calibration. Try to
    // restore it with known values
    //
    // User Page Dump (Intel Hex) of pristine SAMD51G19A:
    // :0200000400807A
    // :1040000039929AFE80FFECAEFFFFFFFFFFFFFFFF3C
    // :1040100010408000FFFFFFFFFFFFFFFFFFFFFFFFDC
    if (data[4] == 0xffffffff) {
      memset(bytes, 0xff, 512);
      data[0] = 0xfe9a9239; // 0xF69A9239
      data[1] = 0xaeecff80;
      data[4] = 0x00804010;
    }


    // Protect the bootloader area.
    data[0] = (data[0] & ~NVMCTRL_FUSES_BOOTPROT_Msk) | NVMCTRL_FUSES_BOOTPROT(13);

    // Enable the Brown-Out Detector
    data[0] &= ~FUSES_BOD33_DIS_Msk;

    // Set EEPROM size (4 Kb)
    data[1] = (data[1] & ~NVMCTRL_FUSES_SEESBLK_Msk) | NVMCTRL_FUSES_SEESBLK(1);
    data[1] = (data[1] & ~NVMCTRL_FUSES_SEEPSZ_Msk) | NVMCTRL_FUSES_SEEPSZ(3);

    // Add our magic, it will skip this configuration at startup.
    data[8] = magic;

    Serial.println("New FUSES");
    Serial.println( data[0],HEX);
    Serial.println( data[1],HEX);
    Serial.println( data[2],HEX);
    Serial.println( data[3],HEX);
    Serial.println( data[4],HEX);
    Serial.println( data[5],HEX);
    Serial.println( data[6],HEX);
    Serial.println( data[7],HEX);
    Serial.println( data[8],HEX);
   
    
    // uncomment to Write Fuses
    // return;


    // V2Memory::Flash::UserPage::write(data);
    while (NVMCTRL->STATUS.bit.READY == 0)
      ;

    // Manual write
    NVMCTRL->CTRLA.bit.WMODE = NVMCTRL_CTRLA_WMODE_MAN;

    // Erase page
    NVMCTRL->ADDR.reg  = (uint32_t)NVMCTRL_USER;
    NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_EP;
    while (NVMCTRL->STATUS.bit.READY == 0)
      ;

    // Page buffer clear
    NVMCTRL->ADDR.reg  = (uint32_t)NVMCTRL_USER;
    NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_PBC;
    while (NVMCTRL->STATUS.bit.READY == 0)
      ;

    // Write page
    uint32_t *addr = (uint32_t *)NVMCTRL_USER;
    for (uint8_t i = 0; i < 128; i += 4) {
      addr[i + 0] = data[i + 0];
      addr[i + 1] = data[i + 1];
      addr[i + 2] = data[i + 2];
      addr[i + 3] = data[i + 3];

      // Write quad word (128 bits)
      NVMCTRL->ADDR.reg  = (uint32_t)(addr + i);
      NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_WQW;
      while (NVMCTRL->STATUS.bit.READY == 0)
        ;
    }
    // Reboot to enable the new settings.
    delay(100);
    NVIC_SystemReset();

  }


Adafruit_ZeroTimer zt4 = Adafruit_ZeroTimer(4);

void TC4_Handler(){
  Adafruit_ZeroTimer::timerHandler(4);
}

/*
Adafruit_FlashTransport_QSPI flashTransport(PIN_QSPI_SCK, PIN_QSPI_CS, PIN_QSPI_IO0, PIN_QSPI_IO1, PIN_QSPI_IO2, PIN_QSPI_IO3);
Adafruit_SPIFlash flash(&flashTransport);

void dump_sector(uint32_t sector) {
  uint8_t buf[512];
  flash.readBuffer(sector*512, buf, 512);
  for(uint32_t row=0; row<32; row++)  {
    if ( row == 0 ) Serial.print("0");
    if ( row < 16 ) Serial.print("0");
    Serial.print(row*16, HEX);
    Serial.print(" : ");
    for(uint32_t col=0; col<16; col++)    {
      uint8_t val = buf[row*16 + col];
      if ( val < 16 ) Serial.print("0");
      Serial.print(val, HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
}
*/




SAMD51_ADC adc51;
// Audio interrupt: 48MHz GCLK1 / TC_CLOCK_PRESCALER_DIV8 / (PER 125 + 1)
// = 47619 Hz. The 8 bit counter counts 0 ... 125, so one period is 126 ticks.
const float samplerate = 48000000.0 / 8.0 / 126.0;
const float reciprocal_sr = 1.0 / samplerate;





void setup() {
  //put your setup code here, to run once:

  fixBOD33();
  // First try remove DC from Ouputs  
  dacInit();
  DAC->DATA[0].reg = 2048;   // ca 1.5V
  DAC->DATA[1].reg = 2048;
  while (DAC->SYNCBUSY.bit.DATA0);

  Serial.begin(115200);
  // while ( !Serial ) delay(10);   // wait for native usb
  Serial.println("Himalia bowed strings firmware " HIMALIA_FW_VERSION);

  /*
  flash.begin();
  Serial.println("Adafruit Serial Flash Info example");
  Serial.print("JEDEC ID: "); Serial.println(flash.getJEDECID(), HEX);
  Serial.print("Flash size: "); Serial.println(flash.size());
  */

  pinMode(PA13,INPUT); digitalWrite(PA13,false);
  pinMode(PA14,INPUT); digitalWrite(PA14,false);
  pinMode(PA15,INPUT); digitalWrite(PA15,false);
  pinMode(PA16,INPUT); digitalWrite(PA16,false);
  pinMode(PA17,INPUT); digitalWrite(PA17,false);
  pinMode(PA18,INPUT); digitalWrite(PA18,false);


  adc51.createADCMap();

  pinMode(PA23,OUTPUT); // LED
  pinMode(PB00,OUTPUT); // LED
  pinMode(PB31,OUTPUT); // LED

  pinMode(PA19,OUTPUT); // SQR1
  pinMode(PA12,OUTPUT); // SQR2
  pinMode(PB15,OUTPUT); // SQR1
  pinMode(PB14,OUTPUT); // SQR1
  pinMode(PB13,OUTPUT); // SQR1
  pinMode(PB12,OUTPUT); // SQR1

  pinMode(PA22,OUTPUT); // PWM OUT

  pinMode(PB01,INPUT_PULLUP); // SQR1
  // PA20 A/B is read through PORTA input register; no PA20 Arduino alias in this variant.
  pinMode(PA21,INPUT_PULLUP); // squares scale selector
  pinMode(PB17,INPUT_PULLUP); // BIT: bow gate
  pinMode(PB16,INPUT_PULLUP); // Manual Trigger
  pinMode(PB23,INPUT_PULLUP); // Manual Trigger
  delay(20);                                  // let the pull-ups settle
  diag_mode = !(PORT->Group[PORTB].IN.reg & (1ul << 16));   // TRIGGER button held at power-up
  

  // create Time for AudioSamples
  zt4.configure(TC_CLOCK_PRESCALER_DIV8, // prescaler
                TC_COUNTER_SIZE_8BIT,   // bit width of timer/counter
                TC_WAVE_GENERATION_MATCH_PWM  // match style
                );

  zt4.setPeriodMatch(125, 250, 0); // 1 match, channel 0
  zt4.setCallback(true, TC_CALLBACK_CC_CHANNEL0, renderAudio);  // set DAC in the callback

  seqStep(0, false);   // start the squares with a defined pattern
  // flush tiny (denormal) floats to zero: decaying filters and strings can
  // never slow the audio interrupt down
  __set_FPSCR(__get_FPSCR() | (1u << 24));
  stringsInit(strings);
  stringsControl(strings);
  NVIC_SetPriority(TC4_IRQn, 0);             // audio first
  // cycle counter for the CPU load report on the serial monitor
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  // square outputs: square-wave layer of the players; filter caps off
  PORT->Group[PORTA].DIRCLR.reg = (1ul << 13) | (1ul << 14) | (1ul << 15) | (1ul << 16) | (1ul << 17) | (1ul << 18);
  zt4.enable(true);
}

//
//  NoiseGenerator
//
class LFSR {
  private:
  uint16_t reg;
  public:
  LFSR(uint16_t seed) : reg(seed) {}
  uint16_t next() {
    uint8_t b = ((reg >> 0) ^ (reg >> 1) ^ (reg >> 3) ^ (reg >> 12)) & 1;
    reg = (reg >> 1) | (b << 15);
    return reg;
  }
};
LFSR lsfr1(0xA1e);


//
//  AUDIO RENDERER Parms
//
volatile float thea[6]     = { 0.0  , 0.0    , 0.0    , 0.0    , 0.0    , 0.0   };
volatile float thea_inc[6] = { 0.001, 0.00101, 0.00102, 0.00105, 0.00107, 0.00109};
volatile float sq_TRS[6]   = { 0.0  ,0.0     ,0.0     ,0.0     ,0.0     ,0.0};
volatile float flt_TRS[6]   = { 0.0  ,0.0     ,0.0     ,0.0     ,0.0     ,0.0};
volatile float spreads[6]  = { 0.0001f, 0.0001002f, 0.0001003f,0.0001007f,0.0001011f,0.0001017f};

float thea_noise  = 0.0f;
volatile float inc_noise   = 0.001f;

volatile float    wt_fold  = 1.0f;     // fold gain: 1 = clean ... 13 = 6 folds

//
//  External excitation (replaces the digital noise)
//    CV IN RATE jack ... audio input: whatever comes in excites the resonator
//                        (like Rings). Sampled every audio tick on ADC0.
//
#define SYNC_ZERO        2028      // ADC value at 0 V (same input stage as the other CV ins)
#define SYNC_HYST          60      // must go ~0.3 V below 0 V before the next sync
#define SYNC_UNPATCHED   4000      // normalled jack reads above this

volatile uint32_t sync_seen   = 0;         // ticks since the last sync edge (LED)
volatile uint32_t audio_tick = 0;          // counts 48 kHz audio ticks

//
//  Generative sequencer state
//
volatile uint32_t seq_steps_pending = 0;   // set by the audio interrupt on each trigger
// Sequencer pitch CV on the noise side DAC output (DAC0), positive only.
// Output stage (schematic): Vout = DAC volts x 3.397 - 6.05 V
//   -> 0 V at DAC code ~2210, ~365 codes per volt. Fine tune here if needed.
#define SEQ_CV_ZERO_CODE   2210.0f
#define SEQ_CV_CODES_PER_V  365.3f
#define EURO_5V_CODES      (5.0f * SEQ_CV_CODES_PER_V)   // +-5 V audio = +-1826 codes
volatile uint16_t seq_cv_code = 2210;
static float      seq_mel_volts = 0.0f;    // melody above the lowest note (1 V/oct)
volatile uint32_t seq_trig_ticks = 0;      // > 0: trigger out (PA22) is high
#define SEQ_TRIG_TICKS  238                // 5 ms
#define SEQ_CV_MIN_V      ( 0.0f)          // knob fully left: lowest note at 0 V
#define SEQ_CV_MAX_V      ( 5.0f)          // knob fully right: highest note at +5 V
#define SEQ_MEL_RANGE_V   (22.0f / 12.0f)  // highest melody note above the lowest
#define SEQ_F0_HZ         65.406f          // squares pitch at 0 V (C2)
static uint16_t   seq_reg   = 0xB3A5;      // 16 bit loop, the "Turing machine" register
static uint32_t   seq_pos   = 0;           // step counter
static uint32_t   seq_rng   = 0x12345678;  // xorshift random
static int32_t    seq_drone = 0;           // drone degree, changes once per loop

static uint32_t seqRand() {
  seq_rng ^= seq_rng << 13; seq_rng ^= seq_rng >> 17; seq_rng ^= seq_rng << 5;
  return seq_rng;
}

// scale degree -> semitones (any degree, negative ok)
static int32_t seqSemis(int32_t degree, bool scaleB) {
  static const int8_t pent[5]  = { 0, 3, 5, 7, 10 };            // minor pentatonic
  static const int8_t minor[7] = { 0, 2, 3, 5, 7, 8, 10 };      // natural minor
  const int32_t n = scaleB ? 7 : 5;
  int32_t oct = degree >= 0 ? degree / n : -(( -degree + n - 1) / n);
  int32_t idx = degree - oct * n;
  return oct * 12 + (scaleB ? minor[idx] : pent[idx]);
}

static inline float semisToRatio(int32_t semis) {
  return exp2f((float)semis * (1.0f / 12.0f));
}

// one sequencer step: shift the register, maybe flip the bit, set the 6 voices
static void seqStep(uint16_t change_adc, bool scaleB) {
  // Generative divisi: the Turing register changes a little on every trigger
  // (25% chance per step that the bit flips), and decides who plays which tone.
  (void)change_adc; (void)scaleB;
  uint16_t bit = seq_reg >> 15;
  if((seqRand() & 3) == 0) bit ^= 1;
  seq_reg = (uint16_t)((seq_reg << 1) | bit);
  seq_pos++;
  const uint8_t before = strings.divisiMask;
  int8_t tones[STR_VOICES];
  for(int i = 0; i < STR_VOICES; i++) tones[i] = strings.divisiTone[i];
  stringsDivisi(strings, seq_reg);
  bool changed = strings.divisiMask != before;
  for(int i = 0; i < STR_VOICES; i++) if(tones[i] != strings.divisiTone[i]) changed = true;
  if(changed && (strings.layout == LAY_DIVISI_MAJOR || strings.layout == LAY_DIVISI_MINOR))
    seq_trig_ticks = SEQ_TRIG_TICKS;
}

// triangle wavefolder: keeps reflecting the signal back into -1 ... +1
static inline __attribute__((always_inline)) float wtFold(float x) {
  float t = (x + 1.0f) * 0.25f + 4.0f;      // +4: keeps t positive for |x| < 15
  t -= (float)(int32_t)t;
  return (t < 0.5f) ? (4.0f * t - 1.0f) : (3.0f - 4.0f * t);
}

//
//  Selector knob with hysteresis. Same mapping as MAP_RANGE(v,lo,hi,0,maxStep)
//  but it only switches when the knob is clearly inside the next step.
//
static uint16_t selectHyst(int32_t v, int32_t lo, int32_t hi, int32_t maxStep, uint16_t &state) {
  const int32_t H = 4;                                   // 4/16 = 1/4 step hysteresis
  int32_t x16 = (v - lo) * maxStep * 16 / (hi - lo);     // position in 1/16 steps
  int32_t lower = (int32_t)state * 16 - H;
  int32_t upper = (int32_t)state * 16 + 16 + H;
  if(x16 < lower || x16 >= upper){
    int32_t n = x16 / 16;
    if(x16 < 0) n = 0;
    if(n > maxStep) n = maxStep;
    state = (uint16_t)n;
  }
  return state;
}

//
//  AUDIO RENDERER callback for a single sample
//
volatile uint32_t isr_cycles_max = 0;      // worst audio interrupt time (CPU load check)

void renderAudio() {
  const uint32_t isr_c0 = DWT->CYCCNT;
  //  PORT->Group[PORTA].OUTSET.reg = 1ul << 22;

  ++audio_tick;

  // Square outputs: every player also plays its note as a square wave while
  // it sounds (synth layer that follows the divisi)
  {
    static float sq[STR_VOICES];
    bool hi[STR_VOICES];
    for(int i = 0; i < STR_VOICES; i++){
      const StrVoice &v = strings.v[i];
      const bool on = v.active && (v.gate || v.env > 0.05f);
      if(on){
        sq[i] += 2.0f * v.hz * reciprocal_sr;              // phase -1 ... +1
        if(sq[i] > 1.0f) sq[i] -= 2.0f;
        hi[i] = sq[i] > 0.0f;
      } else hi[i] = false;
    }
    if(hi[0]) PORT->Group[PORTA].OUTCLR.reg = 1ul << 19; else PORT->Group[PORTA].OUTSET.reg = 1ul << 19;
    if(hi[1]) PORT->Group[PORTA].OUTCLR.reg = 1ul << 12; else PORT->Group[PORTA].OUTSET.reg = 1ul << 12;
    if(hi[2]) PORT->Group[PORTB].OUTCLR.reg = 1ul << 15; else PORT->Group[PORTB].OUTSET.reg = 1ul << 15;
    if(hi[3]) PORT->Group[PORTB].OUTCLR.reg = 1ul << 14; else PORT->Group[PORTB].OUTSET.reg = 1ul << 14;
    if(hi[4]) PORT->Group[PORTB].OUTCLR.reg = 1ul << 13; else PORT->Group[PORTB].OUTSET.reg = 1ul << 13;
    if(hi[5]) PORT->Group[PORTB].OUTCLR.reg = 1ul << 12; else PORT->Group[PORTB].OUTSET.reg = 1ul << 12;
  }

  // Forward Button + Trigger to LED
  static bool     previous_trigger = false;
  if (  (PORT->Group[PORTB].IN.reg & (1ul << 16))  &&      // Trigger from Button
        (PORT->Group[PORTB].IN.reg & (1ul << 23))     ) {  // Trigger from jack
    PORT->Group[PORTB].OUTCLR.reg = 1ul << 31;             // trigger fire!
    previous_trigger=false;
  } else {
    PORT->Group[PORTB].OUTSET.reg = 1ul << 31;
    if(!previous_trigger){          // check for edge
      seq_steps_pending++;  // clock the squares sequencer
    }
    previous_trigger=true;
  }



  strings.tremolo = !(PORT->Group[PORTB].IN.reg & (1ul << 17));   // BIT switch: tremolo
  const bool gate = previous_trigger || strings_cv_bow;

  {
    // Six-voice bowed-string engine; mono sum goes to SAMPLE OUT.
    float out = stringsTick(strings, gate);

    // LED CLK: blinks on the beat the rhythm analysis found (also with
    // ostinato patterns), so you can see that the module has your tempo.
    // Diagnose mode: on while the CPU guard runs fewer than 6 players.
    {
      static uint32_t beat_cnt = 0, led_ticks = 0;
      static bool gate_prev = false;
      const uint32_t P = strings.clockPeriod;
      if(P){
        // a note near the beat pulls the blink back in phase
        if(gate && !gate_prev && (beat_cnt < P / 6 || beat_cnt > P - P / 6)) beat_cnt = 0;
        if(beat_cnt == 0) led_ticks = (uint32_t)(0.04f * samplerate);
        if(++beat_cnt >= P) beat_cnt = 0;
      } else beat_cnt = 0;
      gate_prev = gate;
      bool led = led_ticks > 0;
      if(led_ticks) led_ticks--;
      if(diag_mode) led = strings.maxVoices < STR_VOICES;
      if(led) PORT->Group[PORTB].OUTSET.reg = 1ul << 0;
      else    PORT->Group[PORTB].OUTCLR.reg = 1ul << 0;
    }



    // Eurorack level: +-1 = +-5 V (10 Vpp), centred on the DAC code that
    // gives 0 V at the jack (output stage from the schematic, see SEQ_CV_*)
    int32_t dl = (int32_t)(SEQ_CV_ZERO_CODE + out * EURO_5V_CODES);
    if(dl < 0) dl = 0;  if(dl > 4095) dl = 4095;
    DAC->DATA[1].reg = (uint16_t)dl;            // SAMPLE OUT
    {
      // diagnose mode: CPU load as a voltage instead of the divisi audio
      const float o2 = diag_mode ? diag_load : strings.outDivisi;
      int32_t d2 = (int32_t)(SEQ_CV_ZERO_CODE + o2 * EURO_5V_CODES);
      if(d2 < 0) d2 = 0;  if(d2 > 4095) d2 = 4095;
      DAC->DATA[0].reg = (uint16_t)d2;          // noise side DAC out: divisi players only
    }

    // trigger out (PA22, former digital noise out): 5 ms when the divisi changes
    if(diag_mode){
      // diagnose mode: high while the CPU guard runs fewer than 6 players
      if(strings.maxVoices < STR_VOICES) PORT->Group[PORTA].OUTSET.reg = 1ul << 22;
      else                               PORT->Group[PORTA].OUTCLR.reg = 1ul << 22;
    } else if(seq_trig_ticks){
      seq_trig_ticks--;
      PORT->Group[PORTA].OUTSET.reg = 1ul << 22;
    } else {
      PORT->Group[PORTA].OUTCLR.reg = 1ul << 22;
    }

  }

  const uint32_t isr_dt = DWT->CYCCNT - isr_c0;
  if(isr_dt > isr_cycles_max) isr_cycles_max = isr_dt;
  // CPU guard: close to the time per sample -> drop a player (min 3);
  // after ~2 s with room to spare, bring one back
  {
    static uint32_t calm = 0;
    const uint32_t budget = (uint32_t)(F_CPU / samplerate);
    const uint32_t load = isr_dt;
    const float l = (float)isr_dt / (float)budget;            // for the diagnose output
    diag_load = l > diag_load ? l : diag_load * 0.99996f;
    if(load > budget * 9 / 10){
      if(strings.maxVoices > 3) strings.maxVoices--;
      calm = 0;
    } else if(load < budget * 7 / 10){
      if(++calm > 95000 && strings.maxVoices < STR_VOICES){ strings.maxVoices++; calm = 0; }
    } else calm = 0;
  }
}





//
//  Bipolar CV inputs. The input stage (buffer + 20k / 6k8 to 3V3 / 10k to GND)
//  maps about -10 V ... +10 V onto the ADC, with 0 V near the middle.
//  The values below come from the resistor values in the schematic; measure
//  your module with the serial monitor (115200 baud, see below) to fine tune.
//
#define CV_ZERO_ADC        2028.0f   // ADC reading at 0 V
#define CV_COUNTS_PER_VOLT  208.9f   // ADC steps per volt
#define CV_UNPATCHED        4090     // reading of an unpatched (normalled) jack

static inline float cvVolts(uint16_t adc) {
  return ((float)adc - CV_ZERO_ADC) * (1.0f / CV_COUNTS_PER_VOLT);
}

void loop() {

  // ---- bow speed: CLOCK RATE knob, CV IN RATE as expression
  uint16_t bow_speed_adc = adc51.readAnalog(PB02,ADC_Channel14,false);
  uint16_t bow_cv_adc    = adc51.readAnalog(PB01,ADC_Channel13,false);
  strings.speed = (float)bow_speed_adc * (1.0f / 4095.0f);
  // CV IN RATE = dynamic gate / bow CV: the level is how hard the bow plays,
  // 0 ... 10 V = soft ... full (linear). Above ~0.4 V it is also a gate: plays
  // notes, steps the divisi and gives the tempo.
  // An empty jack also reads high (normalled), just like a 10 V gate, so the
  // jack only counts as empty after reading high for 6 s without a break
  // since it was last below that (a 10 V gate plays at full strength).
  static bool     cv_rate_patched = false;
  static uint32_t cv_rate_low_ms  = 0;
  const uint32_t now_ms = millis();
  float rate_volts;
  if(bow_cv_adc < SYNC_UNPATCHED){
    cv_rate_patched = true;
    cv_rate_low_ms  = now_ms;
    rate_volts = cvVolts(bow_cv_adc);
  } else {
    if(cv_rate_patched && now_ms - cv_rate_low_ms > 6000) cv_rate_patched = false;
    rate_volts = 10.0f;                                  // at / above the top of the input
  }
  if(cv_rate_patched){
    float c = rate_volts * (1.25f / 9.8f);               // 0 ... 10 V -> 0 ... full (1.25)
    if(c < 0.0f) c = 0.0f;
    if(c > 1.25f) c = 1.25f;
    static float cs = 0.0f;
    cs += (c - cs) * (c > cs ? 0.8f : 0.3f);             // v1.34.1: follow a rising CV at once (velocity)
    strings.speedCV  = cs;
    strings.velocity = cs;                               // sampled per note by the engine
    const bool was = strings_cv_bow;
    strings_cv_bow = strings_cv_bow ? (cs > 0.03f) : (cs > 0.05f);   // with hysteresis
    if(strings_cv_bow && !was) seq_steps_pending++;      // a dynamic gate also steps the divisi
  } else {
    strings.speedCV  = 1.0f;
    strings.velocity = 1.0f;
    strings_cv_bow = false;
  }

  // ---- pitch: SAMPLE TUNE knob + CV (1 V/oct)
  uint16_t sample_pitch_poti = adc51.readAnalog(PA07,ADC_Channel7,false);
  uint16_t sample_pitch_jack = adc51.readAnalog(PB06,ADC_Channel8,true);   // ADC1 AIN8 = PB06
  // the knob spans the real range of the instrument: fully left = its lowest
  // open string (violin G3, viola C3, cello C2, bass E1), fully right = its
  // top note; the CV adds 1 V/oct (0 V = the knob's note)
  float PITCH_LOW_HZ, PITCH_OCTAVES;
  stringsPitchRange(strings.v[0].family, PITCH_LOW_HZ, PITCH_OCTAVES);
  float pitch_oct = (float)sample_pitch_poti * (PITCH_OCTAVES / 4095.0f);
  if(sample_pitch_jack < CV_UNPATCHED)
    pitch_oct += cvVolts(sample_pitch_jack);
  float hz = PITCH_LOW_HZ * exp2f(pitch_oct);
  if(hz < 20.0f)   hz = 20.0f;
  if(hz > 3500.0f) hz = 3500.0f;
  strings.rootHz = hz;

  // ---- articulation: SQUARES MAIN TUNE knob + CV (12 steps, CV can sequence them)
  //      in playing order: AUTO (the gate length decides), portamento, legato,
  //      detache, martele, staccato, spiccato, pizzicato, then the tone colours
  //      soft -> harsh: flageolet, sul tasto, sul ponticello, col legno
  uint16_t art_poti = adc51.readAnalog(PB03,ADC_Channel15,false);
  uint16_t art_jack = adc51.readAnalog(PB04,ADC_Channel6,true);
  // knob picks the articulation, the CV adds 1 V per articulation step (so a
  // sequencer can step through them); 0 V / unpatched = the knob's choice
  static uint16_t art_state = 0;
  int art = (int)selectHyst(art_poti, 150, 3900, ART_KNOB_STEPS - 1, art_state);
  const bool art_cv = art_jack < 4000;                            // normalled jack reads above 4000
  if(art_cv) art += (int)lroundf(cvVolts(art_jack));
  if(art < 0) art = 0;
  if(art > ART_KNOB_STEPS - 1) art = ART_KNOB_STEPS - 1;
  strings.artic = art == 0 ? (uint8_t)ART_AUTO : (uint8_t)(art - 1);
  strings.articLocked = art_cv;                                   // an explicit CV wins over the gate length
  // the same knob + CV as a continuous value (1 V = 10 %): models that fix
  // their articulation use it for a gesture of their own
  {
    float ka = (float)art_poti * (1.0f / 4095.0f);
    if(art_jack < 4000) ka += cvVolts(art_jack) * 0.1f;
    strings.knobArt = ka < 0.0f ? 0.0f : (ka > 1.0f ? 1.0f : ka);
  }

  // ---- bow pressure: SAMPLE SELECT knob + CV
  uint16_t cv_sample_select_adc = adc51.readAnalog(PB09,ADC_Channel3,false);
  uint16_t select_adc = adc51.readAnalog(PA06,ADC_Channel6,false);
  float pr = (float)((int32_t)select_adc - 310) * (1.0f / (3750.0f - 310.0f));
  if(cv_sample_select_adc < 4000) pr += cvVolts(cv_sample_select_adc) * 0.1f;
  if(pr < 0.0f) pr = 0.0f;
  if(pr > 1.0f) pr = 1.0f;
  static float pr_s = 0.5f;
  pr_s += (pr - pr_s) * 0.1f;
  strings.pressure = pr_s;

  // ---- buttons: A/B sample = con sordino (mute), A/B squares = vibrato
  strings.mute    = !(PORT->Group[PORTA].IN.reg & (1ul << 20));
  strings.vibrato =  (PORT->Group[PORTA].IN.reg & (1ul << 21)) != 0;

  // ---- instrument (RATCHET) and layout (SPREAD), with hysteresis
  uint16_t ins_adc = adc51.readAnalog(PB07,ADC_Channel9,true);
  static uint16_t ins_state = 0;
  const uint8_t ins = (uint8_t)selectHyst(ins_adc, 300, 3750, INS_COUNT - 1, ins_state);
  uint16_t lay_adc = adc51.readAnalog(PB05,ADC_Channel7,true);
  static uint16_t lay_state = 0;
  strings.layout = (uint8_t)selectHyst(lay_adc, 250, 3700, LAY_COUNT - 1, lay_state);
  {                                                    // continuous: expression depth / widths
    static float ks = 0.5f;
    const float k = (float)((int32_t)lay_adc - 250) * (1.0f / (3700.0f - 250.0f));
    ks += ((k < 0.0f ? 0.0f : (k > 1.0f ? 1.0f : k)) - ks) * 0.1f;
    strings.knobSpread = ks;
  }
  if(ins != strings.instrument){
    __disable_irq();
    strings.instrument = ins;
    stringsClear(strings);
    __enable_irq();
  }

  // ---- generative divisi: one register step per trigger
  while(seq_steps_pending){
    seq_steps_pending--;
    seqStep(0, false);
  }

  stringsControl(strings);

  // Serial monitor (115200 baud): controls, twice per second
  static uint32_t dbg_ms = 0;
  if(Serial && millis() - dbg_ms > 500){
    dbg_ms = millis();
    Serial.print("hz ");        Serial.print(strings.rootHz, 1);
    Serial.print("  tune_cv "); Serial.print(cvVolts(sample_pitch_jack), 2);
    Serial.print(" V  model "); Serial.print(strings.instrument);
    Serial.print("  layout ");  Serial.print(strings.layout);
    Serial.print("  speed ");   Serial.print(strings.speed, 2);
    Serial.print("  press ");   Serial.print(strings.pressure, 2);
    Serial.print("  artic ");   Serial.print(strings.artic);
    Serial.print("  bpm ");     Serial.print(strings.clockPeriod ? 60.0f * samplerate / (float)strings.clockPeriod : 0.0f, 1);
    Serial.print("  pattern "); Serial.print(strings.patLen); Serial.print(" notes / "); Serial.print(strings.patPulses); Serial.print(" pulses");
    Serial.print("  divisi ");  Serial.print(strings.divisiMask, BIN);
    // worst audio interrupt in the last half second, as % of the time per sample
    const uint32_t mx = isr_cycles_max;
    isr_cycles_max = 0;
    Serial.print("  cpu ");     Serial.print(100.0f * (float)mx / (F_CPU / samplerate), 0);
    Serial.print("%  players max "); Serial.println(strings.maxVoices);
  }
}
