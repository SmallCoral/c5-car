# 同学电赛案例索引

这些压缩包和板卡工程由同学提供，作为历史参考保留。
总入口见[同学资料索引](../reference/classmate-materials.md)。
文件名中的题号沿用原交付，不据此新增赛题年份或成绩结论。

## 案例清单

| 原始文件 | 包内平台与结构 | 可参考内容 | 原始成员文件数 |
| --- | --- | --- | ---: |
| [21E 送药小车](<21E 送药小车 天猛星M0.zip>) | MSPM0G3507，单个应用主入口 | 循迹、送药任务状态、视觉结果接收 | 2217 |
| [22C 双车跟随](<22C 双车跟随 天猛星M0.zip>) | MSPM0G3507，`Head` 与 `follow` 两组工程 | 头车/跟随车分工、编码器、PID 与循迹 | 4448 |
| [23E 激光跟随](<23E 激光跟随 天空星F4.zip>) | STM32F407，`23E_F4_Read` 与 `25E_F4_Green` | 视觉坐标、X/Y 电机控制、目标切换 | 4144 |
| [25E 激光打靶](<25E 激光打靶 天猛星M0.zip>) | MSPM0G3507，`25E_chassis` 与 `25E_cloud` | 底盘与云台分工、循迹、目标跟踪 | 4441 |
| [26H 平衡球装置](<26H 平衡球装置 天空星F4.zip>) | STM32F407，`26H_F4 _Speed_now` | 稳球双环控制、闭环步进、视觉串口、循迹及菜单 | 2052 |

成员数量包含包内厂家支持代码和编译产物，不代表同学编写的应用代码数量。
五个 ZIP 的 CRC 检查均通过；这仅说明归档可读，不证明应用功能正确。
精确应用路径见[机器索引](../reference/classmate-materials-archives.json)。

## 平衡球工程的代码入口

`26H` ZIP 内的公共前缀为 `26H 平衡球装置 F407/26H_F4 _Speed_now/`。
以下路径均相对此前缀，原文件仍在 ZIP 内。

| 包内路径 | 内容 |
| --- | --- |
| `ReadMe.txt` | OLED 菜单、按键与串口/定时中断优先级说明 |
| `Core/Src/main.c` | 初始化、主循环、定时调度与任务调用 |
| `Partical/balance.c`、`balance.h` | 稳球目标、位置外环、角度内环、PID 结构 |
| `step_motor/motor_X.c`、`motor_X.h` | Emm 命令发送、电机反馈解码与速度控制 |
| `UART/urt.c`、`urt.h` | 电机与视觉串口的接收回调 |
| `Maxicam/maxicam.c`、`maxicam.h` | 球坐标提取、数据包处理 |
| `Maxicam/Buffer*` | 视觉串口缓冲和数据包辅助处理 |
| `XunJi/Xunji.c`、`Motor.c` | 循迹、轮电机和行驶任务 |
| `Hardware/menu.c`、`key.c` | 题目选择、参数显示和按键 |
| `Core/Src/tim.c`、`usart.c`、`OLED_Test.ioc` | 原平台定时器、串口及工程配置 |

对稳球逻辑和移植问题的分析集中在[平衡球参考解读](../explanation/classmate-balance-reference.md)。
本包没有摄像头端视觉应用的 Python 源码；`Maxicam` 是 MCU 侧接收与解析程序。

## 其他案例的主入口

| 案例 | ZIP 内的主入口 |
| --- | --- |
| 21E | `21E 送药小车 M0G3507/21E/3507mode/empty/empty.c` |
| 22C 头车 | `22C 双车跟随 M0G3507/Head/empty/empty.c` |
| 22C 跟随车 | `22C 双车跟随 M0G3507/follow/22C_dicheng/OLED/3507modeOLED/3507mode/empty/empty.c` |
| 23E | `23E 激光跟随 F407/23E_F4_Read/Core/Src/main.c` |
| 23E 包内另一工程 | `23E 激光跟随 F407/25E_F4_Green/Core/Src/main.c` |
| 25E 底盘 | `25E 激光打靶 M0G3507/25E_chassis/empty/empty.c` |
| 25E 云台 | `25E 激光打靶 M0G3507/25E_cloud/empty/empty.c` |

上述工程仅做入口和相关模块静态阅读，未逐函数审查全部案例。

## 扩展板工程

| 文件 | 已确认的工程内容 |
| --- | --- |
| [天猛星拓展板+12V](天猛星拓展板+12V.eprj2) | 数据库项目名 `new天猛星拓展板+丝印11111`，含 `p1` 原理图与 `pcb1` PCB 文档 |
| [天空星拓展版+12V](天空星拓展版+12V.eprj2) | 数据库项目名 `天空星拓展版2.0+12V`，含 `p1` 原理图与 `pcb1` PCB 文档 |

文件是 `.eprj2` 格式的旧板工程，不是当前 KiCad 工程。
本轮以 SQLite 只读方式核对文档结构，两份 `quick_check` 均为 `ok`。
未在 EDA 软件内进行电路审查、ERC、DRC 或转换，不从文件名推导可直接给当前核心板供电。
