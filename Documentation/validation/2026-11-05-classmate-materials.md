# 2026-11-05 同学参考资料整理记录

源码基点：`0b4d74d`，分支 `main`。
资料提供来源由本次对话确认：同学的历史代码及配套资料。
开始时 Motor PCB、项目设置和两份 `.history` 文件已有未提交修改。
本轮比较基线采用开始时实际文件的 SHA-256，不将它们与 HEAD 的已有差异视为本轮修改。

## 整理范围

- 为电赛案例、Emm 资料和步进片段分别添加阅读索引。
- 新建资料总索引、平衡球控制解读与原始文件/压缩包应用源码清单。
- 在根 README 和文档入口添加链接。
- 保留原目录、文件名、源码字节和全部归档。
- 标明 Emm 副本关系、旧平台差异、源码片段问题和未验证项。

未移动或删除重复资料，未重跑硬件生成器，未修改当前硬件、固件或机械方案。

## 工具与命令

工具为 Git 2.53.0.windows.3、PowerShell 7.6.5、Codex 捆绑 Python 3.12.14、pypdf 6.10.0、Python 标准库 `zipfile`、`hashlib`、`csv`、`sqlite3`，以及 pypdfium2。
日期使用 Asia/Shanghai 的 2026-11-05。
文档规则采用 `mthines/agent-skills` 固定提交 `2b8d75d325c4816c4c1f5f3e982f682442b79c5d` 的 `docs update`。
当前机器缺少历史记录中的安装路径，本轮从相同提交取得完整临时支持树并读取原规则，没有猜测替代规则或全局安装。

在仓库根目录执行了以下 Git 清点命令：

```powershell
git -c core.quotepath=false ls-files --others --exclude-standard -- Documentation
git diff --name-only -- Documentation
git log -3 --date=iso-strict --format='%h %ad %s'
git rev-parse HEAD
```

原始文件清单由整理前的未跟踪路径生成，逐项计算 SHA-256。
归档校验调用 `ZipFile.testzip()`；包内应用索引排除 SDK、CMSIS、设备包和编译目录后，记录路径、长度与 SHA-256。
两套 Emm 包按相对路径集合和文件内容哈希比较。
`.eprj2` 用 `sqlite3.connect(file_uri + '?mode=ro', uri=True)` 打开，执行 `PRAGMA quick_check` 并查看项目与文档名称。
PDF 使用 pypdf 提取正文，Emm 说明书第 6、26、27 个 PDF 页面使用 pypdfium2 渲染后复核产品特点、菜单参数和脉冲接线内容。

## 检查结果

| 检查 | 结果及边界 |
| --- | --- |
| 新增原始资料 | 840 个文件、538,093,310 字节；不是压缩包展开后的源码数量 |
| 电赛资料 | 5 个 ZIP、2 个 `.eprj2` |
| 两套 Emm 包 | 各 413 个原始文件，逐路径 SHA-256 差异为 0 |
| 步进独立片段 | 7 个文件；`pwm/` 为空 |
| ZIP CRC | 5/5 通过，仅证明归档可读 |
| 应用文件索引 | 21E 31 项、22C 61 项、23E 86 项、25E 58 项、26H 43 项；不代表全部逐函数审查 |
| `.eprj2` 数据库结构 | 2/2 `quick_check = ok`；各发现原理图和 PCB 文档，不代表电气验收 |
| PDF | 驱动说明书 66 页，Modbus 说明 30 页；相关正文已读取，3 个说明书页面已渲染复核 |
| 原始资料保留 | 整理后重新比对原始 840 个文件的长度与 SHA-256 |
| 当前工程保护 | 对开始时已存在的硬件、固件、机械与代理配置文件重新比对基线 |
| 文档引用 | 核对新增/修改 Markdown 的本地链接、归档成员路径及清单记录 |

完整原始列表见[文件清单](../reference/classmate-materials-files.csv)，归档与应用文件结果见[机器索引](../reference/classmate-materials-archives.json)。
最终保护比对与链接检查结果见同目录的[检查结果](classmate-materials-check.json)。

## 真实问题与未验证项

`step_motor/uasrt_/urt.c` 含重复定义和函数外接收片段，作为不完整示例保留。
历史接收代码还存在仅按尾字节截帧、缓冲区边界和反馈新鲜度等待处理问题。
平衡球反馈中的机构系数和位置换算没有当前实物依据；详见[控制解读](../explanation/classmate-balance-reference.md)。

未编译或烧录任一旧工程，未运行随包 EXE、视频或 RAR 内程序。
未打开 EDA GUI 检查旧板电路，未执行 ERC/DRC，也未转为当前 KiCad 设计。
未验证摄像头识别程序、实物步进驱动、电平、回零、机械行程、温升或比赛成绩。
静态参考结论不能替代这些验证。
