.syntax unified
.cpu cortex-m33
.thumb
.section .vectors,"a",%progbits
.balign 1024
.global __vectors_start
__vectors_start:
.word __stack_top
.word Reset
.word NMI
.word HardFault
.word DefaultHandler, DefaultHandler, DefaultHandler
.word 0, 0, 0, 0
.word DefaultHandler, DefaultHandler, 0, DefaultHandler, SysTick
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word USART1
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word USART6
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.word DefaultHandler
.section .text.Reset,"ax",%progbits
.global Reset
.thumb_func
Reset:
    cpsid i
    ldr r0, =0xe000ed88
    ldr r1, [r0]
    orr r1, r1, #0x00f00000
    str r1, [r0]
    dsb
    isb
    ldr r0, =__vectors_start
    ldr r1, =0xe000ed08
    str r0, [r1]
    ldr r0, =__ram_start
    ldr r1, =__ram_end
    ldr r2, =__ram_load
1:
    cmp r0, r1
    bhs 2f
    ldr r3, [r2], #4
    str r3, [r0], #4
    b 1b
2:
    ldr r0, =__data_start
    ldr r1, =__data_end
    ldr r2, =__data_load
1:
    cmp r0, r1
    bhs 2f
    ldr r3, [r2], #4
    str r3, [r0], #4
    b 1b
2:
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    movs r2, #0
3:
    cmp r0, r1
    bhs 4f
    str r2, [r0], #4
    b 3b
4:
    dsb
    isb
    bl firmware_main
    b DefaultHandler
.section .text.enter_application,"ax",%progbits
.global enter_application
.thumb_func
enter_application:
    msr msp, r0
    movs r2, #0
    msr control, r2
    isb
    cpsie i
    bx r1
