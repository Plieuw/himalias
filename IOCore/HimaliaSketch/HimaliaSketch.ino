//
//  Himalia firmware -- resonator version
//
//  SAMPLE OUT is a physical-modelling resonator (see resonators.h):
//   - RATCHET knob + CV ........... model: string, bar, bell/glass, drum, wind
//   - SAMPLE TUNE knob + CV ....... pitch (knob 7 octaves, CV 1 V/oct, -10 ... +10 V)
//   - SAMPLE SELECT knob + CV ..... timbre (mallet / brightness / bell<->glass / flute<->brass)
//   - TRIGGER in / button ......... strike; wind blows while the gate is held
//   - A/B button (sample side) .... decay short / long
//   - BIT switch .................. bow / blow: excite continuously while the gate is held
//   - CV IN RATE jack ............. external audio that excites the resonator
//   - CLOCK RATE knob ............. wavefolder after the resonator: clean ... 6 folds
//
//  The 6 squares are a generative sequencer (Turing machine style):
//   - clock: SAMPLE TRIGGER in / button, every trigger is one step
//   - SQUARES MAIN TUNE knob + CV ... root pitch, same 1 V/oct scale as the CV out
//   - SPREAD knob + CV .............. change: left = loop locked, right = random
//   - A/B button (squares side) ..... scale: A = minor pentatonic, B = natural minor
//   - noise side DAC out ............ melody pitch CV, 1 V/oct, 0 ... +5 V
//   - PA22 (digital noise out) ...... 5 ms trigger whenever the melody note changes
//
//  Other changes against the factory firmware:
//   - linker scripts use the full 1 MB flash of the ATSAMD51J20A
//   - unused libraries removed (Wire, SdFat, Adafruit_SPIFlash)
//   - sample tune jack: correct pin (PB06) is switched to analog mode
//
#define HIMALIA_FW_VERSION "1.0.0-beta"

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


#include "strings_engine.h" // bowed violin/cello physical-model engine
StringsEngine strings;
volatile bool strings_gate = false;
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
  pinMode(PA21,INPUT_PULLUP); // SQR1
  pinMode(PB17,INPUT_PULLUP); // 8Bit or sample Button
  pinMode(PB16,INPUT_PULLUP); // Manual Trigger
  pinMode(PB23,INPUT_PULLUP); // Manual Trigger
  

  // create Time for AudioSamples
  zt4.configure(TC_CLOCK_PRESCALER_DIV8, // prescaler
                TC_COUNTER_SIZE_8BIT,   // bit width of timer/counter
                TC_WAVE_GENERATION_MATCH_PWM  // match style
                );

  zt4.setPeriodMatch(125, 250, 0); // 1 match, channel 0
  zt4.setCallback(true, TC_CALLBACK_CC_CHANNEL0, renderAudio);  // set DAC in the callback
  zt4.enable(true);

  seqStep(0, false);   // start the squares with a defined pattern
  stringsInit(strings);
  stringsSetInstrument(strings, (PORT->Group[PORTA].IN.reg & (1ul << 20)) ? STR_VIOLIN : STR_CELLO);
  stringsControl(strings, 220.0f, false, STR_UNISON, 0.35f, 0.45f, 0.0018f, 5.2f);
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
  // change probability: 0 (locked) ... 0.5 (completely random)
  float p = (float)((int32_t)change_adc - 250) * (1.0f / (3700.0f - 250.0f));
  if(p < 0.05f) p = 0.0f;                         // small dead zone: really locked
  if(p > 1.0f)  p = 1.0f;
  p *= 0.5f;

  uint16_t bit = seq_reg >> 15;
  if((float)(seqRand() & 0xFFFF) < p * 65536.0f) bit ^= 1;
  seq_reg = (uint16_t)((seq_reg << 1) | bit);
  seq_pos++;

  const int32_t n = scaleB ? 7 : 5;
  if((seq_pos & 15) == 0 && (seqRand() & 3) == 0)          // drone moves sometimes, per loop
    seq_drone = (int32_t)(seqRand() % 3) * (scaleB ? 2 : 1);

  // melody: low 5 bits -> degree over ~2 octaves
  const int32_t mel = (int32_t)(seq_reg & 0x1F) % (2 * n);
  const uint16_t r = seq_reg;

  // voice ratios relative to the MAIN TUNE pitch
  spreads[0] = semisToRatio(seqSemis(mel, scaleB));             // melody

  // melody as 1 V/oct above the lowest note; the pitch knob adds the base (loop)
  const float new_mel = (float)seqSemis(mel, scaleB) * (1.0f / 12.0f);
  // short trigger on PA22 whenever the melody moves to another note
  if(new_mel != seq_mel_volts) seq_trig_ticks = SEQ_TRIG_TICKS;
  seq_mel_volts = new_mel;
  spreads[1] = semisToRatio(seqSemis(mel, scaleB) - 12);        // sub octave
  spreads[2] = semisToRatio(seqSemis(mel + 2, scaleB));         // harmony (third / fourth)
  spreads[3] = semisToRatio(seqSemis(mel + 4, scaleB));         // harmony (fifth)
  spreads[4] = semisToRatio(seqSemis(mel, scaleB) + 12);        // octave up
  spreads[5] = semisToRatio(seqSemis(seq_drone, scaleB) - 12);  // drone

  // which voices sound on this step (melody always; others from register bits)
  const bool on[6] = { true, (r & 0x0100) != 0, (r & 0x0200) != 0, (r & 0x0400) != 0,
                       (r & 0x0800) != 0 && (r & 0x0020) != 0, true };
  // pulse width per voice from the register: 50% ... ~20%
  for(int i = 0; i < 6; i++){
    const float pw = (float)((r >> (i * 2)) & 3) * 0.2f;      // 0, 0.2, 0.4, 0.6
    sq_TRS[i]  = on[i] ? pw : 3.0f;                             // 3 = voice silent
    flt_TRS[i] = ((r >> (i + 6)) & 1) ? -3.0f : 3.0f;           // filter cap per voice on / off
  }
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
void renderAudio() {
  //  PORT->Group[PORTA].OUTSET.reg = 1ul << 22;

  ++audio_tick;

remove external excitation ISR  // SuperSQUARE Ramps generate binary pattern
  bool sqr_pins[6];
  bool flt_pins[6];
  for(int i = 0 ; i < 6 ; i++){
    thea[i]+=thea_inc[i];
    if(thea[i]>1.0f) thea[i]-=2.0f;
    sqr_pins[i] = (thea[i]>sq_TRS[i]); 
    flt_pins[i] = (thea[i]>flt_TRS[i]); 
  }
  // Pin assign signals as fast as possible for generated binary pattern
  if(sqr_pins[0])  PORT->Group[PORTA].OUTCLR.reg = 1ul << 19; else   PORT->Group[PORTA].OUTSET.reg = 1ul << 19;
  if(sqr_pins[1])  PORT->Group[PORTA].OUTCLR.reg = 1ul << 12; else   PORT->Group[PORTA].OUTSET.reg = 1ul << 12;
  if(sqr_pins[2])  PORT->Group[PORTB].OUTCLR.reg = 1ul << 15; else   PORT->Group[PORTB].OUTSET.reg = 1ul << 15;
  if(sqr_pins[3])  PORT->Group[PORTB].OUTCLR.reg = 1ul << 14; else   PORT->Group[PORTB].OUTSET.reg = 1ul << 14;
  if(sqr_pins[4])  PORT->Group[PORTB].OUTCLR.reg = 1ul << 13; else   PORT->Group[PORTB].OUTSET.reg = 1ul << 13;
  if(sqr_pins[5])  PORT->Group[PORTB].OUTCLR.reg = 1ul << 12; else   PORT->Group[PORTB].OUTSET.reg = 1ul << 12;

  if(flt_pins[0])  PORT->Group[PORTA].DIRSET.reg = 1ul << 13; else   PORT->Group[PORTA].DIRCLR.reg = 1ul << 13;
  if(flt_pins[1])  PORT->Group[PORTA].DIRSET.reg = 1ul << 14; else   PORT->Group[PORTA].DIRCLR.reg = 1ul << 14;
  if(flt_pins[2])  PORT->Group[PORTA].DIRSET.reg = 1ul << 15; else   PORT->Group[PORTA].DIRCLR.reg = 1ul << 15;
  if(flt_pins[3])  PORT->Group[PORTA].DIRSET.reg = 1ul << 16; else   PORT->Group[PORTA].DIRCLR.reg = 1ul << 16;
  if(flt_pins[4])  PORT->Group[PORTA].DIRSET.reg = 1ul << 17; else   PORT->Group[PORTA].DIRCLR.reg = 1ul << 17;
  if(flt_pins[5])  PORT->Group[PORTA].DIRSET.reg = 1ul << 18; else   PORT->Group[PORTA].DIRCLR.reg = 1ul << 18;



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



  strings_gate = previous_trigger || !(PORT->Group[PORTB].IN.reg & (1ul << 17)); // trigger gate or BIT bow hold

  {
    // Six-voice bowed-string engine; mono sum goes to SAMPLE OUT.
    float out = stringsTick(strings, strings_gate) * 2.0f;

    int32_t dl = 2048 + (int32_t)(out * 2047.0f);
    if(dl < 0) dl = 0;  if(dl > 4095) dl = 4095;
    DAC->DATA[1].reg = (uint16_t)dl;            // SAMPLE OUT
    DAC->DATA[0].reg = seq_cv_code;             // noise side DAC out: sequencer pitch CV

    // sequencer trigger out (PA22, former digital noise out): 5 ms per new note
    if(seq_trig_ticks){
      seq_trig_ticks--;
      PORT->Group[PORTA].OUTSET.reg = 1ul << 22;
    } else {
      PORT->Group[PORTA].OUTCLR.reg = 1ul << 22;
    }

  }

  // PORT->Group[PORTA].OUTCLR.reg = 1ul << 22;
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

  // CLOCK RATE knob -> bow speed.
  uint16_t bow_speed_adc = adc51.readAnalog(PB02,ADC_Channel14,false);
  float bow_speed = 0.05f + 0.90f * ((float)bow_speed_adc / 4095.0f);

  // -----------------------------------------------------------------
  // SAMPLE Speed
  uint16_t sample_pitch_poti = adc51.readAnalog(PA07,ADC_Channel7,false); 
  uint16_t sample_pitch_jack = adc51.readAnalog(PB06,ADC_Channel8,true);   // ADC1 AIN8 = PB06 (was PA06)  
  // Resonator pitch: knob = 7 octaves from 27.5 Hz (A0),
  // CV = bipolar -10 ... +10 V, 1 V per octave on top of the knob.
  const float RES_PITCH_LOW_HZ  = 27.5f;
  const float RES_PITCH_OCTAVES = 7.0f;
  float pitch_oct = (float)sample_pitch_poti * (RES_PITCH_OCTAVES / 4095.0f);
  if(sample_pitch_jack < CV_UNPATCHED)                  // jack in use
    pitch_oct += cvVolts(sample_pitch_jack);            // 1V/oct
  float res_hz = RES_PITCH_LOW_HZ * exp2f(pitch_oct);
  if(res_hz < 20.0f)   res_hz = 20.0f;
  if(res_hz > 4000.0f) res_hz = 4000.0f;


  // -----------------------------------------------------------------
  // SQR Speed
  uint16_t sqr_pitch_poti = adc51.readAnalog(PB03,ADC_Channel15,false);  // Manual_6XSqr_Pitch Poti
  uint16_t sqr_pitch_jack = adc51.readAnalog(PB04,ADC_Channel6,true);    // CV_6XSqr_Pitch Poti
  uint16_t sqr_pitch_sum = ((sqr_pitch_poti + sqr_pitch_jack)  );
  if(sqr_pitch_jack > 4000)
    sqr_pitch_sum = sqr_pitch_poti;

  if(sqr_pitch_sum>4000) sqr_pitch_sum=4000;


  // Squares pitch and sequencer CV out use the same 1 V/oct scale, so the
  // squares and a VCO on the CV output play the same notes.
  //   knob fully left = lowest note at 0 V = SEQ_F0_HZ
  //   knob fully right = highest note at +5 V (5 octaves above SEQ_F0_HZ)
  const float base = SEQ_CV_MIN_V + (float)sqr_pitch_sum * ((SEQ_CV_MAX_V - SEQ_MEL_RANGE_V - SEQ_CV_MIN_V) / 4000.0f);
  {
    int32_t code = (int32_t)(SEQ_CV_ZERO_CODE + (base + seq_mel_volts) * SEQ_CV_CODES_PER_V + 0.5f);
    if(code < 0) code = 0;
    if(code > 4095) code = 4095;
    seq_cv_code = (uint16_t)code;
  }
  // squares phase runs -1 ... +1, so one cycle = 2.0 of phase
  float inc_sqr = 2.0f * SEQ_F0_HZ * exp2f(base) * reciprocal_sr;
  thea_inc[0]=  spreads[0] * inc_sqr;
  thea_inc[1]=  spreads[1] * inc_sqr;
  thea_inc[2]=  spreads[2] * inc_sqr;
  thea_inc[3]=  spreads[3] * inc_sqr;
  thea_inc[4]=  spreads[4] * inc_sqr;
  thea_inc[5]=  spreads[5] * inc_sqr;

  // -----------------------------------------------------------------
  // Generative sequencer on the 6 squares (Turing machine style)
  //   clock ........ SAMPLE TRIGGER in / button (every rising edge = 1 step)
  //   MAIN TUNE .... root pitch (knob + CV, as before)
  //   SPREAD ....... change: left = loop locked, right = every step new
  //   A/B button ... scale: A = minor pentatonic, B = natural minor (aeolian)
  while(seq_steps_pending){
    seq_steps_pending--;
    uint16_t spread_adc = adc51.readAnalog(PB05,ADC_Channel7,true);
    seqStep(spread_adc, !(PORT->Group[PORTA].IN.reg & (1ul << 21)));
  }

  

  uint16_t cv_sample_select_adc = adc51.readAnalog(PB09,ADC_Channel3,false);


  uint16_t prg8_smpl_select_adc = adc51.readAnalog(PA06,ADC_Channel6,false);

  bool bankB = !(PORT->Group[PORTA].IN.reg & (1ul << 20));   // PA20 button A/B

  // SELECT knob + CV -> timbre 0 ... 1 (lightly smoothed)
  float tb_target = (float)((int32_t)prg8_smpl_select_adc - 310) * (1.0f / (3750.0f - 310.0f));
  if(cv_sample_select_adc < 4000)                       // CV: -10 ... +10 V = whole range
    tb_target += cvVolts(cv_sample_select_adc) * 0.1f;
  if(tb_target < 0.0f) tb_target = 0.0f;
  if(tb_target > 1.0f) tb_target = 1.0f;
  static float tb_smooth = 0.5f;
  tb_smooth += (tb_target - tb_smooth) * 0.1f;
  // SAMPLE SELECT controls bow pressure in the string model.

  // A/B selects the instrument; RATCHET selects solo/ensemble voicing.
  const uint8_t instrument = bankB ? STR_CELLO : STR_VIOLIN;
  uint16_t model_adc = adc51.readAnalog(PB07,ADC_Channel9,true);
  static uint16_t model_state = 0;
  const uint8_t mode = (uint8_t)selectHyst(model_adc, 300, 3750, 6, model_state);
  static uint8_t previous_instrument = 255;
  if(instrument != previous_instrument){
    stringsSetInstrument(strings, instrument);
    __disable_irq();
    stringsClear(strings);
    __enable_irq();
    previous_instrument = instrument;
  }
  // mode 0 = solo; modes 1..6 = unison, octaves, fifths, major, minor, sus4.
  const bool ensemble = mode != 0;
  const uint8_t chord = ensemble ? (uint8_t)(mode - 1) : STR_UNISON;
  stringsControl(strings, res_hz, ensemble, chord,
                 bow_speed, 0.05f + 0.90f * tb_smooth,
                 0.0018f, 5.2f);

  // Serial monitor (115200 baud): raw CV readings, twice per second, for calibration
  static uint32_t dbg_ms = 0;
  if(Serial && millis() - dbg_ms > 500){
    dbg_ms = millis();
    Serial.print("tune_cv ");  Serial.print(sample_pitch_jack);
    Serial.print(" (");        Serial.print(cvVolts(sample_pitch_jack), 2);
    Serial.print(" V)  scan_cv "); Serial.print(cv_sample_select_adc);
    Serial.print(" (");        Serial.print(cvVolts(cv_sample_select_adc), 2);
    Serial.print(" V)  tune_knob "); Serial.print(sample_pitch_poti);
    Serial.print("  scan_knob "); Serial.print(prg8_smpl_select_adc);
    Serial.print("  sqr_tune "); Serial.println(sqr_pitch_sum);
  }


}
