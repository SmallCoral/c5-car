# 原厂依据和资源记录

2026-11-07 M2.6 当前增量：荔枝派供电与接线已删除。U10 改为 [SGM2531C RevA.2](SGM2531C_RevA2.pdf)，第 3～5 页针序/电气参数、第 11～14 页保护/限流、第 15 页 SOIC EP 焊盘；U14 采用 [LTC4412](LTC4412_4412fb.pdf)，第 6～8 页针序、P-MOS 方向和工作原理。Q8 依据 [MMBT3904 DS30036 Rev27](MMBT3904_DS30036.pdf) 第 1～3 页。D5 依据 [Nexperia BZT52 原厂 PDF](https://assets.nexperia.com/documents/data-sheet/BZT52_SER.pdf) Rev2 的 1K/2A 及 C12 参数，在线已读，本地下载 HTTP403。Q5/Q6/Q7 复用 DMP3007；U15 复用 SN74LVC2G125DCTR。完整计算与限制见 [M2.6 验证](../../../../../../Documentation/validation/2026-11-07-m26-stepper-power.md)。以下旧轮次记录留作追溯，不是当前 BOM。

日期2026-10-27，资料版权归原权利人。库存和价格未作为本次交付结论。

| 资料 | 核对内容 |
| --- | --- |
| [TI DRV8874](https://www.ti.com/lit/ds/symlink/drv8874.pdf)，`DRV8874_SLVSF66A.pdf` | SLVSF66A，2019-12修订，后附订货表更新至2025。第3页脚序，第10页外接元件，第11页PH/EN，第12～14页IPROPI及IMODE，第20页典型应用，第39～41页PWP0016封装。 |
| [Diodes AP63205](https://www.diodes.com/datasheet/download/AP63200-AP63201-AP63203-AP63205.pdf)，`AP63205_DS41326.pdf` | DS41326 Rev3-2，2024-11，第2页典型固定5V电路及脚序，第4～5页额定与电气参数，后续选值及EN章节。 |
| [AP63205 EVB](https://www.diodes.com/assets/Evaluation-Boards/AP63205-EVB-User-Guide.pdf)，`AP63205_EVB.pdf` | 原厂EVB指南，4.7uH、输入10uF、输出2×22uF、自举100nF。 |
| [AOS AO4407A](https://aosmd.com/sites/default/files/res/datasheets/AO4407A.pdf)，`AO4407A.pdf` | Rev11.1，2024-03，第1页SOIC-8脚序与额定，第2页−6/−10V导通电阻，第5页外形。源1/2/3、栅4、漏5/6/7/8。 |
| [TI SN74LVC2G17](https://www.ti.com/lit/ds/symlink/sn74lvc2g17.pdf)，`SN74LVC2G17.pdf` | DCK/SC70-6版本，1A=1、1Y=6、2A=3、2Y=4、VCC=5、GND=2，施密特输入、供电范围及Ioff。 |
| [XKB DC-005-5A-2.0](https://lcsc.com/product-detail/DC-Power-Connector_XKB-Connection-DC-005-5A-2-0_C381116.html)，`DC005_XKB.pdf` | C381116，XKB原厂图A0、1/1，PCB TOP VIEW、1/2/3接点及5.5/2.1mm插头。厂家额定栏空白，采购确认具体5A批次。 |
| [SHOU HAN SS12D07VG4](https://atta.szlcsc.com/upload/public/pdf/source/20190925/C431546_8ABE7D3BFB98214862966F2A5D931037.pdf)，`SS12D07VG4.pdf` | C431546，第3页，0.3A/50V；8.8×4.6mm，信号脚2mm间距，壳体脚8.4mm间距、1.5×0.7mm槽，中脚公共端。 |
| [Bourns SRN6045TA](https://www.bourns.com/docs/product-datasheets/srn6045ta.pdf) | 4R7M=4.7uH，4.5A温升额定、6.8A饱和额定；已有共享PDF。 |
| [Panasonic EEUFR1E471](https://na.industrial.panasonic.com/products/capacitors/aluminum-electrolytic-capacitors/series/83367/model/83716) | 470uF/25V、D10、高12.5mm、脚距5mm，1290mArms高频纹波额定。 |
| [Murata输出电容](https://search.murata.co.jp/Ceramy/image/img/A01X/G101/ENG/GRM31CR71A226KE15-01.pdf) | GRM31CR71A226KE15L，1206、22uF、10V、X7R。 |
| [STM32C5Axxx复用功能](https://www.st.com/content/st_com/en/technical-documents/stm32c5a3cg_asset/chunk-010-topic-stm32c5xx-db-ds-alternate-functions.html) | 项目共享DS15137 Rev3及针脚合同，TIM2左编码器、TIM3右编码器、TIM4两路PWM，保留SWD。 |

电机参照 `D:/dev/EdgeMind-Robot/model/hardware/直流电机-MG513X（霍尔编码器）/` 及该项目 `doc/电机/拟用电机资料核对.md`。后者记录了实物MG513XP28_12V及WHEELTEC系列12V、0.36A额定、3.2A堵转、1:28、13PPR霍尔资料。原手册附有传播限制，未复制原件，实际线束和工作电流仍需核实。

用户指定的 [schematic-design skill](https://github.com/SmallCoral/schematic-design-skill/blob/main/SKILL.md) 快照为 `schematic-design_SKILL.md`。封装、符号和大部分通用模型来自本机KiCad 10安装库；母座沿用现有工程及共享Streamline来源。J1、SW1外形VRML由厂家尺寸建立并作为通用替代。第三方说明见项目根 `THIRD_PARTY.md` 与共享MIT许可。


## 2026-11-06 M2 实际增量

原 DC005 资料保留为 M1 历史依据，J1 当前已改 XT60PW-M。

| 资料 | 本轮用途 |
| --- | --- |
| [AMASS 原厂目录](AMASS_XT60PW.pdf)，2023 产品手册，第15页；[产品页](https://www.china-amass.com/biao/289.html) | 卧式 XT60PW，KiCad 对应7.2mm针距封装；准确卧式模型暂缺 |
| [AP63205 DS41326 Rev3](AP63205_DS41326.pdf)，2024-11，第2、13～14页 | U6 五伏，4.7uH、10uF、两颗22uF、100nF；芯片额定2A，设计预算1.5A |
| [AP2112 DS39724 Rev2](https://www.diodes.com/datasheet/download/AP2112.pdf)，2017-06，第1～3页 | U7 SOT25 引脚及1uF输入输出；实际预算150mA |
| [中景园原规格书](../../../../../../Documentation/中景园ZJY180S0800TG01技术资料/01-规格书与控制芯片手册/ZJY180S0800TG01.pdf)，Rev A，第5～8页 | 三伏三、八针定义、板载背光驱动、2mm孔与30mm短边孔距 |

原厂外设实物版本、主板线束方向与上游四安预算尚待实测。普通排针、SOT25、按钮及钻孔采用 KiCad10 本地库资源；模型路径使用项目相对引用，J1 没有绑定错误的立式模型。

## 2026-11-06 M2.3 后续修改

此前四安入口及普通排针描述属于初始 M2，当前方案已更新。
实际记录见 [M2.3 验证](../../../../../../Documentation/validation/2026-11-06-m23-power-connectors.md)。

| 资料 | 当前用途 |
| --- | --- |
| [DMP3007LK3 原厂 PDF](DMP3007LK3_DS40137.pdf)，DS40137 Rev2-2 | Q1/Q2，1=G、2=D、3=S，7mΩ/-10V 或 10mΩ/-4.5V，TO252 封装及热条件 |
| [SHOU HAN SS12D10G5](SS12D10G5_SHOUHAN.pdf)，2020-06-24 | SW1 三针、4.8mm 针距、12.7×6.7mm；仅作 MOS 栅极开关；通用外形模型明确标识 |
| [Littelfuse 452/454](https://www.littelfuse.com/assetdocs/fuse-452-and-454-datasheet?assetguid=8c87aa93-80a7-4ea8-8749-a19ba03901c2)，2025-10-16 | F1 12A 延时、F2 4A 延时，连续降额与 6.10×2.69mm 推荐焊盘；原厂在线 PDF 已读取，本地精确模型缺失，使用标识的包络模型 |
| [JST XH 原厂](https://www.jst-mfg.com/product/index.php?lang=2&series=277) | 普通 XH，非 XHB 卡扣款，2.50mm、3A/AWG22；TFT/蓝牙改为 2.54mm 排母 |
| KiCad 10 本地 0603 普通/手焊库 | 实算 R/C/LED 间隙 0.85/0.65/0.70mm，67 个实例统一为 0.85mm 内侧间隙与 0.975×0.95mm 焊盘 |


## M2.7 统一电源

2026-11-07 新增 `m27/TPS56837.pdf`（SLVSGM3B）、`m27/TPS25810.pdf`（SLVSCR1C）、`m27/MX1256.pdf`（Rev2.0）和 `m27/GCT_USB4125.pdf`（A2）。用于核对芯片脚位、固定五伏 Type-C 源端、六针座 1A 额定及推荐焊盘；来源 URL 和 SHA 分别见本目录 JSON。原厂版权及许可保留。解释与未验证项见项目 M2.7 验证记录。
