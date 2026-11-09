# Coral-C5 交接：Motor M2.8P2 与 PCB 元器件模型

更新日期：2026-11-08。工作目录 C:/Users/sc/Documents/Dev/Hardware/c5-car，分支 main，远端 https://github.com/SmallCoral/c5-car.git。本轮起点提交 4ef36efcc087ad488966964c933727f7c5b372cc；交付提交以 git log -1 和远端 main 为准。

## 2026-11-09 元器件模型更新

后续用户要求将 LCD 双孔以外四个板级孔改 M3，已在原中心改为 Ø3.2mm NPTH，保留孔与焊盘 UUID。LCD 142.73/111、172.73/111 两孔仍 Ø2.0mm；原 Ø2mm 库未动，新增本地 M3 库。原生钻孔导出确认4×3.2/2×2.0，DRC仍24/156且违规内容一致；其他PCB字段、铜线、模型和原理图保持本次基线。记录 Documentation/validation/2026-11-09-m3-mounting-holes.md，基线 .scratch/m3-holes-20261109/before。

用户要求从网上补齐错误/缺失模型，不自行绘制。已下载五个原样 STEP，更新 J1、F1、J19、SW1、C1/C13/C33 的 PCB 和本地库绑定。SW1 是 DEALON G5 外形参考，脚距4.7与当前4.8mm有差异；三只电解是 Nichicon D10×L12.5/P5参考，保留未剪引线。均未冒充原厂精确型号，没有变更BOM。U1/U6/U10精确EP模型仍未取得；Diodes官网STEP正常浏览器下载亦超时，原通用外形保留。

起始PCB已由用户推进到146封装（140电气+6孔）、545段走线、32过孔。先保存 .scratch/models-20261109/before 及 baseline.json，再仅改model节点；PCB和五个本地封装去掉model节点后与本次基线全部字段一致。新资源工具 Motor/tools/verify_3d_models.py 终检319项通过、140/140模型路径存在；前后原生DRC均24违规/156未连接，失败保留。没有修改原理图、Core、布局、网络或铜线，未提交推送。完整来源、尺寸差异、命令与预览见 Documentation/validation/2026-11-09-3d-models.md。

上轮142封装/329铜对象的字节指纹是历史快照，不能把本次合法模型元数据及用户已有布线变化当作自动覆盖设计的理由。GPIO与PCB当前电气一致性本轮未复核，不靠删旧检查制造通过。

## 2026-11-08 本机续接更新

当前工作目录为 D:/dev/qrs/stm32/c5-car，源码起点8725275。用户选择仅调整GPIO及原理图，保留现有铜线，并要求UART、SPI、编码器、PWM、ADC、EXTI及Core触点全部核对。本轮已实施M2.8P2，20路GPIO变化，39路完整定义以 Documentation/reference/motor-core-pins.md 和两个当前CSV为准。蓝牙USART1、Maix UART5、步进USART6、荔枝派UART4；TFT SPI3，背光PA4/TIM3_CH4；左右编码器TIM5/TIM2、轮PWM TIM4，ADC2_IN13/ADC1_IN8。

原生ERC0错误0警告，原理图956项、独立原厂功能320项通过；十页PDF交叠0/0。几何直线交叉39→14，L1距离1944.202→1961.202mm，未包含实际避障、层或电源铜线。Motor PCB142封装、329走线/过孔对象保持本轮起始字节，未同步本次管脚网络，未运行DRC。Core及98触点合同未改。未改固件、电源元件或Maix限流值。

基线在 .scratch/pinplan-rework-20261108/before，清单已保存到Motor/docs/pinplan-rework-baseline.json；临时脚本不是后续必须依赖的工具。新增正式检查工具为Motor/tools/verify_functional_pinout.py，输出 functional-pinout-verification.json 和 functional-pinout.csv。验证命令及限制见 Documentation/validation/2026-11-08-pinplan-rework.md。本轮未提交或推送。

下面正文为上一轮M2.8P交接历史，其串口、SPI、按键映射和955项结果不再代表当前分配；Core关联修复与模块电源边界仍可追溯。

## 目标

校赛 A 题，保留 Core、轮电机、ZDT_XS 第二代 TTL 一体步进、感为八路灰度辅助板、TFT/四键、HC-08、MaixCAM2 识别、Action 4 电源图传及荔枝派 4A。用户本轮要求按当前 PCB 方位重排 Motor 的核心板接口，只改原理图、不创建备份或副本、不使用 computer-use。用户随后单独授权处理 Core 的原理图/PCB 元件关联，并要求先完成本交接再将项目当前有效变更全部推送 GitHub；不再编写提示词。

验收范围为实际原理图、完整连接合同、98 触点/复用、原生 ERC、渲染，以及 Core 关联路径修复。Motor PCB 不同步、不布线，Core 只补关联元数据。

## 当前状态

- **已验证：原理图完成。** Motor 为 M2.8P，142 个实体、184 个网络、39 路 GPIO；其中 25 路重分配。十页层次图有效，保留实体位号、UUID、封装与 Core 固定触点身份。清理重分配遗留线段、NC、地标签朝向与压线文字，电源标志接到 SWD 供电支路。
- **已验证：GPIO 方位。** 正面 A 排在右、B 排在左；全部 39 路模块信号使用对应一侧。蓝牙 USART2，Maix USART3，步进 USART6，荔枝派 USART1；TFT SPI1、PA6/TIM3_CH1 背光，灰度 PE7～PE14。完整定义只看 Documentation/reference/motor-core-pins.md 及两份当前 CSV。
- **已验证：原生与合同检查。** KiCad CLI 10.0.6 ERC 为 0 错误、0 警告。tools/verify_schematic.py 当前入口为 verify_layout_pinplan.py，955 项检查通过，包括完整原生网络分区、39 路复用和定时器分工、98 触点、资源、层次及两份 BOM 的 140 个电气器件；BOM 已删除残留 C76。
- **已验证：图纸资源。** 十页 PDF 均已渲染、目视检查，程序检查压线/文字重叠均 0。139 个实体绑定模型，H1/H2 机械孔不要求模型，J1 XT60 精确模型仍缺；通用模型不能视为实物精确核验。
- **已验证：Motor PCB 保留。** 工作文件 SHA256 为 2487ae3f6aadba3788bba3936bd0086dbe67d9bf60a262462eea4423fb9c0636，本轮前后字节相同。PCB 当前 142 个封装、329 个走线/过孔对象；J14 尚未接出铜线。本轮 GPIO 改动未同步到焊盘网络，不能把元件 UUID 匹配当作 PCB 已同步。
- **已验证：Core 关联修复完成。** 检查时 Core 的原理图、98 触点和 PCB 电气内容都与起点 HEAD 一致，没有可回退的本地电气改动。PCB 的 43 个电气封装全部缺少 path，逐位号的封装、数值、焊盘和网络原本一致。本轮只补 43 个路径，Core 原理图未改；去掉新增路径行后 PCB 与原文件逐字节一致。KiCad pcbnew 原生加载匹配 43/43，铜线 621 段、过孔 323 个保持不变。LOGO1 为正常 board_only 图形。
- **未验证：板级与实物。** 本轮未运行 Motor/Core PCB DRC，未修改布局或布线，未验证电源启动、补偿稳定性、温升、线束插接、编码器三伏三或整车功能。固件仍未实现这些模块的外围初始化及协议。

## 下一步

本轮原理图和 Core 关联工作已完成，不自动追加板级修改。用户计划自行处理 Motor PCB。

1. 用户开始 PCB 工作前，先读取当前 Documentation/reference/motor-core-pins.md 和 m2-interfaces.md，按新网表更新 Motor，保留已有铜线与布局；不能沿用旧的步进 PB6/PB7 或荔枝派 PD8/PD9。
2. 用户完成 PCB 后，再按新保存的板文件独立检查元件 UUID、焊盘网络与真实铜连接，并运行实际 DRC。955 项里的本轮 PCB 保留指纹会在合法 PCB 修改后变化，应记录新的授权边界和独立证据，不靠删除检查制造通过。
3. 实测 Maix 识别叠加充电峰值、整车启动与持续电流、稳压温升/稳定性，以及编码器电平；再完善固件。当前原理图检查不代表可运行或可制板。

## 关键文件

| 路径 | 本轮作用 |
| --- | --- |
| Hardware/stm32c5a3/Hardware/CoralC5-Motor/03_Core_Interface.kicad_sch | 39 路 GPIO 使用方式与短接线；固定插槽符号/触点定义不变 |
| Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_sch | 层次总览，荔枝派说明更新为 USART1 |
| Hardware/stm32c5a3/Hardware/CoralC5-Motor/09_Bluetooth_Stepper.kicad_sch | 步进直连说明为 USART6 |
| Hardware/stm32c5a3/Hardware/CoralC5-Core/CoralC5-Core.kicad_pcb | 只补 43 个封装 path；无电气、布局或铜线改动 |
| Hardware/stm32c5a3/Hardware/Shared/interface.csv | 不可随意重排的 98 触点合同，本轮未改 |
| Documentation/reference/motor-interface.md | J4/J6 MG513 六针线序唯一入口 |
| Documentation/reference/motor-core-pins.md | 当前 39 路 GPIO、定时器、串口和方向关系 |
| Documentation/reference/m2-interfaces.md | 当前连接器逐针定义与保护边界，替代旧复杂方案 |
| Documentation/validation/2026-11-08-layout-pinplan.md | 本轮具体改动、工具与检查限制 |
| Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/core_pin_assignment.csv | 13 路轮控制分配 |
| Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/m2_pin_assignment.csv | 26 路外设分配 |
| Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/layout-pinplan.json | 明确的角色映射、旧/新触点及修改前指纹 |
| Hardware/stm32c5a3/Hardware/CoralC5-Motor/tools/verify_layout_pinplan.py | 从历史原生合同加明确改动得到完整期望网络，不从当前结果反推通过 |
| Hardware/stm32c5a3/Hardware/CoralC5-Motor/tools/verify_pcb_associations.py | UUID 关联只读预检，允许正常 board_only 图形 |
| Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/verification.json | 当前 955 项原理图核对与具体限制 |
| Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/core-association-verification.json | Core 43 个关联路径、原生加载与保留证据 |
| Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/erc.json | 当前原生 ERC |
| Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/visual_check.json | 十页渲染交叠检查 |
| Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/CoralC5-Motor_Schematic.pdf | 当前导出图纸 |

## 决策与注意事项

- 本轮采用用户指定的 SmallCoral/schematic-design-skill；PCB 方位只读用于接口规划。Core 关联修复是用户后来单独授权的元数据修改，不授权改 Core 电路、触点或铜线。不要重跑旧生成器、恢复早期快照或用旧板覆盖用户继续保存的文件。
- 本轮不新增备份/副本，也不编写 resume-prompt。现有历史目录来自此前工作，保留作旧证据，不能称其为本轮新基线或当前检查结果。Git 能提供版本回溯。
- Maix 保留 U10 SGM2531C、R72 和去耦；ENUV 接输入、OVP 接地、SS 悬空。独立理想二极管与 UART 隔离已按此前确认删除。典型约 0.999A 限流和六针 1A 额定不能替代识别加充电的实测；模块多个五伏口相通，不并联第二个带电 VBUS。
- 步进 J23 仍为 XH 8P，只用 1/2/7/8，VM/UART 直连；F2、Q6～Q9、U15 和 STEP_PWR_EN 均已删除，PD12 空闲。当前没有 MCU 单独断电。旧提示词中的复杂控电电路不是当前方案。
- 双 AP64500 五伏标称 5.088V/500kHz，A 轨预算 4.2A、B 轨 4.0A。整车约 11.09A 是 85% 效率、轮 4A、步进 3A 等假设下的估算，12V/12.5A 适配器余量和十五安保险分断/温升待实测。芯片额定五安不是本板持续能力。
- Core 五伏接 D3 后 A48/B48，由 Core 自带稳压产生三伏三；外设 U7 的三伏三不与其并联，Core 外接三伏三仍为 100mA 预算。荔枝派 U11/U12 保留电平转换但常使能；J18:2 NC，已无 Q3 掉电感知。
- GPIO 变更需固件一并配置；原串口实例不能照旧。左右编码器 TIM5/TIM2、轮 PWM TIM4、背光 TIM3 分开。PA9/PA11/PA12/PA13/PA14、PH2、PE3、PD2 保留 Core 原用途。
- 封装与插接方向、按钮脚跨距、TFT 实孔、通用模型与编码器三伏三工作仍需实物确认。H1/H2 保留用户现位置，本轮不调整。
- 不修改旧 D:/dev/qrs/stm32/Coral-C5；机械和固件未在本轮修改。不关机。
- 项目当前已有未提交用户更改，本轮全部推送按用户最新指令进行。原版提示词只属于旧历史，不继续执行其中的同步、保存新副本或旧目录命令。

## 运行与核验

根目录为 C:/Users/sc/Documents/Dev/Hardware/c5-car。当前工具路径：

- KiCad CLI：C:/Program Files/KiCad/10.0/bin/kicad-cli.exe
- KiCad Python（pcbnew）：C:/Program Files/KiCad/10.0/bin/python.exe
- 通用 Python：C:/Users/sc/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe
- PDF 依赖：项目 .scratch/m2/pdf-deps；不上传临时运行时。换机器先提供兼容 PyMuPDF。

在 Motor 目录运行：

```powershell
& 'C:/Program Files/KiCad/10.0/bin/kicad-cli.exe' sch export netlist --format kicadxml --output docs/netlist.xml CoralC5-Motor.kicad_sch
& 'C:/Program Files/KiCad/10.0/bin/kicad-cli.exe' sch erc --format json --output docs/erc.json CoralC5-Motor.kicad_sch
& 'C:/Program Files/KiCad/10.0/bin/kicad-cli.exe' sch export pdf --output docs/CoralC5-Motor_Schematic.pdf CoralC5-Motor.kicad_sch
$env:PYTHONPATH = 'C:/Users/sc/Documents/Dev/Hardware/c5-car/.scratch/m2/pdf-deps'
& 'C:/Users/sc/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' -B -X utf8 tools/check_visual.py
& 'C:/Users/sc/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' -B -X utf8 tools/verify_schematic.py
```

Core 关联单独调用 tools/verify_pcb_associations.py 的 audit(board_path, netlist_path)，输入 Core PCB 与新导出的 docs/core_netlist.xml，不运行该脚本默认 Motor 保存快照入口。检查结果分别看原理图、资源、关联、DRC 和实物，不相互替代。

提交检查：当前源码的 git diff --check 通过。暂存全部项目后，git diff --cached --check 在此前 schematic-only-resume 的 14 份原始 SVG 中报告 6389 处导出器行末空格；保留历史导出，没有伪报该项通过。这与当前原理图 ERC、955 项合同和十页渲染的通过分别记录。

交接全文保存于本文件，共六节。Windows 未提供 pbcopy，未复制剪贴板；用户要求不再另写提示词。
