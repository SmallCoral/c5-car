# 作品图集

从整车到电路板，再到比赛时的屏幕和赛道画面。点击下方缩略图可以打开大图。报告中的系统图与测试曲线保留原编号对应的内容。[团队设计报告](../Documentation/design-report.docx)

## 整车与装配

| 整车正面 | 整车背面 | 底盘与驱动轮 |
| --- | --- | --- |
| [![整车正面](vehicle/vehicle-front.jpg)](vehicle/vehicle-front.jpg) | [![整车背面](vehicle/vehicle-rear.jpg)](vehicle/vehicle-rear.jpg) | [![底盘局部](vehicle/chassis.jpg)](vehicle/chassis.jpg) |

## 自制核心板与扩展板

| 双板插接实物 | Motor 扩展板正面 |
| --- | --- |
| [![双板插接](hardware/board-assembly.jpg)](hardware/board-assembly.jpg) | [![Motor 扩展板](hardware/motor-board.jpg)](hardware/motor-board.jpg) |
| **Core PCB 布线** | **Motor PCB 布线** |
| [![Core PCB](hardware/core-pcb.png)](hardware/core-pcb.png) | [![Motor PCB](hardware/motor-pcb.png)](hardware/motor-pcb.png) |

接线与器件位号可对照[核心板原理图](../Documentation/drawings/core-schematic.pdf)和[扩展板原理图](../Documentation/drawings/motor-schematic.pdf)。仓库早期的[Core 参考图](core.png)也保存在这里。

## 调试现场

| 主控与线束 | 比赛界面 |
| --- | --- |
| [![台架主控](testing/controller-on-bench.jpg)](testing/controller-on-bench.jpg) | [![比赛界面](testing/display.jpg)](testing/display.jpg) |
| **电池充电** | **HC-08 无线升级界面** |
| [![电池充电](testing/charging.jpg)](testing/charging.jpg) | [![蓝牙升级](testing/ble-update.jpg)](testing/ble-update.jpg) |

## 赛道与球位

| 第一题移球轨迹与实物 | 第二题循线实录 |
| --- | --- |
| [![第一题轨迹](testing/task1-trace.png)](testing/task1-trace.png) | [![第二题赛道](testing/task2-track.jpg)](testing/task2-track.jpg) |
| **第四题载球循线** | **发挥一选点与行驶** |
| [![第四题赛道](testing/task4-track.jpg)](testing/task4-track.jpg) | [![发挥一实录](testing/offset-test.jpg)](testing/offset-test.jpg) |
| **右 23 mm 整圈的蓝牙抽样偏差** | **图传录像回放** |
| [![右 23 mm 偏差](testing/offset-error.png)](testing/offset-error.png) | [![图传回放页面](testing/replay.png)](testing/replay.png) |

偏差图绘制了蓝牙抽样点。主控对同一历史圈统计了 970 帧可信球位，最大绝对偏差 17 mm。[原始记录与测量口径](../Documentation/validation/README.md)

## 控制原理

| 系统总体框图 | 主程序比赛流程 |
| --- | --- |
| [![总体框图](figures/system.png)](figures/system.png) | [![主程序流程](figures/program.png)](figures/program.png) |
