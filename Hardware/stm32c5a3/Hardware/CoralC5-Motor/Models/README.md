# PCB 元器件模型来源

2026-11-09 补入的五个 STEP 文件均从网上下载，文件内容保持原样。
只在 PCB 和本地封装库中设置平移、旋转；比例均为 1，没有自行绘制、拉伸、裁切或生成器件模型。
原有模型文件保留，避免影响历史工程和旧脚本。

| 器件 | 文件来源 | 使用边界 |
| --- | --- | --- |
| J1 XT60PW-M | 嘉立创EDA/EasyEDA 官方库，C98732 | 对应料号；两电源脚及安装片位置已核对 |
| F1 0448015.MR | 嘉立创EDA/EasyEDA 官方库，C187551 | 对应料号；替换原 454 系列方块占位 |
| J19 ZX-MX1.25-6PWT | 嘉立创EDA/EasyEDA 官方库，C7430472 | 对应料号；信号焊脚对齐；安装片与当前焊盘的差异见验证记录 |
| SW1 SS12D10G5 | 嘉立创EDA/EasyEDA 官方库，DEALON C2998777 | G5 外形参考；不是 SHOU HAN 精确 CAD，脚距和高度存在小幅差异 |
| C1/C13/C33 EEU-FR1E471 | Nichicon 官方 10×12.5、P5 径向电解模型 | 同尺寸外形参考；不是 Panasonic 精确 CAD；保留原始未剪引线 |

嘉立创EDA/EasyEDA 官方库来源：[嘉立创EDA](https://lceda.cn/)、[EasyEDA](https://easyeda.com)。
Nichicon 来源：[官方 3D 模型库](https://www.nichicon.com/en-us/design-support/3d-model-library/?page=2)。
KiCad 原有 STEP 文件中的版权及许可证头保持原样。

逐文件下载链接、日期、SHA256、原始模型名称及绑定参数见 [来源清单](../docs/models-2026-11-09.json)。
尺寸差异、未取得的精确模型及独立 DRC 结果见 [验证记录](../../../../../Documentation/validation/2026-11-09-3d-models.md)。
模型显示完整不能替代焊盘核验或实物装配验证；本轮没有改变 BOM 型号。
