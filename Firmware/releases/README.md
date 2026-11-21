# 实车固件存档

从仓库根目录选用与测试题目对应的 OTA。CRC32 和字节数取自主控应用，SHA-256 校验整个 OTA 文件。最近一次实车回读是 2026-11-20 22:23 的第一题版本。

| 用途 | 实车 OTA | 应用 CRC32 / 字节数 | OTA SHA-256 |
| --- | --- | --- | --- |
| 第一题 · 稳球与等待新观测 | [第一题 OTA](task1-8A10EDEE/coral_c5.ota) | `8A10EDEE` / 60804 | `3e1359b9a34732fc862c446ecbeeee7c3c1157fbdabce035e71b31ce46f9b301` |
| 第二题 · 按键起跑、里程停车 | [第二题 OTA](task2-D97DBD0E/coral_c5.ota) | `D97DBD0E` / 61632 | `b3509ea4259e369dd36b916f4e9ebc228cda1d961d9c8fe0bca63cf034b31396` |

第一题的 [Debug ELF](task1-8A10EDEE/coral_c5.elf) 保留实车符号，`arm-none-eabi-objcopy -O binary` 提取的应用与第一题 OTA 载荷逐字节相同。常规源码 Debug 构建会生成表中的第二题版本。[第一题实车回读](../validation/2026-11-20/task1-restore-flash.txt) · [第二题实车回读](../validation/2026-11-20/task2-distance-flash.txt)

两份源码的 Git 标签分别是 `task1-8A10EDEE` 和 `task2-D97DBD0E`。要在主开发目录旁调试板上第一题的准确版本，可从仓库根目录运行 `git worktree add --detach ../c5-car-task1 task1-8A10EDEE`，在新目录中构建。第一题标签的源码已独立导出并重建，产物与实车 OTA 逐字节相同。[核验记录](../../Documentation/validation/2026-11-21-handoff.md)

## 查询与恢复

先按[蓝牙环境说明](../../Documentation/how-to/wireless-update.md)准备带 Bleak 的 Python。在项目根目录，用该解释器运行只读查询。

```powershell
python Firmware/tools/wireless_flash.py --info
```

需要恢复时，选择一个文件上传。以下命令恢复第一题。若恢复第二题，将文件换为 `Firmware/releases/task2-D97DBD0E/coral_c5.ota`。步进正在抱持水管时，先将水管独立托稳，再在这一次上传命令末尾加 `--pipe-supported`。

```powershell
python Firmware/tools/wireless_flash.py Firmware/releases/task1-8A10EDEE/coral_c5.ota
```

等待 `Verified running application`，重连并用 `--info` 核对上表的 CRC 和长度。更早的调试基线分别保存在 [F407 转速回中](balance-f407-282AB8C3/README.md) 和 [3300/2200 CPS 循线](lap-3300-BF33A9F7/README.md)。
