#include <stdint.h>
#include <string.h>
extern int main(void);
extern void initialise_monitor_handles(void);
extern void __libc_init_array(void);
extern void exit(int);
extern uint32_t __stack_top, __data_load, __data_start__, __data_end__, __bss_start__, __bss_end__;
void Reset_Handler(void) {
  *(volatile uint32_t *)0xE000ED88 |= (0xFu << 20);       /* FPU on */
  __asm volatile ("dsb\n isb");
  memcpy(&__data_start__, &__data_load, (uint32_t)&__data_end__ - (uint32_t)&__data_start__);
  memset(&__bss_start__, 0, (uint32_t)&__bss_end__ - (uint32_t)&__bss_start__);
  initialise_monitor_handles();
  __libc_init_array();
  exit(main());
}
void Default_Handler(void) { for (;;) {} }
__attribute__((section(".vectors"), used))
void (*const vectors[16])(void) = {
  (void (*)(void))&__stack_top, Reset_Handler,
  Default_Handler, Default_Handler, Default_Handler, Default_Handler, Default_Handler,
  0, 0, 0, 0, Default_Handler, Default_Handler, 0, Default_Handler, Default_Handler };
void _init(void) {} void _fini(void) {}
