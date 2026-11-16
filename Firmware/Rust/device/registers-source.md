# C5A3 寄存器与运行时边界

## 来源

硬件基准为 STM32C5A3VGT6，沿用原 `Firmware/src/board.c`、`serial.c`、`display.c` 和 `boot/flash_port.c` 的接线、时序与分区。迁移基线为仓库提交 `b9aaa695109ca046874f67017f2174d682bed09d`，核对日期 2026-11-16。

`src/registers.rs` 的 126 个常量由以下现有 ST 头文件中的宏或 `offsetof` 求值后核对；每行保留原始表达式。它们是冻结的芯片常量，正常 Cargo 构建不调用 C 编译器。

| 来源（相对原 Firmware 目录） | SHA-256 |
| --- | --- |
| `vendor/stm32c5xx_dfp/Include/stm32c5a3xx.h`（DFP 2.1.0） | `d9599e741c18bd81e80e6967b4fab2b80f350918f0c279ca478260fe104c952b` |
| `vendor/stm32c5xx_drivers/ll/stm32c5xx_ll_tim.h` | `295cd812f6e9c12089fe36ddba8db44ef54b41170d31516d4fa4dd7414cbf3ed` |
| `vendor/stm32c5xx_drivers/ll/stm32c5xx_ll_flash.h` | `e9a29dc328c2db785a1328a8ddcd346be057c4def2ff6963d9dd0ddbad007112` |

还使用同一 `ll` 目录的 bus、SPI 头文件计算组合掩码。不得把 STM32F4 等其他系列的 RCC/Flash 偏移套用到 C5A3。

`LL_TIM_IC_FILTER_FDIV1_N8` 是 LL API 的编码值 `0x00300000`，需要右移 `LL_TIM_IC_CONFIG_POS=16` 得到 CCMR1 的 IC1F 位。两通道直接输入、N8 滤波配置最终为 `0x3131`。PWM1 的 `0x60` 则已经位于 OC1M 位置。

## 接线与时钟

| 功能 | 配置 |
| --- | --- |
| 系统时钟 | 检查 HSI/3 就绪与系统源，48 MHz，AHB/APB 分频 1 |
| 左/右 PWM | PC2 TIM4_CH4 / PB6 TIM4_CH1，20 kHz |
| 左/右方向、共用使能 | PC3 / PC11、PA3 |
| 左/右 nFAULT | PH4 / PC10，上拉 |
| 左编码器 | TIM5，PA0 / PA1，AF2 |
| 右编码器 | TIM2，PB3 / PA15，AF1 |
| 按键（左至右） | PH3 RUN、PA10 DIR、PA8 LEVEL、PD15 TASK |
| 灰度 | PE7..PE14，原始低有效位图 |
| 蓝牙 | USART1 PA6 / PA7，AF7，57600，IRQ 51 |
| 步进 | USART6 PC6 / PC7，AF7，115200，IRQ 96 |
| 屏幕 | SPI3 PB1 SCK AF4、PB2 MOSI AF7；PC4 CS、PC5 DC、PB0 RES、PA4 BL |

只有 SysTick 和串口中断启用；SysTick 优先级 0、UART 优先级 2。屏幕按 8 行分片发送，完整帧最多每 200 ms 生成一次。

## 内存与 unsafe

- `core` 使用 `#![forbid(unsafe_code)]`，可直接运行主机测试。MCU 无堆分配、无 RTOS。
- `board` 的 MMIO、`runtime` 的中断/共享状态、`display` 的单例帧缓冲、`flash` 的 Flash 访问为设备侧边界。
- `Shared<T>` 在单核 Cortex-M 上屏蔽中断并检查递归借用，离开闭包恢复原 PRIMASK；NMI 不借用这些单例。帧缓冲只由主循环持有。
- 启动汇编设置 FPU、VTOR，复制 `.data`/`.ramfunc`，清零 `.bss`；144 项向量表覆盖本器件 IRQ 0..98。未使用中断进入停机处理。
- 栈顶 `0x2003FFE0`，保留顶部 32 字节升级请求区；链接器另为栈预留 8160 字节。动态栈峰值尚未上板测量。
- 引导区 `0x08000000` / 64 KiB，应用 `0x08010000` / 448 KiB，暂存 `0x08080000` / 448 KiB，元数据 `0x080FC000`、`0x080FE000`，8 KiB 页。
- 写入口只允许应用、暂存和各 32 字节元数据，拒绝引导区、保留间隙、RAM、越界和非 16 字节对齐。页擦除仅允许对应合法页首。
- Flash 擦写及超时复位全部在 200 字节 `.ramfunc` 中完成，含文字常量池；期间屏蔽 IRQ，无 Flash 函数调用。逐四字编程后回读验证，拒绝 bank swap、ICACHE 开启和忙状态。
- NMI 记录并清除 Flash ECC 双位错误，使镜像读校验失败；实际 C5A3 ECC 中断与掉电行为尚未验证。

## 参考

[Rust ARMv8-M 目标文档](https://doc.rust-lang.org/rustc/platform-support/thumbv8m.main-none-eabi.html)说明 `thumbv8m.main-none-eabihf` 对 Cortex-M33 的支持。上面的外设参数以仓库内 ST 文件和既有接线为依据；Rust ARM 目标支持不等于本板所有外设已经通过实测。
