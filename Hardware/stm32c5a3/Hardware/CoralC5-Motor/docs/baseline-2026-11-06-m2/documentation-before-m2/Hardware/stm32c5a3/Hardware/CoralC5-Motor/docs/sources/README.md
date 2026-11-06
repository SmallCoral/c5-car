# 原厂依据和资源记录

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
