# 2026-11-10 Motor PCB 复查

检查日期：2026-11-10；整理日期：2026-11-11。
对象为 `CoralC5-Motor.kicad_pcb`，不包含 Core PCB 复查。
起点提交为 `0b9b2a66126a25e0a6bbb31a1b0634d705a871a3`，KiCad CLI 为 10.0.6。
PCB SHA256：`9de443e35a98fb3a4f64617e2b5f9bcd7b38bafe53b79f5d6056c7db906da830`。
11 月 11 日整理再次核对指纹一致；本轮没有修改设计或将待改项标作完成。

## 结果与原始证据

| 检查 | 结果 | 证据 |
| --- | --- | --- |
| 原生 DRC，内存中重新铺铜 | 17 错误、6 警告；未连接 0 | [DRC JSON](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/pcb-review-2026-11-10/drc-refill.json) |
| 原理图一致性 | 6 警告：4 条重复位号、2 条机械孔属性差异 | 同上 |
| 新导出网表与 PCB | 有效焊盘网络错配 0；H1 重复和孔关联使脚本返回失败 | [同步检查](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/pcb-review-2026-11-10/verify-pcb-sync.json) |
| 板形、层数和钻孔 | 100×100mm、双层、1.6mm；872 个普通过孔和 1 个微孔 | [统计](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/pcb-review-2026-11-10/stats.txt)、[钻孔报告](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/pcb-review-2026-11-10/drill-report.txt) |
| 布局复查 | 顶视 3D、铜层/Fab 图及原生几何检查；发现电源回路和散热风险 | [摘要](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/pcb-review-2026-11-10/review-summary.json) |

上次的 14 项未连接、J8 的 V3V3/SWCLK 间距和 L_nFAULT 悬空过孔已经消失。
当前没有通过完整打板审查，DRC 配置也不等于完整的嘉立创 DFM 规则。

## 需要修改或核对

| 类别 | 位置与证据 | 处理方向 |
| --- | --- | --- |
| 孔到铜间距 | J14:51 与 NPTH，实际 0.05mm，工程要求 0.25mm；孔中心约 X186.4/Y120.8 | 对照连接器图纸退让铜皮及膏层，保留正确定位孔位 |
| 钻孔工艺 | V5_SYS，X179.5/Y118，0.3mm 外径、0.1mm 钻孔 | 改普通通孔；例如 0.6/0.3mm 后重跑 DRC |
| USB-C 电源 | U8 输出、U16 输入/输出含承担供电的 0.2mm 线段 | 加宽或铺铜，换层使用合适过孔，按实际铜厚核算 3A 压降与温升 |
| 散热 | U1/U6/U2/U4 的 EP 下没有 GND 过孔；U6/U2/U4 中心 5mm 内没有 GND 过孔 | 结合底层地铜及焊接工艺增加有效散热过孔 |
| 降压开关回路 | U1→L1、U6→L2 的 SW 路线约 8～9mm，宽 0.8mm | 电感靠近 SW，缩短回路，反馈远离开关节点 |
| 机械孔标识 | H1 共 5 个，另有 H2；原理图/PCB 的 H1 封装名称及值不一致 | 统一唯一位号与机械孔关联，不把所有孔强行同步为同一符号 |
| 丝印 | J1 越板边 2 条、C13 与 C19 焊盘开窗冲突 4 条 | 修剪丝印并保留清楚的接口/极性标识 |

电源、散热与开关回路是静态布局风险，尚无实测温升、振铃或带载结果；不能据此声称器件必然过热或电路已经验证可用。

## 16 处 Courtyard 重叠

| 区域 | 器件对 |
| --- | --- |
| 左电机附近 | R16/C16、R16/R18、R19/R18、C16/C17、C11/C10、U2/C10、C17/R18、C19/C13 |
| 右电机附近 | C37/C36、R39/J6、R39/R38、U4/C30、C40/J6、C36/R36、C38/R37、C39/J6 |

Courtyard 重叠不直接证明器件本体相撞。
按实际料号尺寸、焊接空间和贴装要求决定移位或有依据的例外；详细坐标及 UUID 在 DRC JSON 中。

## 命令与边界

在仓库根目录执行，输出到临时目录：

```powershell
$cli = 'C:/Program Files/KiCad/10.0/bin/kicad-cli.exe'
$pcb = 'Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_pcb'
$sch = 'Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_sch'
$out = Join-Path $env:TEMP 'c5-pcb-review-20261110'
New-Item -ItemType Directory -Force $out | Out-Null
& $cli pcb drc --format json --severity-all --all-track-errors --schematic-parity --refill-zones --output (Join-Path $out 'drc-refill.json') $pcb
& $cli sch export netlist --format kicadxml --output (Join-Path $out 'motor-current.xml') $sch
& $cli pcb export stats --output (Join-Path $out 'stats.txt') $pcb
& $cli pcb export drill --output (Join-Path $out 'drill') --excellon-separate-th --generate-report --report-path (Join-Path $out 'drill-report.txt') $pcb
Copy-Item -LiteralPath $pcb -Destination (Join-Path $out 'review-copy.kicad_pcb')
& 'C:/Program Files/KiCad/10.0/bin/python.exe' 'Hardware/stm32c5a3/Hardware/CoralC5-Motor/tools/verify_pcb_sync.py' --board (Join-Path $out 'review-copy.kicad_pcb') --netlist (Join-Path $out 'motor-current.xml') --output (Join-Path $out 'verify-pcb-sync.json')
```

DRC 未使用 `--save-board`，没有将内存重铺铜写回源板。
同步脚本使用内容相同的临时 PCB 和新导出网表，避开旧修复记录的历史保留约束；其真实失败保留在输出中。
配置中的忽略检查见 DRC JSON 的 `ignored_checks`，没有调低规则消除问题。
本次未执行 Core 复查、实物插接、完整 Gerber DFM、温升、启动、负载阶跃或 EMI 测试。

原厂参考：[嘉立创工艺能力](https://www.jlc.com/portal/vtechnology.html)、[AP64500](https://www.diodes.com/datasheet/download/AP64500.pdf)、[DRV8874](https://www.ti.com/lit/ds/symlink/drv8874.pdf)、[TPS25810](https://www.ti.com/lit/ds/symlink/tps25810.pdf)。
