# 2026-11-08 Motor 接口规划与 Core 关联修复

本轮使用 SmallCoral/schematic-design-skill，沿用现有 KiCad 工程，不重跑生成器，不新增备份或设计副本。只调整 Motor 原理图和必要参考表；用户随后要求处理 Core，另行修复其 PCB 元件关联元数据。

源码基点为 4ef36efcc087ad488966964c933727f7c5b372cc，工作区含用户未提交更改。工具为 KiCad CLI 10.0.6；[检查记录](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/verification.json)保存当前各原理图 SHA256。

## Motor

- 保留 142 个实体、184 个网络、39 路 GPIO，调整其中 25 路。A 排接右侧模块，B 排接左侧模块；完整定义见[信号分配](../reference/motor-core-pins.md)及 [规划合同](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/layout-pinplan.json)。
- Core 原理图与共享 98 触点合同保持不变。Motor PCB SHA256 为 2487ae3f6aadba3788bba3936bd0086dbe67d9bf60a262462eea4423fb9c0636，修改前后相同；未同步焊盘网络、未布线、未运行 PCB DRC。
- 清理重分配后残留的线段和 NC，缩短调试线，将电源标志接到 SWD 供电支路，修正 GND 标签朝向与注释位置。实体元件位号、UUID、符号和封装不变。
- 原生 ERC 为 0 错误、0 警告；原理图检查 955 项通过，完整网分组与历史原生电路叠加明确改动的期望合同一致。39 路复用、定时器互斥和 98 触点均核对。
- 十页 PDF 已渲染并检查；[视觉报告](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/visual_check.json)记录压线和文字重叠均 0。139 个实体有模型绑定；两个机械孔不要求模型，J1 XT60 精确模型仍缺。
- Motor 的元件 UUID 关联仍在，但 PCB 仍是旧 GPIO 网络。后续更新 PCB 需要接收这次接口重分配；不能把关联通过当作 PCB 已同步。

## Core

检查前 43 个电气封装均缺少原理图路径；按位号核对封装、数值、焊盘和网络均一致。PCB 另有 LOGO1，已标记 board_only，属于正常图形。

只在原 PCB 插入 43 个 path 字段。Core 的原理图没有待回退的工作区改动，本轮未改写；移除新增 path 行后 PCB 与原文件逐字节相同，器件位置、封装 UUID、焊盘、铜线及其他内容不变。[关联复核](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/core-association-verification.json)为 43 个器件全部匹配，预计新增、歧义、多余电气封装、型号替换、焊盘网络差异均 0。未用 GUI 执行 F8，也未运行 Core PCB DRC。

## 验证命令与边界

在 CoralC5-Motor 目录执行；kicad-cli 指向本机 C:/Program Files/KiCad/10.0/bin/kicad-cli.exe。

```powershell
kicad-cli sch export netlist --format kicadxml --output docs/netlist.xml CoralC5-Motor.kicad_sch
kicad-cli sch erc --format json --output docs/erc.json CoralC5-Motor.kicad_sch
kicad-cli sch export pdf --output docs/CoralC5-Motor_Schematic.pdf CoralC5-Motor.kicad_sch
python -B tools/check_visual.py
python -B tools/verify_schematic.py
```

PDF 检查使用现有 .scratch/m2/pdf-deps 的 PyMuPDF。检查工具为现用精简电路与本轮 GPIO 重分配增加独立合同入口，没有删除历史版本合同或隐藏 ERC 失败。

接口覆盖当前外设，但仍有 Maix 约一安限流与充电峰值、整车电源余量、UART 时序、编码器三伏三工作、通用按钮封装及线束插接待验证。当前步进是直供，没有独立 MCU 控电；固件尚未实现新外设配置，不能据原理图检查判定整车可运行或可制板。
