# XT60PW 模型与封装来源

两份 STEP 均来自 **嘉立创EDA/EasyEDA官方库**：[嘉立创EDA](https://lceda.cn/)、[EasyEDA](https://easyeda.com)。保留原始几何与文件内容，仅在封装中设置偏移和旋转。模型来源 URL、SHA-256 及变换见 [资源记录](../docs/xt60-resources.json)。

| 位号 | 型号 | 立创编号 | KiCad 模型偏移 X/Y/Z，mm | KiCad 旋转 X/Y/Z，° |
| --- | --- | --- | --- | --- |
| J1 | XT60PW-M，卧式公头 | C98732 | 3.6 / 7.1 / 0 | 0 / 0 / 90 |
| J2 | XT60PW-F，卧式母头 | C428722 | -3.6 / 6.8 / 0 | 0 / 0 / 0 |

符号由 KiCad 10.0.6 的 `Connector:Conn_01x02_Pin` / `Conn_01x02_Socket` 派生，显式标注极性；封装由同版本 `Connector_AMASS` 的对应卧式条目复制。沿用其 **1 脚负极、2 脚正极** 的编号约定；两片固定脚使用无编号焊盘。原厂图纸只标极性，编号是本地库约定。保留 KiCad 库原有许可及元件库使用例外，来源为 [KiCad 符号库](https://gitlab.com/kicad/libraries/kicad-symbols) 与 [KiCad 封装库](https://gitlab.com/kicad/libraries/kicad-footprints)。

依据 AMASS《XT60PW SPEC》V1.2，第 1 页母头、第 2 页公头：电源脚间距 7.2 mm、推荐孔径 2.7 mm、固定脚间距 13.5 mm，固定槽 0.6 × 1.7 mm。随工程保存于 `docs/sources/AMASS_XT60PW_SPEC_V1.2.pdf`。[原厂规格书内容，分销商托管](https://www.triopak.fi/files/product/Amass/XT60PW%20SPEC.pdf)。

2026-11-14 使用 Gmsh 4.15.2 / OpenCASCADE 直接导入两份 STEP，并核对圆柱电源脚中心，变换后与对应焊盘中心的偏差均小于 0.001 mm。检查没有生成板文件。公头参考模型外形约 18.1 × 15.5 × 8.1 mm，V1.2 图纸为 18.2 × 15.5 × 8.4 mm；模型不作为精密干涉和加工尺寸依据。母头模型外形约 15.5 × 17.2 × 8.4 mm。详见 [几何检查记录](../docs/model-geometry-check.json)；尚未进行实物配合验证。

原理图仅放置两个元件，没有接线，ERC 保留 4 个 `pin_not_connected` 错误，未通过电气连线检查。封装、模型和原生网表的资源核查通过；原始 ERC、网表、PDF 与验证结果保存在 `docs`。

2026-11-14 已核对型号与立创编号，国内商城页面访问被拦截，**未确认国内自营现货数量**；采购状态为候选，不作为生产 BOM 或已落实供货结论。
