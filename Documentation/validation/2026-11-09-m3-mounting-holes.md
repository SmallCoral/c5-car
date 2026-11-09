# 2026-11-09 四个板级安装孔改 M3

用户要求保留 LCD 固定柱的两个孔，只把其余四个螺丝孔改为 M3。
已将四个无原理图关联的板级孔改为 Ø3.2mm 非金属化通孔，保留孔中心、角度及全部 UUID。
新增本地封装 `Motor:MountingHole__MountingHole_3.2mm_M3`，取自本机 KiCad 10.0.8 的 `MountingHole_3.2mm_M3`；未重跑生成器。
同步使用其直径 6.4mm 的注释外形和直径 6.9mm 的装配边界。

| 孔中心 X/Y，mm | 修改前 | 修改后 |
| --- | --- | --- |
| 126.25 / 62.25 | Ø2.0 NPTH | Ø3.2 NPTH，M3 |
| 126.25 / 115.5 | Ø2.0 NPTH | Ø3.2 NPTH，M3 |
| 221.25 / 45.25 | Ø2.0 NPTH | Ø3.2 NPTH，M3 |
| 221.25 / 140.5 | Ø2.0 NPTH | Ø3.2 NPTH，M3 |
| 142.73 / 111.0、172.73 / 111.0 | LCD 两孔 Ø2.0，间距30mm | 原样保留 |

当前四个板级孔的位号都为用户原有的 H1，按 UUID 和坐标定位，没有重编号。
原 Ø2mm 本地库没有修改，LCD 两个封装保持原样。
原理图、项目约束、其他封装、元器件模型、545段走线及32个过孔均未改动。

## 验证

源码起点为 `8725275ab4f3aace2bfe87971e3c5ed062f33165`，工作区含此前未提交的修改。
修改前保存13个文件到 `.scratch/m3-holes-20261109/before/`，SHA256 清单为同目录上级的 `baseline.json`。
PCB 起始 SHA256 为 `ebf9593409df1450adedcb3f712c7182457a805a13dcb4a63acdb5dbbaf6d6d7`。

- 结构对比只允许四孔的封装名称、说明、Value、钻孔/孔图形及对应装配圆尺寸改变；PCB 其余全部字段与基线一致。
- KiCad pcbnew 10.0.8 原生加载确认安装孔径为 `2.0, 2.0, 3.2, 3.2, 3.2, 3.2` mm。
- 原生 Excellon NPTH 导出确认四个 Ø3.2mm 钻孔坐标与上表完全一致，LCD 对应两个 Ø2.0mm 钻孔保留。
- 原生 DRC 修改前后均为 **24 条违规、156 项未连接，仍未通过**；24条违规内容一致，忽略规则未改。
- 原理图没有变更，本轮未重跑 ERC；没有进行实物装配验证。

工具为 KiCad CLI/pcbnew 10.0.8、KiCad Python 3.11.5。
[检查数据](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/m3-holes-2026-11-09-check.json)保留逐孔 UUID 和前后 PCB 指纹。
[原生 DRC](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/m3-holes-2026-11-09-drc.json)与[核对用 NPTH 导出](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/m3-holes-2026-11-09-NPTH.drl)用于此次验证，不能据此认定整板已完成制板检查。

```powershell
$pcb = 'Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_pcb'
& 'D:/KiCad/10.0/bin/kicad-cli.exe' pcb drc --format json --output '.scratch/m3-holes-20261109/drc-after.json' $pcb
& 'D:/KiCad/10.0/bin/kicad-cli.exe' pcb export drill --output '.scratch/m3-holes-20261109/drill' --format excellon --excellon-separate-th --generate-report $pcb
```
