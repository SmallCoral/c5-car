# 2026-11-05 小车机械框架记录

源码基点：`0b4d74d15b1d01e5a56c54c186ac4865f6cf75db`，在用户已有未提交修改之上工作，未提交或回退硬件设计。
本轮目标是自行设计小车框架，复用指定编码轮组、万向轮及用户 STEP 指出的电池定位框；扩展板机械规划 99×99 mm，补齐双摄支架和摆杆执行机构。

## 工具、来源和基线

- FreeCAD 1.1.4，Git `4fd3bf320d9566a27e60069fc8387448aaa3a094`，配套 Python 3.11。
- KiCad 10.0.6 导出当前 PCB 参考，Python 3.12.14/NumPy 导出 GLB 和离线页面，Chrome/Playwright 查看。实际浏览器和 Node 版本保存在 `viewer-check.json`。
- 输入基线保存在 `C:/Users/sc/.codex/visualizations/2026/11/03/01a1a08e-6411-7c12-ae04-d49bee901009/tmp/car-3d-framework/baseline.json`。最终逐文件核对 Hardware 和 Firmware；机械参数与说明是本轮有意修改。
- 最终基线检查发现 Motor 原理图、PCB、编辑状态、锁文件及自动历史发生变化，因此**原始输入基线检查未通过**。本轮 CAD 脚本仅只读原硬件文件，不回退这些新保存；电控参考以 `pcb-reference-report.json` 中记录的导出哈希为准，不能报告为最新 PCB 同步完成。
- 参考来源及哈希见 [来源说明](../../Mechanical/reference/README.md)、[轮组清单](../../Mechanical/reference/reused-hardware/manifest.json)、[电池框提取记录](../../Mechanical/reference/reused-hardware/battery-bay-source.json)。EdgeMind、旧项目及用户下载 STEP 未写回。

## 交付与检查命令

原生总装、STEP、GLB、离线页面及视图截图见 [Mechanical](../../Mechanical/README.md)。完整重建顺序及本机运行时路径也在该入口维护。

```powershell
& 'C:/Program Files/KiCad/10.0/bin/python.exe' -X utf8 Mechanical/source/prepare_pcb_references.py
& $fcPython -X utf8 -u Mechanical/source/extract_reference_hardware.py
& $fcPython -X utf8 -u Mechanical/source/extract_battery_bay.py
& $fcPython -X utf8 -u Mechanical/source/build_model.py
& $fcPython -X utf8 -u Mechanical/source/optimize_preview.py
& $fcPython -X utf8 -u Mechanical/source/refresh_motion.py
& $previewPython -X utf8 Mechanical/source/export_preview.py
& $fcPython -X utf8 -u Mechanical/source/verify_model.py
& $previewNode Mechanical/source/verify_viewer.cjs
& $previewPython -X utf8 Mechanical/source/verify_delivery.py --baseline 'C:/Users/sc/.codex/visualizations/2026/11/03/01a1a08e-6411-7c12-ae04-d49bee901009/tmp/car-3d-framework/baseline.json'
```

变量定义见机械入口的命令块。参考提取仅更新派生文件；PCB 规划副本在临时目录内生成并销毁。额外通过 Word COM 的只读方式读取了原始 `AC题清单.doc`，确认底板限制为长度≤350 mm、宽度≤250 mm，文件没有保存写回。

| 检查范围 | 原始结果文件 |
| --- | --- |
| 原生实体有效性、表达式、参考轮胎修复 | [model-build.json](../../Mechanical/validation/model-build.json) |
| 保存后重新打开、±6°闭合、最低高度、列出的关键干涉和柱顶齐平 | [mechanical-check.json](../../Mechanical/validation/mechanical-check.json) |
| 离线页面装载、实际连杆端点、分组隐藏、动画、截图及 JS 错误 | [viewer-check.json](../../Mechanical/validation/viewer-check.json) |
| 原始 Hardware/Firmware 哈希、GLB 结构和索引、文档入口与产物哈希 | [delivery-check.json](../../Mechanical/validation/delivery-check.json) |
| PCB 参考导出退出码、53 个范围内封装与 22 个范围外封装 | [pcb-reference-report.json](../../Mechanical/validation/pcb-reference-report.json) |
| 预览简化参数及三角形数量 | [preview-mesh-reduction.json](../../Mechanical/validation/preview-mesh-reduction.json) |

最终原生检查通过：142 个模型部件 BRep 有效，−6°、0°、+6°的连杆闭合误差均小于 10⁻⁶ mm，最低自由端基准高于底板 60.73 mm；列出的关键结构对未发现正体积干涉，10 项柱顶检查通过。零角度总包络约 287.5×236.04×449.78 mm，底板为 245×180 mm。

网页检查通过：Chrome 156.0.8078.80、Node v24.22.1，142 部件、三个姿态的接点对齐、实际分组隐藏及动画均通过，未出现 JavaScript 错误。GLB 格式与索引检查通过；输入基线一致性因前述 Motor 新保存变化未通过，整体 `delivery-check.json` 如实保留 `pass: false`。

## 保留的失败与处理

Core STEP 导出返回 **2**，3 次提示 VRML 无法用于非网格导出。实体文件已生成，3 个对应外形缺失；没有改报为完整成功。Motor99 临时参考导出返回 0。原始导出日志保留在机械验证目录。

参考 `RearRWheel` 最初 BRep 无效，使用 OCC 容差/拓扑修复，最大容差 0.001 mm；修复前后体积同为 44133.40741890523 mm³，原始 BRep 不改写。最初检查脚本的包围盒相交调用触发 `FloatingPointError: Invalid bounding box`，已改为精确包围盒的逐轴区间判断，再执行实体交集；该脚本异常不作为干涉通过。

保存后检查还发现 `HingeX` 使用不允许负数的 Length 属性，−115 mm 被截为 0，曲柄计算产生 NaN。已在构建源代码中改用 Distance 属性，并用 `refresh_motion.py` 更新本轮原生文件、STEP 和网格位置；新增有限数检查，拒绝将 NaN 视为闭合通过。早期网页验证因此失败，最终结果以修复后的记录为准。

布局复查时调整了电池和电控位置、加强筋及步进座避让；连杆按侧向错层建立，叉耳、销轴和轴孔分开建模。电机参考支架原本即与安装面齐平，记录的去除材料量为 0；万向轮新隔柱不带顶部凸段。

MaixCAM 官方外壳模型下载入口返回 **403**，未取得整机 CAD。相机夹持包络是本轮布局假设，官方型号及镜头参数来源见 [双摄资料](../reference/cameras.md)。

## 验证边界

当前仅验证已列结构对的几何间隙和三个姿态的运动闭合，没有执行完整机械 DRC、有限元、质量重心、振动、轴承寿命、打印或加工公差及实物装配检查。相机覆盖计算以官方镜头视场角、图像长边沿球槽、无裁剪取流为条件，不代表已经识别或图传成功。

电池 25 mm 高、MaixCAM/OV2640 模块包络、42 步进外形及孔距均需实物核对。没有导出可打印 STL、加工 DXF 或生产机械 BOM。

本轮未修改原理图和 PCB，因此未重跑 ERC、网表合同或 PCB DRC；数字机械检查不能代替这些电气检查，也不能代替实物验证。TFT、按钮及四脚 HC-08 继续只记录需求和机械空间。
