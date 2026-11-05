# 同学步进控制片段

本目录保留历史源码片段和 Emm 厂家资料副本。
总入口见[同学资料索引](../reference/classmate-materials.md)。
源码没有合入当前固件，未按 STM32C5 的端口及 HAL 配置移植。

## 文件清单

| 文件 | 已有内容 | 依赖与状态 |
| --- | --- | --- |
| [Clk_MyStep.c](gpio_turn/Clk_MyStep.c)、[头文件](gpio_turn/Clk_MyStep.h) | GPIO 方向/使能、定时翻转脉冲、指定步数和连续转动 | 依赖 `main.h`、`tim.h`、`gpio.h` 和 `htim1`；没有对应完整工程 |
| [motor_X.c](uasrt_/motor_X.c)、[头文件](uasrt_/motor_X.h) | Emm 参数查询、位置、速度、回零及返回结果处理 | 依赖 `huart3`、`bsp_system.h` 和串口回调 |
| [urt.c](uasrt_/urt.c)、[头文件](uasrt_/urt.h) | 电机和视觉串口接收片段 | 文件包含不同阶段代码的拼接，不能作为完整可编译模块直接使用 |
| [ReadMe.txt](uasrt_/ReadMe.txt) | 速度幅值应为非负数的旧使用提示 | 实际接口以方向参数表达正反转 |
| `pwm/` | 当前为空目录 | 没有 PWM 实现文件 |
| `Emm_V5.0闭环步进资料/` | 413 个原始文件 | 与[外层资料](../Emm_V5.0闭环步进资料/README.md)逐文件哈希一致，保留副本 |

## GPIO 脉冲片段的使用边界

`Clk_MyStep.h` 固定使用 PA5 使能、PA7 方向、PA6 脉冲，并把 `htimx` 定义为 `htim1`。
这些是旧示例的分配，不是 C5 小车的既定接线。

`Stepper_SetSpeed()` 实际写入的是脉冲翻转间隔，绝对值越大越慢，并非 RPM 单位。
一个完整脉冲需要两次翻转，因此不能直接把该值当作每步周期。
`Stepper_TickCallback()` 内的计数器和脉冲状态是函数级 `static`，多个电机对象会共享它们。
停止函数没有同时复位这些静态状态及输出电平，重新启动和换向行为需要重新设计或验证。
文件中关于 1us 回调的注释只是使用假设，本目录缺少能证明其实际周期的定时器配置。

源文件引用 `clk_mystep.h`，实际文件名为 `Clk_MyStep.h`。
大小写差异在部分平台会导致包含失败；本轮保留原件。

## 串口片段的使用边界

`uasrt_/urt.c` 重复定义 `serial_Motor_X_Flag`、`Motor_X_RxBuffer` 和 `pMotor_X_RxBuffer`。
后半段还有位于函数外的 `if (huart == &huart3)` 接收处理。
这是代码片段拼接的静态证据，不把它标为可直接构建的串口驱动。

接收逻辑把 `0x6B` 当作帧结束，未完整按功能码和固定长度组帧，也没有完整的数组写入上限检查。
数据字段可能出现同一字节值，因此协议重组需要同时处理帧长、超时、错误恢复和地址。
`motor_X.c` 混用阻塞发送和 DMA 发送，未形成统一的发送队列与忙状态处理。

`26H` ZIP 内也有 `step_motor/motor_X.c`，版本与本目录片段不同。
其中包含 `Now_Angle` 的反馈换算和立即停止命令，不能只因同名就相互覆盖。
完整控制调用关系见[平衡球参考解读](../explanation/classmate-balance-reference.md)。
