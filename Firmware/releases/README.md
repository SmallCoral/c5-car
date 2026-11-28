# 实车固件版本

2026-11-27 经 HC-08 升级后，实车回传 `C5APP,1,A9898F82,69224`，主控在比赛首页，两轮关闭。比赛提交包使用较早的版本 `F3EA96EE`，应用长度 67,372 字节。

| 文件 | 结项归档 |
| --- | --- |
| 最终实车 OTA 与配套 ELF | [A9898F82 固件目录](https://github.com/SmallCoral/c5-car/tree/1b936e16e70845d13a9f0e41dbcd20ecfca4ad00/Firmware/releases/competition-A9898F82) |
| 比赛提交包 | [F3EA96EE 原始提交](https://github.com/SmallCoral/c5-car/blob/1b936e16e70845d13a9f0e41dbcd20ecfca4ad00/Documentation/showcase/submission-F3EA96EE.zip) |

用 Arm GNU 14.3.1 按 Debug 配置构建 `competition_image`，升级文件生成在 `Firmware/build/debug/coral_c5.ota`。结项版的整包 SHA-256 为 `29719ecfce8fe8b481eea08f29b85806c415e617417b6cc45b6ccca14d6a1813`。[构建与回归](../README.md#构建与回归) · [无线升级与恢复](../../Documentation/how-to/wireless-update.md)
