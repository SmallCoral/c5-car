# M2.6：取消荔枝派、步进电源控制与易焊图传保护

2026-11-07。基于 `cc8dcdb9731c375813c00e5a12133c461657d29e` 加用户当前手工保存的原理图，再作增量修改。工具为 KiCad CLI 10.0.6 与项目 Python 检查器；修改前基线见 [baseline-2026-11-07-connectors](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/baseline-2026-11-07-connectors/baseline.json)。

用户最终要求仅修改原理图，PCB 由用户更新后另行检查。本轮未读取或修改 PCB，未运行 DRC，未修改固件，也未推送 GitHub。此前被 Esc 停止的电脑操作不视为完成 PCB 保存或同步。

## 本轮电路

- 删除荔枝派 J18/J25/J26、U8/U9、U11/U12、Q3、L3 及对应外围，移除整张 `10_Lichee_Power.kicad_sch` 和主页引用；释放 PD8/PD9。
- 步进只保留 J23 普通 XH 8P。1=电源、2=地、7=模块 RX、8=模块 TX，3～6 不连接。J22 脉冲备用及 R54/R57/R58、J24 重复电源口删除；PE9/PD14/PD15 释放。
- PD12 / J14:B30 / STEP_PWR_EN 高有效控制电源，低电平或悬空时默认关闭。F2 后增加 Q6/Q7 共源背靠背 P-MOS，R81=100k 栅源上拉；Q8 经 R82=4.7k 拉栅，R83=10k 基极输入、R84=100k 基极下拉。D5 为 12V 栅源保护稳压管。
- U15 SN74LVC2G125 隔离两路 TTL。Q9 从开关后的 VM_STEP 经 100k/100k 分压感知供电，R85=10k 默认将 nOE 拉高。断电时 MCU TX 不能持续经缓冲输出给驱动供电。该电路不是精密欠压电源良好检测，固件仍需遵守上电等待和停机时序。
- U10 改为 SGM2531CXPS8G/TR，SOIC-8 EP，脚距 1.27mm；U14 LTC4412 + Q5 DMP3007 补上低压降防反灌。U10 保留原位号及对应功能引脚 UUID，明确映射到新脚号。J27 仍是使能跳帽。

保留用户的器件身份与手工设计；左右轮、外设降压和 TFT 按键页与本轮基线逐字节一致。电源页仅更新预算说明。主页所有已用信号继续以真实层次导线连接，新增 STEP_PWR_EN 已连续接到功能页。98 触点合同不变；现为 13 路轮控制加 25 路外设，共 38 路 GPIO。

## 原厂依据

| 器件或接口 | 原厂资料与核对位置 |
| --- | --- |
| SGM2531C | [原厂 PDF](https://www.sg-micro.com/rect/assets/300dbe1c-22ce-4aa8-9a2a-98b968b29eb2/SGM2531C.pdf)，2025-07 Rev A.2，第 2～5 页订货、脚序、电气及定时，第 11～14 页 UV/OV、限流、体二极管和故障处理，第 15 页 SOIC EP 焊盘；[本地原件](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/sources/SGM2531C_RevA2.pdf) |
| LTC4412 | [原厂 PDF](https://www.analog.com/media/en/technical-documentation/data-sheets/4412fb.pdf)，4412fb，第 6～8 页引脚、工作原理及 P-MOS 方向；[本地原件](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/sources/LTC4412_4412fb.pdf) |
| DMP3007LK3 | [原厂 PDF](https://www.diodes.com/datasheet/download/DMP3007LK3.pdf)，DS40137 Rev2-2，第 1～3 页，1=G、2=D、3=S，TO252，最大 7mΩ/-10V、10mΩ/-4.5V；[本地原件](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/sources/DMP3007LK3_DS40137.pdf) |
| MMBT3904 | [原厂 PDF](https://www.diodes.com/datasheet/download/MMBT3904.pdf)，DS30036 Rev27-2，2025-03，第 1～3 页，SOT23 的 1B/2E/3C 与开关参数；[本地原件](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/sources/MMBT3904_DS30036.pdf) |
| BZT52-C12 | [Nexperia 原厂 PDF](https://assets.nexperia.com/documents/data-sheet/BZT52_SER.pdf)，2025-10-13 Rev2，第 2～4 页，SOD123、1K/2A 及 C12 稳压参数。在线 PDF 已读；本地下载返回 HTTP403，未声明取得本地副本 |
| SN74LVC2G125 | [TI 原厂 PDF](https://www.ti.com/lit/ds/symlink/sn74lvc2g125.pdf)，引脚、三态使能与 Ioff；[本地原件](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/sources/SN74LVC2G125.pdf)，DCT/SSOP-8 版本 |
| ZDT TTL 主接口 | 项目厂商包中 X42S 用户手册 V1.0.5_260527，第 6 页输入供电，第 10 页相电流，第 12 页主接口，第 23/26 页串口设置和接线；电源与 TTL 已包含在同一 8 位主接口，故不需要 J24 |
| MaixCAM2 | [官方原理图](https://dl.sipeed.com/fileList/MaixCAM/MaixCAM2/Hardware/board_maixcam2/SCH_MaixCam2_379C.pdf)，第 7/9/10 页六针、五伏相通及充电；本地原件在 `Documentation/摄像头与视觉板` |

未重新核价或查库存；没有下单。为满足用户手工焊接和继续保留防反灌，采用 SOIC 保护开关加 TSOT 控制器及外部 MOS，器件数多于原 QFN 一体方案。

## 计算与额定边界

U10 设置采用两组独立分压，0.1% 电阻：

- 欠压典型 `1.39 × (1 + 33.6/15) = 4.5036V`。
- 过压典型 `1.39 × (1 + 41.2/15) = 5.2079V`。计入基准 1.35～1.43V、0.1% 电阻和 ±100nA 漏电，约为 **5.046～5.370V**。这是稳态门限计算，不含瞬态响应、寄生和电容容差，也不是输出钳位保证。
- `R72=215k`，按手册公式 `0.0103 × 215 + 0.017 = 2.2315A`，仅为典型限流值。手册 ±5% 的标题值只在 1A/25℃规定，未给 215k 档完整最小/最大表，不能把它当保证二安最低门限。
- `C73=12nF`，`195000 × 5 × 12nF ≈ 11.7ms` 典型启动斜坡。U10 的过流持续超过典型 2ms 后进入自动重试，热故障也按器件规定重试；不再沿用旧 TPS259470L 的热锁断描述。

SGM2531C 有 OUT→IN 体二极管，单独使用不能防反灌。Q5 采用漏极在输入、源极在摄像头侧的方向，LTC4412 用输入 VIN 和输出 SENSE 控栅，才构成低损耗理想二极管。正常调节压差典型 20mV，电流较大时还受 MOS 导通电阻限制。反向阻断有动态响应和漏电，未作瞬态实测。

U10 SOIC 典型导通电阻约 60mΩ，手册上限为 110mΩ；二安时 U10 压降估计典型 0.12V、按上限 0.22V，另有 Q5、连接器和线缆压降。必须实测 Maix 端负载电压、充电和启动，未宣称可无条件持续二安。Maix 内部 1.5A PTC 保留，外部预算不提高其额定。

整车输入预算按左右轮 4A、步进 3A、Core 五伏 0.75A、外设五伏 2.5A、假定效率 85%：

`Ibat = 4 + 3 + 5 × (0.75 + 2.5) / (Vbat × 0.85)`。

| 输入电压 | 电池端设计预算 |
| --- | --- |
| 10V | 8.9118A |
| 12V | 8.5931A |
| 12.6V | 8.5173A |

F1 15A 按此前 75% 连续降额为 11.25A，高于上述预算；F2 保持 4A 延时。此计算不代替保险温度曲线、短路分断、启动浪涌和线束核算。新增 Q6/Q7 在三安、每颗 10mΩ 上限条件下冷态合计损耗约 `2 × 3² × 0.01 = 0.18W`；实际温度升高后电阻、开关过程 SOA 和过流能力须验证。

## 实际执行结果

| 检查 | 结果 |
| --- | --- |
| 原生网表 | 161 个网络，完整引脚分组与独立增量意图一致 |
| KiCad ERC | 0 错误、0 警告，无 ERC 排除项 |
| 项目检查 | 2165 项通过，含 98 触点、复用功能、UUID、既有电路、默认断电与串口隔离连接 |
| 九页 PDF | 已逐页渲染并审阅；导线/文字交叠 0、文字/文字交叠 0；图体和标题栏另作目视核查 |
| 元件资源 | 150 个实体位号含两个机械孔，147 个有可解析模型；148 个电气 BOM 草表位号 |
| 0603 | 当前 92 个实例，保持 0.85mm 内侧间隙 |
| 本地封装库 | 49 个封装原生 SVG 导出；属于库资源检查，不是 PCB DRC |
| 检查器边界探测 | 保存顺序与显式默认字段可规范化；改变输入脚号、引脚坐标仍被判为不同 |
| PCB、固件与实物 | 本轮未修改或验证；PCB 等用户完成后另行检查 |

模型中 F1/F2、L2、SW1、SW2～SW5、U10 共九个为通用替代，J1 准确卧式模型仍未绑定。U10 焊盘按原厂图绘制，绑定的 SOIC 外形没有准确 EP 几何，未冒充厂商精确装配模型。SOIC 外露引脚更便于焊接，但底部散热焊盘必须接地并可靠焊接。TFT 孔距和无牌按键焊脚跨距仍须实物复核。

检查器保留历史电气合同，再加 M2.6 显式变更；不会重生成用户图纸。用户保存为 KiCad 原生格式后，部分符号字段顺序和显式默认属性不同，旧检查器在开始本轮前已因此抛错，旧 `verification.json` 不能当作本轮运行成功。已只对这类无电气含义差异做规范化，项目设置按用户本轮基线逐字节检查。

初次增量检查发现过悬空 VM 标签、D5 缓存库差异，以及网表核查发现的稳压管跨线、未连接栅极交点、排针地线触及空脚；均已修正。真实失败与处理见 [检查历史](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/m26_verification_history.json)，最终结果见 [verification.json](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/verification.json)、[ERC](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/erc.json)、[图面检查](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/visual_check.json)。

## 复现命令

以下命令在仓库根目录执行，路径前缀为 `Hardware/stm32c5a3/Hardware/CoralC5-Motor`。本机 CLI 为 `C:/Program Files/KiCad/10.0/bin/kicad-cli.exe`；Python 渲染依赖在 `.scratch/m2/pdf-deps`。

```powershell
$motor = 'Hardware/stm32c5a3/Hardware/CoralC5-Motor'
& 'C:/Program Files/KiCad/10.0/bin/kicad-cli.exe' sch export netlist --format kicadxml --output "$motor/docs/netlist.xml" "$motor/CoralC5-Motor.kicad_sch"
& 'C:/Program Files/KiCad/10.0/bin/kicad-cli.exe' sch erc --format json --severity-all --exit-code-violations --output "$motor/docs/erc.json" "$motor/CoralC5-Motor.kicad_sch"
& 'C:/Users/sc/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' -B -X utf8 "$motor/tools/verify_schematic.py"
& 'C:/Program Files/KiCad/10.0/bin/kicad-cli.exe' sch export pdf --output "$motor/docs/CoralC5-Motor_Schematic.pdf" "$motor/CoralC5-Motor.kicad_sch"
$env:PYTHONPATH = (Resolve-Path -LiteralPath '.scratch/m2/pdf-deps').Path
& 'C:/Users/sc/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' -B -X utf8 "$motor/tools/check_visual.py"
& 'C:/Program Files/KiCad/10.0/bin/kicad-cli.exe' fp export svg --layers F.Cu,F.SilkS,F.Fab --output "$motor/docs/footprint-previews" "$motor/Motor.pretty"
```

本轮没有测量电流、温升、Maix 充电浪涌、USB 双源、ZDT 启动时序、关断残压及电机再生。对应验证完成前保持原理图草案状态。
