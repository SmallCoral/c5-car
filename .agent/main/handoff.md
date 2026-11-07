# Coral-C5交接：简化Maix保护与步进直连接口

生成日期2026-11-08，分支`main`，工作目录`D:/dev/qrs/stm32/c5-car`。本次上传基于父提交`d79ffc78ad5093452b1b1b852f7de347f0cc084b`；实际上传后的HEAD以`git rev-parse HEAD`为准。本文件按完整handoff范围编写，新接手者无需会话历史。

## 目标

校赛A题Motor底板继续采用Core、荔枝派4A、MaixCAM2识别、Action4图传、感为八路灰度和ZDT_XS第二代TTL一体套件。下一轮按用户最后确认，减少不必要的保护：Maix保留单芯片简单限流，步进改直接电源与UART接口。完成标准是原理图实际修改、PCB元件/网络同步、独立连接合同和98触点/GPIO复核、原生ERC/DRC及图纸核查分别报告。

用户最新指令是“还是先写交接文档与提示词吧，然后上传到GitHub就行了”。因此**硬件修改已停止，新的简化方案尚未实施**。本条交接不自动恢复工作；用户再次要求继续后再编辑电路。

## 当前状态

- **已验证**：当前仍为M2.8复杂电路。Maix的U10、U14/Q5、U13/Q4及J27等均在；步进的Q6～Q9、U15、D5、F2和PD12通断控制均在。没有按本轮讨论删除元件。
- **已验证**：最新保存PCB有180个封装、176段走线、3过孔，SHA256`e90e3653baefca012a6a8e8c4b6e853c7d2f1770460ffc8d91d0b7b150d3ee80`。用户在此前交付后继续保存和布线，不能拿旧151段或旧摆放覆盖当前文件。
- **已验证**：原理图180实体位号，205网络分区与独立M2.8合同一致；F8默认UUID规则唯一匹配，预计新增/替换0，焊盘网络不匹配0，原生一致性0。未在GUI中实际操作F8。
- **已验证**：最新ERC为0错误3警告，均为U10/U11/U12缓存与本地库符号差异，编号/名称/电气类型相同而引脚几何等不同。
- **已验证**：最新DRC18违规、379项未连接；15处Courtyard重叠、1处J14孔间距、1处缺少外形、1处L_nFAULT悬空过孔。全走线错误补查结果相同，未额外发现短路/铜间距问题。没有GND走线或地铜区。
- **未验证**：本轮未重新完成旧3786项全套保留约束/资源检查和新的98触点逐项报告；没有重新导出PDF；实物电源、温升、启动、UART和稳球均未测试。旧PDF/0交叠、旧40或229条DRC只属于历史快照。
- **Git范围**：本轮上传Motor当前工程、相关Documentation、根README与.agent交接。工作区网络Expansion目录另有71个外部删除，不属于本次任务，未恢复且不纳入本次提交；不要顺手`git add -A`把这些删除上传。

详细原始证据及命令在`Documentation/validation/2026-11-08-motor-recheck.md`，完整输入快照与原生报告在`Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/recheck-2026-11-08-004941-e90e3653/`。

## 下一步

1. 读取根AGENTS.md、本交接及`Documentation/reference/module-power-simplification.md`，核对实时文件后保存新基线。不重跑旧生成器或.scratch旧脚本。
2. 简化Maix：优先复用U10 SGM2531C，ENUV接VIN，OVP接GND；保留R72限流及必要输入/输出去耦，SS可按原厂默认斜率开放。删除独立U14/Q5防反灌、额外分压和控制；按原厂确认UART电平后直连J19，删除U13/Q4隔离。保持J19针序、Maix识别职责及六针1A额定边界。不要把用户曾说的“全删保护”当作最终要求，用户随后已改为保留非常简单的保护。
3. 简化步进：用户明确要求删除MCU通断功能。J23:1接总开关后的VM，2接GND，7接PB6/USART1_TX，8接PB7/USART1_RX，3～6NC。删除Q6～Q9、U15、D5/F2和对应R81～R90/C80，删除STEP_PWR_EN层次端口，J14:B30/PD12改NC。更新GPIO40→39及相应CSV/文档，不保留旧默认断电叙述。
4. 其余保留公共双AP64500五伏、TFT/蓝牙三伏三、Core供电隔离、荔枝派1.8V转换及Type-C必要CC/供电功能；不要把这些都误当作冗余保护删除。新选型需国内立创可购现货证据。
5. 同步原理图/PCB/BOM/独立网表合同，保留未删除件的位号、UUID和用户布局、铜线。先检查被删元件有没有已布连接，不为了DRC计数清空走线。
6. 导出原生网表、ERC和新PDF；检查98触点、39GPIO、封装/模型、F8默认UUID匹配，再运行原生一致性/DRC。记录真实失败。最后分别验证电源、串口和实物，不能将同步通过当成可制板。

## 关键文件

| 文件 | 用途 |
| --- | --- |
| `Documentation/reference/module-power-simplification.md` | 最终确认的精简需求与具体实现起点，未实施 |
| `.agent/main/resume-prompt.md` | 可直接粘贴的续接提示词 |
| `Hardware/stm32c5a3/Hardware/CoralC5-Motor/08_Grayscale_Cameras.kicad_sch` | Maix复杂保护和串口，待简化；灰度部分保留 |
| `Hardware/stm32c5a3/Hardware/CoralC5-Motor/09_Bluetooth_Stepper.kicad_sch` | 步进开关/隔离待删除，HC-08保留 |
| `Hardware/stm32c5a3/Hardware/CoralC5-Motor/03_Core_Interface.kicad_sch` | PD12改NC、STEP_PWR_EN端口待删除 |
| `Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_sch` | 层次总览端口与说明待同步 |
| `Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_pcb` | 当前用户布局与176走线，先备份 |
| `Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/m2_pin_assignment.csv` | 外设GPIO分配；删除PD12后更新端点/数量 |
| `Hardware/stm32c5a3/Hardware/CoralC5-Motor/tools/m28_contract.py` | 旧M2.8静态电气意图，简化后要新增明确合同，不能以当前网表反建期望 |
| `Hardware/stm32c5a3/Hardware/CoralC5-Motor/tools/verify_pcb_associations.py` | 默认UUID匹配检查；audit()可单独检查当前对应关系 |
| `Hardware/stm32c5a3/Hardware/Shared/interface.csv` | 不变的98触点合同 |
| `Documentation/validation/2026-11-08-motor-recheck.md` | 最新ERC/DRC、警告说明及原始证据 |

## 决策与注意事项

- 12V/12.5A适配器、两路AP64500的5.088V预算4.2A/4.0A保持现状；芯片额定不是实板能力。Maix简化后单芯片不宣称独立防反灌，模块USB与六针五伏不能默认可任意并联供电。
- SGM2531C RevA2的第11～12页明确允许ENUV接VIN、未用OVP接GND、SS浮空默认斜率。ILIM不能浮空。R72=95.3k的公式典型约0.9986A，不是全温严格上限；模块识别加充电是否够用未实测。
- 旧verify_schematic.py分派M2.8，对旧姿态、属性、历史和工程哈希有严格检查；当前用户另存、图形调整、Expansion外部删除会触发旧约束失败。不要删检查或回滚用户设计来冒充通过，新版要明确当前基线/合同。
- verify_pcb_sync.py当前额外验证“只改F8关联”，它以此前151走线基线为准；当前176走线是用户后续工作，默认保留检查可能失败。应使用独立audit()检查对应，并为下一次真实修改建立新保留基线。
- F8历史故障是左右轮48个封装丢失path；已经修复。按位号计数不能替代默认UUID匹配。禁止无理由换现有符号/封装UUID。
- U10/U11/U12缓存符号几何和本地库不同，强刷旧库可能移动引脚和断线，应保留用户当前布局并合理协调库资源。
- 不修改旧`D:/dev/qrs/stm32/Coral-C5`。不使用CUA，不自动布局/布线，不改98触点合同。固件和机械不在本轮范围，不关机。
- `.scratch`不上传，别依赖其中一次性脚本；本交接、需求、原厂PDF和输入/报告均在跟踪目录。未发现pbcopy，未复制剪贴板，文件为主要交付。

## 运行与核验

Motor目录为`Hardware/stm32c5a3/Hardware/CoralC5-Motor`。本机CLI：`D:/KiCad/10.0/bin/kicad-cli.exe`，Python：`D:/KiCad/10.0/bin/python.exe`，运行Python加`-X utf8 -B`。

```powershell
git status --short
git rev-parse HEAD
# 进入Motor后，使用新建报告目录承接输出，不覆盖历史证据
& 'D:/KiCad/10.0/bin/kicad-cli.exe' sch export netlist --format kicadxml --output docs/netlist.xml CoralC5-Motor.kicad_sch
& 'D:/KiCad/10.0/bin/kicad-cli.exe' sch erc --format json --severity-all --output docs/erc.json CoralC5-Motor.kicad_sch
& 'D:/KiCad/10.0/bin/kicad-cli.exe' pcb drc --format json --schematic-parity --severity-all --all-track-errors --refill-zones --output docs/pcb-drc-next.json CoralC5-Motor.kicad_pcb
# 新版原理图/保留约束检查器须先适配已确认简化；旧通过结果不可复用
git diff --check
```
