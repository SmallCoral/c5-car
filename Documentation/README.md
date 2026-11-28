# 校赛 A 题文档

| 用途 | 资料 |
| --- | --- |
| 了解赛题和作品 | [校赛原题](2026校赛题目（小车·无人机·嵌入式AI）.pdf) · [器材清单](AC题清单.doc) · [团队设计报告](design-report.docx) · [作品图集](../Images/README.md) |
| 上车操作 | [菜单、按键与语音命令](../Firmware/README.md) · [首次联调](how-to/firmware-bringup.md) · [灰度探头](how-to/grayscale-check.md) · [循线停车](how-to/line-following-lap.md) |
| 烧录与观察球位 | [蓝牙升级步骤](how-to/wireless-update.md) · [OTA 协议](reference/wireless-update.md) · [遥测字段](reference/firmware-telemetry.md) · [球位标定](../Firmware/calibration/camera640-and-shaft.json) |
| 接线与选型 | [外设接口](reference/m2-interfaces.md) · [电机六针口](reference/motor-interface.md) · [GPIO](reference/motor-core-pins.md) · [器件选型](reference/parts-selection.md) · [Motor 采购表](reference/motor-bom.md) |
| 查看电路图纸 | [可编辑的 KiCad 工程](../Hardware/stm32c5a3/README.md) · [Core 图纸](drawings/core-schematic.pdf) · [Motor 图纸](drawings/motor-schematic.pdf) |
| 对照测试结果 | [各版本实测与结项时的验证范围](validation/README.md) |

相机与主控用 UART 交换球位。步进驱动的接线与报文可对照随工程保存的手册。[相机 UART V2](摄像头与视觉板/MaixCAM2_UART_V2_2026-11-19.md) · [ZDT X42S 手册](<ZDT_XS系列第二代闭环步进电机资料/3. 说明书/ZDT_X42S第二代闭环步进电机用户手册V1.0.5_260527.pdf>) · [Emm V5 手册](Emm_V5.0闭环步进资料/说明书/Emm_V5.0步进闭环驱动说明书Rev1.3.pdf)
