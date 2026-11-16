# Transfer 电源输入转接板

用于修正此前电源输入正负极画反的问题，通过 XT60PW 公母接口转接输入极性；用途由用户于 2026-11-16 确认。它与 Core/Motor 主控制板分开维护。用 KiCad 10 打开 [Transfer.kicad_pro](Transfer.kicad_pro)，本地符号、封装、模型和库表需一同保留。

| 内容 | 入口 |
| --- | --- |
| 编辑设计 | [原理图](Transfer.kicad_sch)、[PCB](Transfer.kicad_pcb) |
| 查看图纸 | [PDF](docs/Transfer.pdf)、[预览](docs/Transfer-preview.png) |
| 库和模型来源 | [模型说明](Models/README.md)、[资源清单](docs/xt60-resources.json) |
| 既有检查 | [原生 ERC](docs/Transfer.erc.json)、[资源核查](docs/verification.json)、[几何核查](docs/model-geometry-check.json) |

仓库中的 2026-11-14 设计文件只放置 J1/J2，没有连线，ERC 保留 4 个未连接引脚错误；资源核查通过不代表电气 ERC 通过。当前 PCB 有两个封装、没有走线或过孔。该文件快照尚未体现极性修正连线，未完成布线和 DRC 验证，也未做实物配合与极性验证。

J1 为 XT60PW-M（C98732），J2 为 XT60PW-F（C428722）；1 脚负极、2 脚正极是本地 KiCad 库编号约定。采购库存尚未确认，不列为已落实生产选型。本轮项目整理仅纳入已有设计文件，没有修改连接、封装或模型。
