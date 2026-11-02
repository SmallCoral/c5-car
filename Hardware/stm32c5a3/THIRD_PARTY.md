# 代码和资源来源

Coral-C5 把硬件、固件和本地元件资源放在一起，方便换一台电脑继续做。随项目保存的第三方文件仍按各自的版权声明和许可证使用。本项目没有另外为原创内容指定开源许可证。

## 固件

ST 设备支持包、HAL 和系统调用代码保留了原文件头及随附许可。

| 内容 | 本地许可证 |
| --- | --- |
| STM32C5 设备支持 | [stm32c5xx_dfp/LICENSE.md](Firmware/STM32C5A3VGT6_cmake/stm32c5xx_dfp/LICENSE.md) |
| STM32 HAL | [stm32c5xx_drivers/LICENSE.md](Firmware/STM32C5A3VGT6_cmake/stm32c5xx_drivers/LICENSE.md) |
| Arm CMSIS | [arch/cmsis/LICENSE.md](Firmware/STM32C5A3VGT6_cmake/arch/cmsis/LICENSE.md) |
| 系统调用 | [utilities/syscalls/LICENSE.md](Firmware/STM32C5A3VGT6_cmake/utilities/syscalls/LICENSE.md) |

生成目录中还保留了 `mx_stm32c5xx_hal_drivers_license.md` 和 `mx_syscalls_license.md`，它们对应各自的生成文件。

## 元件库和资料

本地库的大部分通用符号、封装与外形模型来自 KiCad 安装库。无线模块的封装和 STEP 来自 [Espressif KiCad 库](https://github.com/espressif/kicad-libraries)。项目把模型路径改成了相对路径，当前使用的封装、符号和模型分别保存在两块板的本地库中，资料入口见 [厂家资料](Hardware/Shared/sources/README.md)。

B1 的 U4、J3、Y3 以及 SW1 至 SW5 的 `GENERIC_` 模型是通用外形替代，具体限制写在 [双板说明](Hardware/Shared/interface.md) 中。它们不能代替厂家的精确结构模型。分立 LED 使用 KiCad 原模型，按用户现有颜色调整了显示材质，几何尺寸未改。

Streamline 公共符号、板框金手指封装、母座封装及 STEP 来自 [CurrentMakers 官方库](https://github.com/currentmakers/streamline_kicad_libraries/tree/27dc9a9413d3e3f1fc1cdf63371ded3b01293c0f)，保留了 [MIT 许可证](Hardware/Shared/Streamline/LICENSE)，版权归 CurrentMakers，2026。项目对资源作了名称和模型路径本地化，移除未设置的丝印变量，接口符号按 C5A3 实际引脚重新绘制。母座封装中的原始 UUID 与几何尺寸保持原样。

新两脚按键封装与通用外形按 [SD-1107 产品页](https://sungmun.cn/qingchukaiguan/qingchu3625liangjiaotiepiananj.html) 中的厂家图纸绘制。图纸原件保存在 Hardware/Shared/sources，版权归原权利人所有。板形摆放参考了 STM32World 公开的 H562 照片和 C551、H562 原理图，来源记录保存在 Hardware/Shared/Streamline。

`Hardware/Shared/sources` 保存了设计时使用的厂家资料，并保留 DS15137 的文本提取结果供检查脚本使用。手册、产品图纸和其中的内容归原权利人所有。关键版本、章节和原始链接列在 [资料依据](Hardware/Shared/sources/README.md) 中。

新增 `Hardware/CoralC5-Motor` 的通用符号、封装和模型基于KiCad 10安装库，母座资源沿用上述Streamline来源。DRV8874封装按TI PWP0016原厂图调整，DC-005和SS12D07VG4封装按各自厂家图建立；这两件的VRML是明确标注的通用外形。厂家资料、用户指定的schematic-design skill快照及限制见 [Motor资料依据](Hardware/CoralC5-Motor/docs/sources/README.md)，原资料版权归原权利人。

## 珊瑚 Logo

Core 丝印中的珊瑚图案沿用 [SmallCoral/Coral-Wave](https://github.com/SmallCoral/Coral-Wave/blob/main/Images/coral-logo.svg) 的 Logo。原始 SVG、随附的 MIT 许可和转换说明保存在 [branding](Hardware/Shared/branding/README.md)，原许可中的版权署名为 Coral Launcher contributors，2025。板上采用单色轮廓，图案下方署名 Designed by SmallCoral。
