# Motor 飞线规划与全功能引脚复核

日期：2026-11-08。源码起点 `8725275ab4f3aace2bfe87971e3c5ed062f33165`，分支 `main`，本轮结果包含尚未提交的原理图修改。工作目录 `D:/dev/qrs/stm32/c5-car`。使用 KiCad CLI/pcbnew 10.0.8、其 Python 3.11、Codex 捆绑 Python 及 uv 临时 PyMuPDF 环境；精确运行版本见 `docs/pinplan-rework-final-audit.json`。

用户选择调整 GPIO 和原理图以改善 PCB 飞线，保留现有铜线；随后强调全部引脚功能都要核对。没有授权或执行实际重布线。沿用项目保存的 schematic-design 规范，文档采用 mthines docs update。

## 修改范围与基线

基线先于编辑保存到 `.scratch/pinplan-rework-20261108/before/`，30份输入的 SHA256 和当时 Git 状态见 [基线清单](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/pinplan-rework-baseline.json)。保留用户已保存的 Motor PCB、工程设置和历史改动，不恢复旧板或运行旧生成器，不处理网络 Expansion 的71项外部删除。

本轮修改核心板接口页的20路GPIO连接，以及主页SPI/串口说明。142个实体器件、184个网络、39路GPIO不变；实体器件位号、UUID、封装和位置保持。Core原理图及98触点合同保持原样。CSV、接口表、入口和交接同步到M2.8P2。

## 功能核对

每一路检查“模块端点→Motor J14触点→Core J13触点→U1实际LQFP100管脚→GPIO→外设及AF”，不能用网络名字证明硬件功能。

- 固定触点和实际Core原生网表逐针核对，MCU管脚号另对照本地 DS15137 Rev3 Table13 的 LQFP100 栏。39个选中GPIO在Core均为专用引出，没有额外板内负载。
- AF除原PDF版面解析器外，独立抓取 [ST Table14 HTML](https://www.st.com/content/st_com/en/technical-documents/stm32c5a3cg_asset/chunk-010-topic-stm32c5xx-db-ds-alternate-functions.html) 的16列单元格；保存[原厂单元格快照](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/sources/pinplan-st-table14.json)及源HTML SHA256，避免同一个解析器自证。
- 蓝牙为PA6/PA7、USART1、AF7；Maix为PB13/PB12、UART5、AF14；步进为PC6/PC7、USART6、AF7；荔枝派为PB9/PB8、UART4、AF8。每组TX/RX属于同一实例，四组互不复用同一实例。Maix模块端自己的UART4、荔枝派Linux端UART1与Core编号不同。
- TFT为PB1/SPI3_SCK/AF4和PB2/SPI3_MOSI/AF7；片选PC4、复位PB0、DC为PC5。AF编号不必相同，必须分别属于相同SPI实例。
- 左编码器PA0/PA1、TIM5_CH1/CH2；右编码器PB3/PA15、TIM2_CH2/CH1。每组使用具备编码器模式的同一定时器CH1/CH2；右计数方向需实车标定。
- 轮PWM为PC2/TIM4_CH4、PB6/TIM4_CH1，共享轮PWM频率；背光为PA4/TIM3_CH4。TIM5、TIM2、TIM4、TIM3计数器互不冲突，没有将CHxN误作普通主通道。
- 电流采样保持PH5/ADC2_IN13和PC0/ADC1_IN8，核对ADC实例与真实通道。故障输入PH4/EXTI4、PC10/EXTI10互不冲突。
- 灰度保持PE7～PE14八路并行、无内部上下拉。按钮从左到右PD15/PA8/PA10/PH3，低有效、轮询消抖；PA3请求轮使能，上电和故障保持低。
- PA9、PA11/PA12、PA13/PA14、PH2、PE3、PD2继续保留Core原有用途；Core三伏三、电源、复位和调试合同不改。

完整管脚号及电气端点见[逐路CSV](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/functional-pinout.csv)，结果见[独立功能报告](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/functional-pinout-verification.json)。

## 飞线比较

使用用户当前PCB实际焊盘坐标作为固定输入。串口收发成对集中，调整屏幕控制及四键顺序。39条“J14→模块端点”直线的交叉由39处降到14处，减少约64.1%；L1距离和从1944.202mm到1961.202mm，增加17mm、约0.87%。[对比图](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/pinplan-routing-comparison.png)和[几何输入](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/pinplan-geometry-input.json)保留具体端点。

这是连接顺序规划，没有计算板内障碍、走线宽度、层、回流、实际绕行和电源走线。剩余14处直线交叉不等于14条必然短路或14个必须过孔；减少直线交叉也不等于证明可以单层完成。

Motor PCB保持本轮起点字节，142个封装、329个走线/过孔对象不变；本轮没有更新焊盘网络。默认工具明确报告`motor_pcb_update_pending=true`。将来按新网表更新PCB后，需要再独立检查焊盘网络、铜线真实连通和DRC。

## 执行与结果

在仓库根目录执行：

```powershell
& 'D:/KiCad/10.0/bin/kicad-cli.exe' sch export netlist --format kicadxml --output Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/netlist.xml Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_sch
& 'D:/KiCad/10.0/bin/kicad-cli.exe' sch export netlist --format kicadxml --output Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/core_netlist.xml Hardware/stm32c5a3/Hardware/CoralC5-Core/CoralC5-Core.kicad_sch
& 'D:/KiCad/10.0/bin/kicad-cli.exe' sch erc --format json --output Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/erc.json Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_sch
& 'D:/KiCad/10.0/bin/kicad-cli.exe' sch export pdf --output Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/CoralC5-Motor_Schematic.pdf Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_sch
& 'D:/KiCad/10.0/bin/python.exe' -B -X utf8 Hardware/stm32c5a3/Hardware/CoralC5-Motor/tools/verify_functional_pinout.py
& 'D:/KiCad/10.0/bin/python.exe' -B -X utf8 Hardware/stm32c5a3/Hardware/CoralC5-Motor/tools/verify_schematic.py
```

PDF检查通过 `uv run --no-project --python <bundled-python> --with pymupdf python -B -X utf8 .../tools/check_visual.py` 执行。

最终原生ERC为0错误0警告；`verify_schematic.py` 956项通过，其中调用独立功能审计320项；十页PDF压线与文字交叠0/0，修改页目视核对。139个实体绑定模型，H1/H2不要求模型，J1精确XT60模型仍缺。

编辑中间ERC曾报告4个已释放触点（B21、B4、B37、A29）未连接。原因是新增NC对象的关键字被错误引号化，KiCad未识别；已修复原生对象语法并重新导出，未增加ERC排除或降低规则。该中间失败不隐藏，最终报告只对应修复后的文件。

保留审计发现本地`.kicad_prl`的`board.active_layer`从25变为0，其余字段相同；该文件属于打开工程的本地界面状态，未恢复或覆盖。原始字节比较失败和字段差异保留在最终审计的`local_ui_state_observations`，不将其写成字节未变；PCB设计文件及`.kicad_pro`保持基线字节。

未运行PCB DRC，未更新固件，未上板验证编码器三伏三、电源启动/温升、串口协议、线束及整车功能。Maix不充电工况的启动/识别电流仍未实测；本轮没有改供电或1A限流值。未提交、推送或修改旧Coral-C5项目。
