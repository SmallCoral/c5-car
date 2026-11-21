# 实车固件恢复

日常开发从当前合并源码构建。需要复现历史实车状态时，按应用 CRC32 选对应的 OTA 和调试符号。CRC32 和字节数针对应用载荷，SHA-256 针对含包头的整个 OTA。

| 实车用途 | 应用 CRC32 | 字节 | 固件与符号 | OTA SHA-256 |
| --- | --- | ---: | --- | --- |
| 当前合并版 · 第一题、第二题、发挥一 | `EC2ED1B4` | 64500 | [OTA](offset-EC2ED1B4/coral_c5.ota) · [ELF](offset-EC2ED1B4/coral_c5.elf) | `f4d54d9cd8ece53cd7b6ddd1830058289a825ee6501df3e495b9d052883efac2` |
| 前两题合并版 | `D97DBD0E` | 61632 | [OTA](task2-D97DBD0E/coral_c5.ota) · [ELF](task2-D97DBD0E/coral_c5.elf) | `b3509ea4259e369dd36b916f4e9ebc228cda1d961d9c8fe0bca63cf034b31396` |
| 第一题历史版 | `8A10EDEE` | 60804 | [OTA](task1-8A10EDEE/coral_c5.ota) · [ELF](task1-8A10EDEE/coral_c5.elf) | `3e1359b9a34732fc862c446ecbeeee7c3c1157fbdabce035e71b31ce46f9b301` |

当前合并版源码标签为 `offset-EC2ED1B4`。早期两份镜像对应源码标签 `task1-8A10EDEE` 和 `task2-D97DBD0E`。实车验证使用 Debug 构建。恢复历史镜像会将整份应用切换到该版本。

在仓库根目录，用已安装项目蓝牙依赖的 Python 只读查询主控版本。

```powershell
python Firmware/tools/wireless_flash.py --info
```

要恢复包含三个题目的实车版，使用下面的命令。步进正在抱持水管时，先将水管独立托稳，再给本次上传加 `--pipe-supported`。待工具报告 `Verified running application` 后，对照上表核对 CRC 和长度。[蓝牙连接与异常恢复](../../Documentation/how-to/wireless-update.md)

```powershell
python Firmware/tools/wireless_flash.py Firmware/releases/offset-EC2ED1B4/coral_c5.ota
```
