@ Cortex-M33 的向量表和复位入口，Rust 裸机应用与引导程序共用。

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
    @ 先屏蔽中断并启用浮点单元，再将向量表和 RAM 内容切换到当前镜像。
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
    @ 把必须驻留 SRAM 的例程复制到运行地址。
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
    @ 将有初值的静态变量从 Flash 搬到 RAM。
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
    @ 将零初始化数据段清零，然后进入 Rust 的 firmware_main。
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
    @ 由引导程序传入已验证的应用栈顶和复位地址，切换栈后直接跳转。
    msr msp, r0
    movs r2, #0
    msr control, r2
    isb
    cpsie i
    bx r1
