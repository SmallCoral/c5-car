# 设计资料


优先采用用户资料包内的主控手册和勘误，参考模块只用于结构和器件选择。关键外围取值已按原厂手册修正。以下 PDF 已存于 本目录。DS15137 另保留文本提取结果，供引脚与复用检查脚本使用。

| 资料 | 版本、定位与用途 |
| --- | --- |
| ST DS15137 | 用户提供 Rev3，2026-08；Table13、Table14，电源章节及封装章节。完整 100 脚表独立解析比对；在线资料入口 [ST 文档页](https://www.st.com/content/st_com/en/technical-documents/stm32c5a3cg.html) |
| ST ES0677 | 用户提供 Rev1，2026-02；§2.2.2、§2.2.3，PC13/LSE 与最低驱动档限制 |
| [LAN8720A/AI](https://ww1.microchip.com/downloads/aemDocuments/documents/UNG/ProductDocuments/DataSheets/LAN8720A-LAN8720Ai-Data-Sheet-DS00002165.pdf) | DS00002165C，2024；引脚表、§3.7/§3.8，VDDCR、RBIAS、模式采样、LED 极性与外部时钟 |
| [ESP32-C3-MINI-1/1U](https://documentation.espressif.com/esp32-c3-mini-1_datasheet_en.pdf) | v2.2，2026-05-06；p11 引脚、p12-15 启动、p34 应用电路、p36-37 外置天线 |
| [ESP-AT 连接说明](https://docs.espressif.com/projects/esp-at/en/latest/esp32c3/Get_Started/Hardware_connection.html) | 默认 GPIO6/7 AT UART、GPIO20/21 下载 UART、GPIO4/5 流控；访问于 2026-10-18 |
| [Espressif KiCad 库](https://github.com/espressif/kicad-libraries) | MINI-1U 官方封装和 STEP，本地保存并改为项目相对模型路径 |
| [AP63200/01/03/05](https://www.diodes.com/assets/Datasheets/AP63200-AP63201-AP63203-AP63205.pdf) | DS41326 Rev3-2，2024-11；固定输出反馈接法、引脚和外围要求 |
| [AP63203 EVM](https://www.diodes.com/assets/Evaluation-Boards/AP63203WU-EVM-User-Guide.pdf) | p4-5，固定输出参考电路，6.8uH 与两颗 22uF 输出电容 |
| [SRN6045TA](https://www.bourns.com/docs/product-datasheets/srn6045ta.pdf) | p1，6R8M 的额定和饱和电流 |
| [ABM8](https://abracon.com/Resonators/abm8.pdf)、[ABS07](https://abracon.com/Resonators/ABS07.pdf)、[ASE](https://abracon.com/Oscillators/ASEseries.pdf) | 晶体负载选项、封装与 50MHz 振荡器供电、引脚及旁路要求 |
| [HanRun HR911105A 原厂页](https://www.hanrun.com/rj45_100/154.html) | 本地原厂 A/2 图纸 p1 电气连接、p2 外形；[原厂图纸镜像](https://xonstorage.z8.web.core.windows.net/pdf/hanrun_hr911105a_apr22_xonlink.pdf) |
| [GCT USB4105](https://gct.co/files/specs/usb4105-spec.pdf) | A3，2023-02-27；USB-C 连接器型号与尺寸 |
| [JST XH](https://www.jst-mfg.com/product/index.php?lang=2&series=277) | B4B-XH-A 四针直插座，2.50mm 间距，针跨距 7.50mm，壳体宽 12.40mm；与 KiCad 库内对应符号脚号、封装和 STEP 核对 |
| [Hirose DM3AT-SF-PEJM5](https://www.hirose.com/en/product/p/CL0609-0031-0-00) | 官方产品页及 KiCad 对应卡座符号、封装、模型；官方二维图直链返回 403，未将其列为已下载资料 |

本机 `pcb-model` 中的 LAN8720A/HR911105A、microSD 和 IPEX 示例用于交叉参考。PHY 的 12.1k 偏置、1uF+470pF 内核去耦、外部 RMII 时钟和无线模块自带射频接口均以本次所用芯片的原厂资料为准。

2026 年 10 月 23 日晚的 USB-C 孔距修订另核对了在线 [USB4105 机械图 B4，2023-12-18](https://gct.co/files/drawings/usb4105.pdf)。定位孔仍为 0.65mm，原孔位及 0.60×1.15mm 地焊盘外尺寸保留；本项目将这两处地焊盘的圆角半径增至 0.30mm，孔到铜约为 0.233mm。具体修改和当前 DRC 结果见 [Core PCB 复查记录](../../CoralC5-Core/docs/pcb_review.md)。

