// Runs the strings engine on an emulated Cortex-M4F (same ISA and FPU as the
// SAMD51) and counts instructions per audio sample with SysTick in icount mode.
#include <cstdio>
#include <cmath>
#include <cstdint>
#include "../../IOCore/HimaliaSketch/strings_engine.h"
static StringsEngine s;
#define SYST_CSR (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018)
static inline uint32_t ticks() { return SYST_CVR; }
int main() {
  uint32_t fpscr; __asm volatile("vmrs %0, fpscr" : "=r"(fpscr)); fpscr |= (1u << 24); __asm volatile("vmsr fpscr, %0" :: "r"(fpscr));
  SYST_RVR = 0xFFFFFF; SYST_CVR = 0; SYST_CSR = 5;           // CPU clock, no interrupt
  const int models[] = { MDL_SOLO_VIOLIN, MDL_SOLO_CELLO_ROM, MDL_VIOLIN_SECTION, MDL_CELLO_SECTION_FILM, MDL_LOW_STRINGS, MDL_FULL_STRINGS_FILM };
  for (int mi = 0; mi < 6; ++mi) {
    stringsInit(s); s.instrument = models[mi]; s.rootHz = 220.0f; s.artic = ART_AUTO; stringsControl(s);
    uint64_t sumT = 0, sumC = 0; uint32_t maxT = 0, maxC = 0; int nT = 0, nC = 0;
    for (int n = 0; n < 6000; ++n) {
      if ((n & 127) == 0) {
        s.rootHz = 220.0f * (1.0f + 0.25f * ((n >> 10) & 3));
        uint32_t a = ticks(); stringsControl(s); uint32_t d = (a - ticks()) & 0xFFFFFF;
        sumC += d; if (d > maxC) maxC = d; ++nC;
      }
      const bool gate = (n % 2400) < 2000;
      stringsFmInput(s, 0.0f, 0.5f);
      uint32_t a = ticks(); stringsTick(s, gate); uint32_t d = (a - ticks()) & 0xFFFFFF;
      if (n > 500) { sumT += d; if (d > maxT) maxT = d; ++nT; }
    }
    // SysTick runs at the CPU clock; with -icount shift=0 one instruction = one tick of the
    // virtual clock at 1 GHz, the MPS2 CPU clock is 25 MHz -> ticks * 40 = instructions
    printf("model %2d: tick avg %6.0f max %6lu insn | control avg %6.0f max %7lu insn (per 128 samples)\n",
           models[mi], 40.0 * sumT / nT, (unsigned long)(40 * maxT), 40.0 * sumC / nC, (unsigned long)(40 * maxC));
  }
  return 0;
}
