/* 将引导程序放在 Flash 起始的 64 KiB，擦写例程运行在 SRAM。栈顶留出交接邮箱。 */

ENTRY(Reset_Handler)
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K
  RAM (rw) : ORIGIN = 0x20000000, LENGTH = 248K
  STACK (rw) : ORIGIN = 0x2003E000, LENGTH = 8K - 32
}
__StackTop = ORIGIN(STACK) + LENGTH(STACK);
__StackLimit = ORIGIN(STACK);
PHDRS
{
  flash PT_LOAD FLAGS(5);
  ramcode PT_LOAD FLAGS(5);
  ramdata PT_LOAD FLAGS(6);
  stack PT_LOAD FLAGS(6);
}
SECTIONS
{
  .vectors : { . = ALIGN(512); KEEP(*(.vectors)) } > FLASH :flash
  .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
  .ARM.extab : { *(.ARM.extab*) } > FLASH
  .ARM.exidx : {
    __exidx_start = .; *(.ARM.exidx*) __exidx_end = .;
  } > FLASH
  .copy.table (READONLY) : {
    . = ALIGN(4); __copy_table_start__ = .;
    LONG(LOADADDR(.data)); LONG(ADDR(.data)); LONG(SIZEOF(.data) / 4);
    LONG(LOADADDR(.ramfunc)); LONG(ADDR(.ramfunc)); LONG(SIZEOF(.ramfunc) / 4);
    __copy_table_end__ = .;
  } > FLASH
  .zero.table (READONLY) : {
    __zero_table_start__ = .;
    LONG(ADDR(.bss)); LONG(SIZEOF(.bss) / 4);
    __zero_table_end__ = .;
  } > FLASH
  .ramfunc : { . = ALIGN(4); *(.ramfunc*) . = ALIGN(4); } > RAM AT> FLASH :ramcode
  .data : { . = ALIGN(4); *(.data*) . = ALIGN(4); } > RAM AT> FLASH :ramdata
  .bss (NOLOAD) : {
    . = ALIGN(4); *(.bss*) *(COMMON) . = ALIGN(4); end = .; _end = .;
  } > RAM :ramdata
  .stack (NOLOAD) : { . += LENGTH(STACK); } > STACK :stack
  ASSERT(_end <= __StackLimit, "RAM overlaps stack")
  /DISCARD/ : { *(.note*) *(.comment*) }
}
