# Coral-C5 下一阶段提示词

```text
请在 D:/dev/qrs/stm32/c5-car 接续校赛A题项目。先读根AGENTS.md、README.md、.agent/main/handoff.md及Documentation/validation/2026-11-07-m28-resume.md，核对实时工作区。

先阅读 Documentation/validation/2026-11-08-f8-association.md。已修复左右轮48个丢失的path；当前180个封装按UUID唯一对应、预计F8新增0，用户最新布局和151走线/3过孔保留，当前DRC229违规/390未连接。新的保留基线是docs/baseline-2026-11-08-f8-association-71d22540；GUI尚未操作复测。下述为此前M2.8交付历史快照，33件暂存及40条DRC不再是当前布局状态。

M2.8此前本地交付已完成：十页原理图、205网络、98触点、40GPIO核对，ERC0错误0警告；22种新增/修订SKU有国内现货快照，178位号草表；PCB180封装、33个新增件在右侧待布局区，一致性0，原151走线/3过孔保留。DRC仍40条违规、390未连接，没有Edge.Cuts，不是制板版。

保留荔枝派4A、MaixCAM2与Action4，12V/12.5A整车输入，双AP64500 5.088V A/B轨4.2A/4.0A预算；Core隔离、Maix六针保护、ZDT TTL及PD12默认断电、轮六针和TFT双孔均保留。荔枝派J25六触点Type-C电源、J18原板2×6 UART1，PD8/PD9 Core USART3。C63/C89已470pF，外部补偿15.8k+10nF待测。

请按我本次给定的下一阶段范围继续。若进行PCB布局/布线，先保存当前实时新基线，再结合机械外形处理33个暂存件与U1/双电源走线，保留未受影响的UUID、摆放和铜线。不要直接沿用旧脚本重跑，不用CUA，不改旧Coral-C5、Core/Expansion或98触点合同。原理图、PCB一致性、DRC和实物验证分别记录，保留真实失败。新选型仍需国内立创直接现货证据；不把候选写入生产BOM。

本提示词不要求推送、关机或整车固件开发；需要的具体下一项工作由我当前消息指定。不要重复已完成审计冒充新进展，也不要用旧M2.7结果代替M2.8。
```
