# Transfer 电源输入转接板

用于修正此前电源输入正负极画反的问题，通过 XT60PW 公母接口转接输入极性；用途由用户于 2026-11-16 确认。它与 Core/Motor 主控制板分开维护。用 KiCad 10 打开 [Transfer.kicad_pro](Transfer.kicad_pro)，本地符号、封装、模型和库表需一同保留。

| 内容 | 入口 |
| --- | --- |
| 编辑设计 | [原理图](Transfer.kicad_sch)、[PCB](Transfer.kicad_pcb) |
| 查看图纸 | [PDF](docs/Transfer.pdf)、[预览](docs/Transfer-preview.png) |
| 库和模型来源 | [模型说明](Models/README.md)、[资源清单](docs/xt60-resources.json) |
| 当前原理图检查 | [2026-11-16 网表](docs/handoff-2026-11-16/Transfer.net.xml)、[ERC](docs/handoff-2026-11-16/Transfer.erc.json)、[资源检查结果](docs/handoff-2026-11-16/verification.json) |
| 初稿检查 | [初稿 ERC](docs/Transfer.erc.json)、[初稿资源核查](docs/verification.json)、[几何核查](docs/model-geometry-check.json) |

2026-11-16 原样收录手工修改后的原理图。
新导出的原生网表确认 `J1:1 ↔ J2:2`、`J1:2 ↔ J2:1`，KiCad 10.0.6 ERC 为 0 错误、0 警告。
当前 PCB 有两个封装、没有走线或过孔；PCB 同步、布线、DRC 和实物极性尚未验证。
上表 PDF、预览和初稿报告属于旧快照，不用于判断当前导线连接。

现有 `tools/verify_schematic.py` 仍检查“只放置、不连线、恰好四个未连接引脚”的初稿合同，因此本轮返回 1。
28 项中 4 项失败：不应有导线、J1/J2 嵌入符号与本地库不一致、应恰有四个未连接引脚。
其中符号一致性仍需核对，不能因 ERC 通过而忽略；[本轮记录](https://github.com/SmallCoral/c5-car/blob/e631797d99cd60486eccc874f66cc99cdf3f7496/Documentation/validation/2026-11-16-rust-handoff.md)保留检查范围。

J1 为 XT60PW-M（C98732），J2 为 XT60PW-F（C428722）；1 脚负极、2 脚正极是本地 KiCad 库编号约定。
采购库存尚未确认，不列为已落实生产选型。
本轮项目整理仅纳入已有设计文件并做只读检查，没有修改连接、封装或模型。
