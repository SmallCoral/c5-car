# Motor M2.4 固定蓝牙供电与跳帽接口

2026-11-06，继续按 `schematic-design` 修改原理图及必要符号库，按 `docs update` 同步状态。
工具为 KiCad CLI 10.0.6、Python 3.12.14、PyMuPDF 2.1；源码基于 `4cf36a2ce5a0d5bb6b469a9a82320fcfbcc38c1f` 加已有工作区与本轮修改。

## 本次修改

| 项目 | 结果 |
| --- | --- |
| 蓝牙供电 | J15 的 1 脚直接接独立外设三伏三 V3V3_EXT，2/3/4 脚仍为 GND/BT_TX/BT_RX |
| 删除选择器 | 删除 J16 实体、蓝牙五伏分支、VBT 中间网络，以及蓝牙/步进页和主页对应的 V5_EXT 层次端口 |
| 保留五伏用途 | 外设五伏继续给灰度和两路摄像头供电，仅删除蓝牙的五伏选择支路 |
| J10 跳帽 | 恢复 2.54mm 两针直排针及匹配模型，保持位号、实例 UUID、引脚 UUID 和原电路连接 |
| HC-08 实物针序 | 后续照片确认从 3V3 端起为 3V3/GND/RXD/TXD；J15 第 3/4 脚名称改为模块 RXD/TXD，网络仍接主控 TX/RX，电气连接未变 |
| 用户排布 | 从本轮保存的当前文件增量修改，保留用户已有的器件位置和其他手工调整，未重跑旧生成器 |

J10 插上跳帽后，主控 PE8 的 MOTOR_EN_REQ 经 R6、J10 控制左右轮 MOTOR_RUN。
拔帽后 R7 将 MOTOR_RUN 拉低，左右轮 DRV8874 的 nSLEEP 为低，驱动休眠；插帽本身不会命令电机转动。
它不切断整车供电，也不控制 ZDT 一体步进驱动。
J16 已删除，蓝牙不再需要或支持电压选择跳帽。

## 验证

| 检查 | 结果 |
| --- | --- |
| 原生网表与 ERC | ERC 0 错误、0 警告，无新增排除 |
| `tools/verify_schematic.py` | 1571 项通过，0 失败，含实物针序检查 |
| 完整连接对比 | 138 个引脚分组符合既有合同加本次明确变更，J15 电源与 U7 输出同网，J16 不在实体列表 |
| 主页层次连接 | 106 个现存引脚都有实际导线，父子页端口一致；仅移除不再使用的蓝牙五伏端口 |
| 资源 | J10 两针排针的焊盘/引脚集合和已有模型引用匹配；当前 116 个实体位号含两个孔，113 个有可解析模型 |
| 图面 | 九页自动版面检查无导线/文字或文字相互交叠；已查看实际导出的核心接口及蓝牙页 |
| PCB、固件、实物 | 本轮未读取或修改 PCB，未运行 DRC、固件或实物验证 |

基线在 [baseline-2026-11-06-jumpers](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/baseline-2026-11-06-jumpers/)。
首次合同检查发现两张电机页里未使用的二针连接器缓存默认封装尚未同步，已与局部库统一为跳帽排针，未改变电机连接。
同时记录了早期基线到本轮已存在的 IPC2581 导出修订号 `M2→M2.3`；本轮未写工程文件，其余工程设置完全一致，验证按这一明确差异比对而没有覆盖用户设置。

[完整验证](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/verification.json)、[网表](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/netlist.xml)、[ERC](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/erc.json)、[版面检查](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/visual_check.json)记录当前结果。
[M2 接口表](../reference/m2-interfaces.md)是当前线序入口；电源预算和既有模型缺项沿用 [M2.3 记录](2026-11-06-m23-power-connectors.md)。

本日后续的[用户蓝牙照片](../蓝牙/HC08_实物针序_2026-11-06.png)已保存原件与 SHA256，图页和局部符号库同步为模块侧引脚名称。
修改前基线在 [baseline-2026-11-06-bluetooth-pinout](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/baseline-2026-11-06-bluetooth-pinout/)，修改前后 138 个原生引脚分组完全一致。
