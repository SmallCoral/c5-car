# SmallCoral Logo

这里沿用 [SmallCoral/Coral-Wave](https://github.com/SmallCoral/Coral-Wave/blob/main/Images/coral-logo.svg) 里的珊瑚图案。原始 SVG 和随附的 MIT 许可一并保留，源文件的 Git blob 为 `f04b1f7ec4d30968befcd20e3db8e2c8f75e5ca9`。

彩色渐变不适合普通 PCB 丝印，板上使用透明区域的单色轮廓。转换时把原始 SVG 渲染到 1600×1600 像素，提取轮廓后以不超过 0.012mm 的偏差简化，得到珊瑚主体和六个气泡，共七个实心多边形。轮廓保存在 coral-logo-silkscreen.svg，宽高约为 11mm。

Core 的本地封装 `DevBoard:Logo_SmallCoral_11mm` 包含这组图形和下方的 `Designed by SmallCoral` 署名，可作为一组移动。它只有正面丝印，没有焊盘，也不进入 BOM 或贴片坐标文件。当前中心在 X 67.5、Y 58.5mm，名字采用 KiCad 默认字体，字高 0.9mm、笔画 0.15mm。

三个按键的说明放在正面丝印。SW1 对应 RESET，SW2 对应 BOOT0，SW5 对应 USER，用户按键接 PD2。添加丝印前后原有的走线、过孔、铜区、焊盘和器件位置逐项一致，KiCad 10.0.6 检查没有新增违规项。后续 USB-C 地焊盘圆角已调整，用户也更新了布线。2026 年 10 月 24 日复查 Core 为 0 个 DRC 错误、0 个警告、0 个未连接项，原理图与 PCB 一致性问题为 0。处理过程见 [PCB 复查记录](../../CoralC5-Core/docs/pcb_review.md)。
