ENTRY(Reset)
MEMORY {
  FLASH (rx) : ORIGIN = @ORIGIN@, LENGTH = @LENGTH@
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 248K
}
__stack_top = 0x2003ffe0;
SECTIONS {
  .vectors : ALIGN(1024) { KEEP(*(.vectors)); } > FLASH
  .text : ALIGN(4) { *(.text .text.*); *(.rodata .rodata.*); . = ALIGN(4); } > FLASH
  .ramfunc : ALIGN(4) {
    __ram_start = .; *(.ramfunc .ramfunc.*); . = ALIGN(4); __ram_end = .;
  } > RAM AT> FLASH
  __ram_load = LOADADDR(.ramfunc);
  .data : ALIGN(4) {
    __data_start = .; *(.data .data.*); . = ALIGN(4); __data_end = .;
  } > RAM AT> FLASH
  __data_load = LOADADDR(.data);
  .bss (NOLOAD) : ALIGN(4) {
    __bss_start = .; *(.bss .bss.*); *(COMMON); . = ALIGN(4); __bss_end = .;
  } > RAM
  /DISCARD/ : { *(.ARM.exidx .ARM.exidx.*); *(.ARM.extab .ARM.extab.*); *(.eh_frame*); }
  ASSERT(__bss_end <= 0x2003e000, "RAM overlaps reserved 8160-byte stack")
  ASSERT(SIZEOF(.vectors) >= 512, "incomplete vector table")
}
