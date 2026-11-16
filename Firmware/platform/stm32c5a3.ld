ENTRY(Reset_Handler)
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08010000, LENGTH = 448K
  RAM (rw) : ORIGIN = 0x20000000, LENGTH = 248K
  STACK (rw) : ORIGIN = 0x2003E000, LENGTH = 8K - 32
}
__StackTop = ORIGIN(STACK) + LENGTH(STACK);
__StackLimit = ORIGIN(STACK);
SECTIONS
{
  .vectors : { . = ALIGN(512); KEEP(*(.vectors)) } > FLASH
  .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
  .ARM.extab : { *(.ARM.extab*) } > FLASH
  .ARM.exidx : {
    __exidx_start = .; *(.ARM.exidx*) __exidx_end = .;
  } > FLASH
  .copy.table (READONLY) : {
    . = ALIGN(4); __copy_table_start__ = .;
    LONG(LOADADDR(.data)); LONG(ADDR(.data)); LONG(SIZEOF(.data) / 4);
    __copy_table_end__ = .;
  } > FLASH
  .zero.table (READONLY) : {
    __zero_table_start__ = .;
    LONG(ADDR(.bss)); LONG(SIZEOF(.bss) / 4);
    __zero_table_end__ = .;
  } > FLASH
  .data : { . = ALIGN(4); *(.data*) *(.ramfunc*) . = ALIGN(4); } > RAM AT> FLASH
  .bss (NOLOAD) : {
    . = ALIGN(4); *(.bss*) *(COMMON) . = ALIGN(4); end = .; _end = .;
  } > RAM
  .stack (NOLOAD) : { . += LENGTH(STACK); } > STACK
  ASSERT(_end <= __StackLimit, "RAM overlaps stack")
  /DISCARD/ : { *(.note*) *(.comment*) }
}
