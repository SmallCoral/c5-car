# Coral-C5 固件

这里保留了 STM32C5A3VGT6 的配置文件和生成的 CMake 工程。当前 `main.c` 只调用 `mx_system_init()`，初始化成功后进入空循环。硬件已拆成 B1 的 Core 和 Expansion，配置与应用尚未按这次端口分配重新生成，网口、ESP32-C3 和 microSD 的应用代码也还没有接进去。

Core也可配Motor底板，两种底板分别使用，不能把其外设配置同时套用。
Motor当前引脚和定时器分配见[Motor核心板信号分配](../Documentation/reference/motor-core-pins.md)；该配置尚未写入固件。

## 文件放在哪里

`STM32C5A3VGT6.ioc2` 是主控配置文件。`STM32C5A3VGT6_cmake` 是 CMake 工程，里面包含 ST HAL、设备支持文件、CMSIS、生成的初始化代码和用户代码目录。厂商代码随附的许可证保留在各自目录里。

主控料号、CMake 目标和 preset 名称沿用生成工具的命名。Coral-C5 是整个开发板项目的名称。

## 构建入口

生成的工程要求 CMake 至少 3.30，使用 Ninja。现有 preset 对应 GCC 14.3.1，`arm-none-eabi-gcc`、`arm-none-eabi-g++` 等工具需要能从 PATH 找到。

在本目录执行下面的命令。

```powershell
cd STM32C5A3VGT6_cmake
cmake --preset debug_GCC_STM32C5A3VGT6
cmake --build --preset debug_GCC_STM32C5A3VGT6
```

输出目录由 preset 指定为 `build/debug_GCC_STM32C5A3VGT6`。这次整理仓库没有编译或烧录固件，构建结果还需要在装好工具链的环境里确认。

## 网络Expansion底板的外设

B1 无线 AT 串口改用 USART3 的 PD8 发、PD9 收，PD3 控制 EN，PD4 控制 BOOT。SD 卡改走 SPI3，PC10 为 SCK，PC11 为 MISO，PC12 为 MOSI，PA15 为片选，PH4 为卡检测。旧单板的 SPI1 和 PB10／PC4 串口分配不再适用。

网口使用 RMII，PHY 地址为 0，独立振荡器提供 50MHz。REF_CLK、MDIO、MDC 为 PA1、PA2、PC1，CRS_DV、RXD0、RXD1 为 PA7、PC4、PC5，TXD0、TX_EN、TXD1 为 PB12、PB13、PB15。PC4 的 RXD0 使用 AF12，PC5 的 RXD1 使用 AF13，其余以资料表核对结果为准。PHY 复位为 PE0，中断为 PE1。

PHY 中断 PE1、SD 检测 PH4 和用户按键 PD2 分别占用 EXTI1、EXTI4、EXTI2。用户红灯使用 PE3，高电平点亮，黄绿色 D3 改为直接显示电源状态。其余引脚和时钟限制见 [双板接口说明](../Hardware/stm32c5a3/Hardware/Shared/interface.md)。这些连接尚未在应用固件中逐项实现，本次没有编译或烧录。

如果重新生成工程，先保存自己加的代码，再检查生成结果有没有覆盖它。需要提交的是配置和源码，`build`、调试缓存及编译产物由根目录的 `.gitignore` 排除。
