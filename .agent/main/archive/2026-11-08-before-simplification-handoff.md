# Coral-C5 M2.8 本地交付交接

## 2026-11-08 F8问题修复后的最新状态

左右轮48个PCB封装丢失原理图path，已只补齐关联。最新原理图导出180件，PCB180件，默认UUID匹配唯一对应，预计新增0、替换0；值/焊盘网不匹配0，原生一致性0。用户在此前交付后保存了新的布局与工程设置；以`docs/baseline-2026-11-08-f8-association-71d22540/`新基线为准，不能用下方旧布局覆盖。所有当前封装UUID/摆放、151走线与3过孔保留，输出PCB SHA256 `ba073cc304ebe87265242cb90751b266800e45346027e4b35721cdc8af43f8e7`。当前DRC229违规/390未连接，旧40违规已不是最新数量。

当前入口`tools/verify_pcb_sync.py`调用新的F8关联预检，会原生导出最新原理图后检查；旧元数据生成器已保护，不能从旧报告再生成当前状态。证据见 [F8验证](../../Documentation/validation/2026-11-08-f8-association.md)。GUI F8尚未操作验证；如KiCad仍开着，保留未保存布局副本后重新载入磁盘文件。软件、机械及生产验证仍未完成。

## 此前交付快照（保留历史）


更新日期：2026-11-08（设计与采购快照日期2026-11-07）。工作目录：`D:/dev/qrs/stm32/c5-car`，分支 `main`，基于 `d79ffc78ad5093452b1b1b852f7de347f0cc084b` 的未提交本地修改。本轮用户已明确要求依据交接继续完成，先前暂停状态已经解除。

## 已完成范围

荔枝派4A、MaixCAM2、Action4三者保留，整车12V/12.5A适配器。两颗AP64500提供A/B两轨5.088V，预算4.2A/4.0A，Core隔离、Maix保护、步进PD12默认断电、六针轮口及TFT双孔保留。荔枝派J25是六触点Type-C电源，J18是原板2×6针序；UART1接Core USART3 PD8/PD9，11脚收3.3V、12脚发1.8V。软件分工未实现。

| 项目 | 实际结果 |
| --- | --- |
| 原理图 | 十页、180实体位号含两孔、205网络；独立 M2.8 合同、98触点、40GPIO及资源审计 3786 项通过 |
| ERC | KiCad10.0.8，全部严重度，0错误0警告；10个原局部GND标注改全局标注，UUID/位置与网络保留，标签方向修正 |
| 补偿 | C63/C89从39pF改470pF/50V C0G；15.8k+10nF保留，原厂公式与有效电容/ESR假设见电源计算；未测稳定性 |
| PDF | 最终十页逐页查看；自动导线/文字与文字/文字交叠均0，说明文字也已同步470pF |
| 采购/BOM | 22种新增或修订SKU均有国内独立现货与本地仓快照；178电气位号草表，旧料未全表重审，未下单 |
| PCB同步 | 180封装，新增33个在现有布局右侧待布局区；U1改SO8EP，原UUID/中心/角度保留；3815 项同步审计通过，原生一致性0 |
| 铜线与布局 | 原147封装身份/位置保留，U1除显式封装替换外不变；151段走线、3过孔及其它板级对象完全保留；无自动布局布线 |
| DRC | 40条违规、390项未连接，真实失败；原输入PCB无Edge.Cuts，未生成外形 |
| 实物/固件/机械 | 本轮均未执行；没有可运行整车或制板发布结论 |

## 依据与入口

- 当前状态从 [根README](../../README.md) 进入，详细事实与命令在 [M2.8验证](../../Documentation/validation/2026-11-07-m28-resume.md)。
- 当前 [接线表](../../Documentation/reference/m2-interfaces.md) 与 `docs/m2_pin_assignment.csv` 为接口入口；98触点合同仍是 Shared/interface.csv。
- Motor 当前快照在 `docs/current_design.json`，`verification.json`、`pcb_sync_verification.json`、`visual_check.json` 为最新结果。
- `tools/verify_schematic.py` 已优先分派 M2.8；历史 M2～M2.7 检查器保留。完整连接意图来自 `tools/m28_contract.py`，基于旧 M2.7 原生网表加静态修改，不能用当前观察网表反建合同。
- 采购 `docs/procurement_m28.json` 和 `docs/sources/m28-resume/`，BOM两个入口都明确是草表。

## 基线与保留约束

本次真实接手基线为 `Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/baseline-2026-11-07-m28-resume-local-ee068449/`。接手 PCB SHA256 为 `c8fe2600a7aec6cc1627188a5c0e4b1c9985cbafa91eda6d5d0ecd78e8bced77`，不能用旧会话的哈希替换。本次输出 PCB SHA256 为 `00a8f4df9f2de84503631e53f272d768f1cbecb6c0b6373d4bcd2ee2f693e43f`。`baseline.json` 记录原文件、Core/Expansion/共享接口和历史文件的真实哈希，保护项复查通过。

旧 `docs/baseline-2026-11-07-m28/` 仍为独立连接合同输入。其44个文本文件的历史哈希按CRLF生成，Git落地文件为LF；检查器明确验证CRLF表示，并单独固定旧原生网表原始SHA，未篡改历史清单。另有先前已提交的 `baseline-2026-11-07-m28-resume/`，不与本轮新基线混用。

旧交接与提示词原文在 [archive](archive/)，旧 `.scratch/m28/handoff-inventory.json` 本机不存在，不能依赖。`.scratch/m28-resume/` 的一次性编辑脚本已执行，不要重跑；正式只读验证工具在Motor/tools。不得重跑生成器、覆盖手工设计、改旧Coral-C5项目、使用CUA、自动布局布线、推送或关机。

## 下一阶段实际工作

当前授权的本地原理图审计、采购草表、PCB元件同步与文档交付已完成。后续是人工布局/布线和样机验证，按新任务范围继续：先保存新基线，结合实际板框和机械位置布局33个待摆件；重做U1开关/补偿/输入热回路及A/B电源铜线，不能仅改网络名掩盖旧铜线短路；逐项解决DRC后再上板。

需实测环路/阶跃、5A级温升、线损、共源启动、Type-C接入、UART掉电反灌、Maix识别加充电及Action4录像。输入连续预算约11.09A，主保险按75%降额仅余0.16A。C81/C92为NRND、L1/L2库存少，生产前重审。J1准确模型缺失，12个通用外形不能作供应商准确模型；按键焊脚/TFT孔距/三模块CAD仍待实物确认。
