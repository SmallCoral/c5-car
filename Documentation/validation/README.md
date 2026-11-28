# 比赛实测与固件版本

球位以 O 为零，向右为正。下表按实际跑车的固件列出结果。停车距离由操作者用同一车体参考点估计。

| 项目 | 当时的固件 | 实车记录 |
| --- | --- | --- |
| 第一题，从右挡杆起步 | `8A10EDEE` | 9.948 s 完成。随后 10 s 球位在 −52～−46 mm。 |
| 第一题，后续复测 | `5A905E4E` | 8.024 s 完成，比比赛目标多 0.024 s。随后约 10 s 球位在 −51～−50 mm。 |
| 第二题，里程停车版本 | `D97DBD0E` | 13.203 s 完圈，41800 计数停车，现场估计提前 1～3 cm。 |
| 发挥一，目标右 23 mm | `EC2ED1B4` | 32.468 s 完圈。主控采到 970 帧可信球位，偏差 −17～+17 mm，停车点估计提前约 1 cm。 |

2026-11-27，最终版 **`A9898F82`** 经 HC-08 写入并回读，应用长 **69,224 字节**。第二、四题与发挥一采用 A 点黑线标记停车，第三题仍按 A→B 直线里程停车。最终版仅完成烧录与回读，终点误差尚未上赛道尺量。[最终镜像与校验值](../../Firmware/releases/README.md)

[第一题实测轨迹图](data/task1-position.png) · [比赛现场与曲线](../../Images/README.md) · [历次分段计时与数据说明](https://github.com/SmallCoral/c5-car/blob/1b936e16e70845d13a9f0e41dbcd20ecfca4ad00/Documentation/validation/README.md) · [蓝牙原始记录](https://github.com/SmallCoral/c5-car/tree/1b936e16e70845d13a9f0e41dbcd20ecfca4ad00/Documentation/validation/data)
