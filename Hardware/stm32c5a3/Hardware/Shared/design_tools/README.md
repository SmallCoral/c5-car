# 双板检查工具

当前设计以两套 KiCad 工程为准，直接在 KiCad 中修改。一次性拆分和初始摆放脚本已随旧工程归档，避免重新生成时覆盖后续修改。

verify_split.py 检查两套工程的元件归属、全部编号焊盘网络（含未连接脚）、金手指、两组排针、供电与机械接点，并更新 BOM 和检查记录。已开始布线的板还检查铜线和实际铜区的网络是否存在于原理图，规则禁布区不当作未命名铜区。主动移除的历史元件在各板 design_manifest.json 的 retired_references 中注明原因；后来添加又取消的板上特征放在 removed_board_features，目前为 Core 的 TP1 至 TP4。其余原始元件仍须保留。USB 检查确认 U3 经 R3、R4 接主控，原理图和 PCB 的两个阻值均为 0R。失败项会完整写入报告，脚本返回非零状态，不会沿用旧的通过记录。sexpr.py 负责解析 KiCad 文件，pin_functions.py 从本地 DS15137 文本读取复用功能。

接口按 C5-EDGE-3 的实际 MCU 焊盘坐标检查：下半信号对应正面 B 排，上半信号对应背面 A 排，各排从左到右排序。铜面以实际层集合判断，不能使用边缘连接器焊盘的 GetLayer() 返回值。两板接口合同、排针表中的金手指触点号也必须一致。PH0、PH1 只检查本板 HSE 回路，A13、A14 和 J9 的 33、34 脚须明确不连接；其余 84 路 GPIO 直接引出。

先用 KiCad CLI 对两套主原理图执行 ERC，并导出 kicadxml 格式的 docs/netlist.xml，再对 PCB 执行带 schematic-parity 的 JSON DRC。随后用 KiCad 10 自带的 Python 执行下面的检查。

```powershell
& "D:\KiCad\10.0\bin\python.exe" Hardware/Shared/design_tools/verify_split.py
```

检查脚本对应 B1 设计进行中的连通性检查，允许文档中列出的五处封装孔距问题，未连接项和布线过程中的悬空线端在原生 DRC 报告中保留，悬空线端不计作器件摆放碰撞，不能作为制板放行检查。后续调整电路后，应同步修改对应功能连接条件。原单板的 115 个器件位号基线和 PCB 哈希保存在 Shared/legacy_baseline.json，归档时已经核对旧 PCB 内容。Python 示例路径应按本机 KiCad 安装位置调整。
