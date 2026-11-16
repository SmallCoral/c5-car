# 固定厂商依赖

这些目录从本仓库原 `STM32C5A3VGT6_cmake` 工程原样迁入，不通过构建下载依赖。
原始基线为 Git `64c5210`，迁移日期 2026-11-16；本机还有迁移前 ZIP。

| 目录 | 版本依据 | 实际构建用途 |
| --- | --- | --- |
| `stm32c5xx_drivers` | `Release_Notes.md`：2.1.0；README 徽章仍是上游旧版本 | 只包含 `ll/` 内联驱动头文件，不编译 HAL |
| `stm32c5xx_dfp` | `Include/stm32c5xx.h`：2.1.0 | STM32C5A3 寄存器、启动与系统时钟文件 |
| `CMSIS` | `Core/Include/cmsis_version.h`：Core(M) 6.1 | Cortex-M33 中断、内核寄存器与启动支持 |

ST 两个组件的 `LICENSE.md` 保留；CMSIS 的 [Apache-2.0 许可证](CMSIS/LICENSE.md)从原 `arch/cmsis/LICENSE.md` 原样迁入，源文件中的 Arm 版权和许可标识保留。
当前构建使用原包的 `stm32c5xx_dfp/Source/Templates/system_stm32c5xx.c`，其向量表位置本已取自 `__VECTOR_TABLE`，可供引导程序和偏移后的应用共用；该文件与迁移前版本相同。
包内自带 CMake 文件属于旧生成器环境；新入口只使用 `Firmware/CMakeLists.txt` 明确列出的文件。
LCD 初始化寄存器参数和 Emm 帧格式依据仓库内厂家资料，新驱动和 UI 为本项目实现。
