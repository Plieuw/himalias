MEMORY { FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 4M
         RAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 4M }
ENTRY(Reset_Handler)
SECTIONS {
  .text : { KEEP(*(.vectors)) *(.text*) *(.rodata*)
            KEEP(*(.init)) KEEP(*(.fini))
            . = ALIGN(4); __preinit_array_start = .; KEEP(*(.preinit_array)) __preinit_array_end = .;
            __init_array_start = .; KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array)) __init_array_end = .;
            __fini_array_start = .; KEEP(*(.fini_array)) __fini_array_end = .; } > FLASH
  .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
  .data : { __data_load = LOADADDR(.data); __data_start__ = .; *(.data*) __data_end__ = .; } > RAM AT > FLASH
  .bss (NOLOAD) : { __bss_start__ = .; *(.bss*) *(COMMON) __bss_end__ = .; } > RAM
  end = .; __end__ = .;
  __stack_top = ORIGIN(RAM) + LENGTH(RAM);
  __HeapLimit = __stack_top - 0x10000;
}
