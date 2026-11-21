# 赛题与工程文档

| 查找内容 | 入口 |
| --- | --- |
| 赛题、器材与总体方案 | [2026 校赛原题 PDF](2026校赛题目（小车·无人机·嵌入式AI）.pdf) · [器材清单](AC题清单.doc) · [A 题总体方案](explanation/a-system-outline.md) |
| 实车数据 | [三个已接入题目的基线、到达和停稳时间](validation/README.md) · [校准数据](../Firmware/calibration/camera640-and-shaft.json) |
| 主控操作 | [按键与编译](../Firmware/README.md) · [板级联调](how-to/firmware-bringup.md) · [循线停车](how-to/line-following-lap.md) |
| 蓝牙与相机 | [无线烧录](how-to/wireless-update.md) · [OTA 分区与协议](reference/wireless-update.md) · [遥测字段](reference/firmware-telemetry.md) · [相机 UART V2](摄像头与视觉板/MaixCAM2_UART_V2_2026-11-19.md) |
| 外设与接线 | [电机六针口](reference/motor-interface.md) · [外接模块针序](reference/m2-interfaces.md) · [主控 GPIO](reference/motor-core-pins.md) · [灰度校准](how-to/grayscale-check.md) |
| 硬件与机械 | [KiCad 工程](../Hardware/stm32c5a3/README.md) · [采购 BOM](reference/motor-bom.md) · [Motor 检查结果](validation/2026-11-21-handoff.md) · [FreeCAD 总装](../Mechanical/README.md) |
| 继续开发 | [当前交接](../.agent/main/handoff.md) · [项目目录](reference/repository-layout.md) · [固件恢复](../Firmware/releases/README.md) |

当前步进驱动的接线和参数可查 [ZDT X42S 手册](<ZDT_XS系列第二代闭环步进电机资料/3. 说明书/ZDT_X42S第二代闭环步进电机用户手册V1.0.5_260527.pdf>)，控制指令另见 [Emm V5 手册](Emm_V5.0闭环步进资料/说明书/Emm_V5.0步进闭环驱动说明书Rev1.3.pdf)。厂家手册保留在原来的相对路径，KiCad 器件属性也使用这些路径。
