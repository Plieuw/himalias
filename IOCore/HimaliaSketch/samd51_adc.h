
#ifndef SAMD51_ADC_H
#define SAMD51_ADC_H

#include "wiring_private.h" // pinPeripheral() function

//
//  SAMD ADC Registers
//
// on Himalia it produce with ADC_REFCTRL_REFSEL_INTVCC1_Val
// 2040 .... 3080 with potis
// 2000 .... 4095 with external CV 
class SAMD51_ADC {
  private:
  uint16_t reg;
  public:

  float adcToInc[4096]; // public array for lookups

  SAMD51_ADC() {
    // analogReference(AR_EXTERNAL);
    // analogReadResolution(12);
    ADC0->REFCTRL.bit.REFSEL = ADC_REFCTRL_REFSEL_INTVCC1_Val;  // // 1/1.48 VDDANA = 1/1.48* 3V3 = 2.2297
    while(ADC0->SYNCBUSY.reg & ADC_SYNCBUSY_CTRLB);
    
    ADC0->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_12BIT_Val;
    while(ADC0->SYNCBUSY.reg & ADC_SYNCBUSY_CTRLB);

    // add clk stuff from https://github.com/noscene/ArduinoCore-samd/blob/master/cores/arduino/wiring.c
    GCLK->PCHCTRL[ADC1_GCLK_ID].reg = GCLK_PCHCTRL_GEN_GCLK1_Val | (1 << GCLK_PCHCTRL_CHEN_Pos); //use clock generator 1 (48Mhz)
    ADC1->CTRLA.bit.PRESCALER = ADC_CTRLA_PRESCALER_DIV32_Val;
    ADC1->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_12BIT_Val;
    
    while( ADC1->SYNCBUSY.reg & ADC_SYNCBUSY_CTRLB );  //wait for sync
    ADC1->SAMPCTRL.reg = 5;                        // sampling Time Length
    while( ADC1->SYNCBUSY.reg & ADC_SYNCBUSY_SAMPCTRL );  //wait for sync
    ADC1->INPUTCTRL.reg = ADC_INPUTCTRL_MUXNEG_GND;   // No Negative input (Internal Ground)
    while( ADC1->SYNCBUSY.reg & ADC_SYNCBUSY_INPUTCTRL );  //wait for sync
    // Averaging (see datasheet table in AVGCTRL register description)
    ADC1->AVGCTRL.reg = ADC_AVGCTRL_SAMPLENUM_1 |    // 1 sample only (no oversampling nor averaging)
                ADC_AVGCTRL_ADJRES(0x0ul);   // Adjusting result by 0
    while( ADC1->SYNCBUSY.reg & ADC_SYNCBUSY_AVGCTRL );  //wait for sync

    
    ADC1->REFCTRL.bit.REFSEL = ADC_REFCTRL_REFSEL_INTVCC1_Val;
    while(ADC1->SYNCBUSY.reg & ADC_SYNCBUSY_CTRLB);

    ADC1->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_12BIT_Val;
    // while(ADC1->SYNCBUSY.reg & ADC_SYNCBUSY_CTRLB);     // HÄNGT!!!!!
  };


  uint16_t readAnalog(uint16_t pin, uint16_t channel , bool adc_alt){
    pinPeripheral(pin, PIO_ANALOG);
    if(adc_alt){
      // (only used before beginScan(); afterwards the interrupt owns ADC1)
      while( ADC1->SYNCBUSY.reg & ADC_SYNCBUSY_INPUTCTRL ); //wait for sync
      ADC1->INPUTCTRL.bit.MUXPOS = channel; // Selection for the positive ADC input
      while( ADC1->SYNCBUSY.reg & ADC_SYNCBUSY_ENABLE ); //wait for sync
      ADC1->CTRLA.bit.ENABLE = 0x01;             // Enable ADC    
      while( ADC1->SYNCBUSY.reg & ADC_SYNCBUSY_ENABLE ); //wait for sync
      ADC1->SWTRIG.bit.START = 1;
      while (ADC1->INTFLAG.bit.RESRDY == 0);   // Waiting for conversion to complete
      return ADC1->RESULT.reg;
    }else{
      // ADC0 is shared with the audio interrupt, which samples the sync input
      // (syncChannel) every tick. Block the interrupt for this one conversion,
      // then hand ADC0 back with a fresh sync conversion running.
      __disable_irq();
      if(syncRunning){
        uint32_t timeout = 2000;
        while(ADC0->INTFLAG.bit.RESRDY == 0 && --timeout);   // let the running conversion finish
        (void)ADC0->RESULT.reg;
      }
      while( ADC0->SYNCBUSY.reg & ADC_SYNCBUSY_INPUTCTRL ); //wait for sync
      ADC0->INPUTCTRL.bit.MUXPOS = channel; // Selection for the positive ADC input
      while( ADC0->SYNCBUSY.reg & ADC_SYNCBUSY_ENABLE ); //wait for sync
      ADC0->CTRLA.bit.ENABLE = 0x01;             // Enable ADC    
      while( ADC0->SYNCBUSY.reg & ADC_SYNCBUSY_ENABLE ); //wait for sync
      ADC0->SWTRIG.bit.START = 1;
      while (ADC0->INTFLAG.bit.RESRDY == 0);   // Waiting for conversion to complete
      uint16_t r = ADC0->RESULT.reg;
      if(syncRunning) startSyncConversion();
      __enable_irq();
      return r;
    }
  };

  // --- sync input sampled by the audio interrupt on ADC0 ---
  volatile bool syncRunning = false;
  uint16_t      syncChannel = 13;

  void startSyncConversion(){
    while( ADC0->SYNCBUSY.reg & ADC_SYNCBUSY_INPUTCTRL );
    ADC0->INPUTCTRL.bit.MUXPOS = syncChannel;
    while( ADC0->SYNCBUSY.reg & ADC_SYNCBUSY_INPUTCTRL );
    ADC0->SWTRIG.bit.START = 1;
  };

  void beginSyncInput(uint16_t pin, uint16_t channel){
    pinPeripheral(pin, PIO_ANALOG);
    syncChannel = channel;
    ADC0->CTRLA.bit.ENABLE = 0x01;
    while( ADC0->SYNCBUSY.reg & ADC_SYNCBUSY_ENABLE );
    syncRunning = true;
    startSyncConversion();
  };

  // --- v1.40: the audio interrupt owns both ADCs (no blocking reads) ---
  // Every tick it picks up the conversion started the tick before and
  // starts the next one. Even ticks: the audio-rate channel (ADC0 VELOCITY,
  // ADC1 1V/OCT, 23.8 kHz each); odd ticks: the other channels in turn
  // (ADC0: 5 channels at ~4.8 kHz, ADC1: 3 channels at ~7.9 kHz). loop()
  // only reads the cached values, so it never stalls the audio.
  static const uint8_t N0 = 5, N1 = 3;
  uint8_t  ch0[N0 + 1], ch1[N1 + 1];          // [0] = audio-rate channel
  volatile uint16_t val0[N0 + 1], val1[N1 + 1];
  uint8_t  cur0 = 0, cur1 = 0, rot0 = 1, rot1 = 1;
  bool     scanRunning = false, tick = false;
  volatile uint16_t fast0 = 0, fast1 = 2028;  // audio-rate values (VELOCITY, 1V/OCT)

  static void adcEnable(Adc *adc){
    adc->CTRLA.bit.ENABLE = 0x01;
    while( adc->SYNCBUSY.reg & ADC_SYNCBUSY_ENABLE );
  }
  static void adcStart(Adc *adc, uint8_t channel){
    while( adc->SYNCBUSY.reg & ADC_SYNCBUSY_INPUTCTRL );
    adc->INPUTCTRL.bit.MUXPOS = channel;
    while( adc->SYNCBUSY.reg & ADC_SYNCBUSY_INPUTCTRL );
    adc->SWTRIG.bit.START = 1;
  }

  // pins[0] / chans[0] are the audio-rate inputs
  void beginScan(const uint16_t *pins0, const uint8_t *chans0, const uint16_t *pins1, const uint8_t *chans1){
    for(int i = 0; i <= N0; i++){ pinPeripheral(pins0[i], PIO_ANALOG); ch0[i] = chans0[i]; val0[i] = 0; }
    for(int i = 0; i <= N1; i++){ pinPeripheral(pins1[i], PIO_ANALOG); ch1[i] = chans1[i]; val1[i] = 0; }
    adcEnable(ADC0); adcEnable(ADC1);
    // fill the cache once with blocking reads, then hand over to the interrupt
    for(int i = 0; i <= N0; i++) val0[i] = readAnalog(pins0[i], ch0[i], false);
    for(int i = 0; i <= N1; i++) val1[i] = readAnalog(pins1[i], ch1[i], true);
    fast0 = val0[0]; fast1 = val1[0];
    cur0 = 0; cur1 = 0; tick = false;
    adcStart(ADC0, ch0[0]); adcStart(ADC1, ch1[0]);
    scanRunning = true;
  }

  // first thing in the audio interrupt
  inline void isrPoll(){
    if(!scanRunning) return;
    if(ADC0->INTFLAG.bit.RESRDY){ const uint16_t r = ADC0->RESULT.reg; val0[cur0] = r; if(cur0 == 0) fast0 = r; }
    if(ADC1->INTFLAG.bit.RESRDY){ const uint16_t r = ADC1->RESULT.reg; val1[cur1] = r; if(cur1 == 0) fast1 = r; }
    tick = !tick;
    if(tick){                                   // odd tick: the next slow channel
      cur0 = rot0; rot0 = rot0 >= N0 ? 1 : rot0 + 1;
      cur1 = rot1; rot1 = rot1 >= N1 ? 1 : rot1 + 1;
    } else { cur0 = 0; cur1 = 0; }              // even tick: the audio-rate channels
    adcStart(ADC0, ch0[cur0]);
    adcStart(ADC1, ch1[cur1]);
  }

  // see how to compute: https://electronics.stackexchange.com/questions/278050/making-voltmeter-accepting-bipolar-input-voltage-using-a-microcontroller
  float voltageDivider(float vIn){
    const float r1 =  22000.0f;  // 20k resitor  input resistor
    const float r2 = 150000.0f;  // 6k8 resitor  to vref
    const float r3 =  10000.0f;  // 10k resitor  to gnd
    const float vref = 3.3f;    // input ref from array
    const float dby = 1.0f / ( 1.0f / r1 + 1.0f / r2 + 1.0f / r3 );
    const float utx = vref / r2;
    return ( vIn / r1 + utx) *  dby;
  };

  // scale 12 bit adc hardware
  uint16_t ADCValueByVolt(float v){
    const float adc_ref = 3.3f;
    const float bitdeep = 4096.0f;
    return  (v / adc_ref * bitdeep);
  };

  void createADCMap() {
    const float samplingrate = 96000.0f;
    const float volt_per_octave = 1.0f;     // mode for with range
    for(float vin = -15.0f ; vin < 15.0f ; vin+=0.001f){      // lets brute force, but fast enough
      uint16_t adc_v = ADCValueByVolt(voltageDivider(vin));   // 
      if(adc_v < 4096){
        // A4 = 440Hz = 2.75V as a reference point,
        float frq =  110.0f / pow(2.0f, 2.75) * pow(2.0f, vin * volt_per_octave );  // <--- tuning stuff
        float thea_inc = 2.0f / samplingrate * frq;
        if(thea_inc>1.0f) thea_inc=1.0f;            // Limit nyquist
        adcToInc[adc_v]=thea_inc;                   // compute table ADC value to phase increment for ramp osc
        /** /
        Serial.print(vin,DEC);
        Serial.print(" -> ");
        Serial.print(adc_v);
        Serial.print(" -> ");
        Serial.print(thea_inc);
        Serial.print(" -> ");
        Serial.println(frq);
        /**/
      }
    }
  };


};


#endif