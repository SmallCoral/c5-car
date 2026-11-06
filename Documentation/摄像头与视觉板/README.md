# 视觉板原始参考资料

2026-11-06。当前分工为荔枝派 4A 识别、MaixCAM2 图传。原始资料用于核对接口，不代表软件和整车已验证。

- MaixCAM2：[官方硬件资料](https://wiki.sipeed.com/hardware/zh/maixcam/maixcam2.html)、[官方原理图](https://dl.sipeed.com/fileList/MaixCAM/MaixCAM2/Hardware/board_maixcam2/SCH_MaixCam2_379C.pdf)。本地 PDF 第 7 页核对 U56 六针接口，第 9 页核对五伏输入并联关系，第 10 页核对充电电路。
- 荔枝派：[外设说明](https://wiki.sipeed.com/hardware/zh/lichee/th1520/lpi4a/6_peripheral.html)、[供电 FAQ](https://wiki.sipeed.com/hardware/zh/lichee/th1520/lpi4a/12_faq.html)、[底板原理图](https://dl.sipeed.com/fileList/LICHEE/licheepi4a/02_Schematic/SIPEED_TH1520_EXPAND_LPI4A_51602_SCH.pdf)。本地 PDF 第 17 页（图签 GPIO Expansion 25/25）用于核对 J2500/J2501，不能将图签页码直接当 PDF 页码。
- `MaixCAM2_六针接口_用户实物.png` 是用户提供的照片。官方 IO 图来自 Sipeed Wiki 资源目录。
- EdgeMind-Robot 仅只读参考了 README、架构、主机识别说明与电源预算文档。未复制其代码、未修改该仓库，历史 FPGA 图像来源不等于本项目已经接通。

具体线束表见 [M2 接线表](../reference/m2-interfaces.md)，保护与验证见 [M2.5 记录](../validation/2026-11-06-m25-vision-power.md)。原厂资料版权及原有声明保留；未据公开下载推定其具有开源硬件许可。
