#!/bin/sh
# Runs the strings engine on an emulated Cortex-M4F (QEMU mps2-an386: the same
# ISA and FPU as the SAMD51) with the firmware's compiler flags (-Os) and
# prints instructions per audio sample. Budget on the module: ~2520 cycles
# per sample (120 MHz / 47.6 kHz); one instruction is at least one cycle.
# Needs: arm-none-eabi-gcc, qemu-system-arm.
set -e
cd "$(dirname "$0")"
FL="-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Os -ffunction-sections -fdata-sections"
arm-none-eabi-gcc $FL -c startup.c -o /tmp/m4_startup.o
arm-none-eabi-g++ $FL -fno-exceptions -fno-rtti -std=gnu++11 -c bench.cpp -o /tmp/m4_bench.o
arm-none-eabi-g++ $FL -T mps2.ld -nostartfiles --specs=nano.specs --specs=rdimon.specs -Wl,--gc-sections \
  /tmp/m4_startup.o /tmp/m4_bench.o -o /tmp/m4_bench.elf -lm -u _printf_float
qemu-system-arm -M mps2-an386 -nographic -semihosting -icount shift=0 -kernel /tmp/m4_bench.elf
