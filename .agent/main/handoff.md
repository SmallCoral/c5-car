# 校赛 A 题交接

截至 2026-11-21，现场最后一次通过 HC-08 回读的合并应用为 `EC2ED1B4`，64500 字节。三个已接入题目从同一首页进入。继续开发时先核对主控身份，使用默认 `competition_image` 入口构建。旧题的独立实车镜像和调试符号可按 CRC 恢复。[构建与操作](../../Firmware/README.md) · [恢复包](../../Firmware/releases/README.md)

- 第一题从右挡杆起步时相机可能暂时失测。已测一轮 9.948 s，8 s 目标尚未达到。可对照右点首次进区到停稳期间的球速与轴角，再决定制动量。[每段到达和停稳时间](../../Documentation/validation/README.md)
- 第二题从 A 点朝顺时针启动，双轮平均里程 41800 计数停车。发挥一另用 42000 计数。右 23 mm 的最近一圈为 32.468 s，970 帧偏差均在 ±17 mm 内，现场估计提前约 1 cm。下圈比较停车位置时沿用同一个车体参考点。[原始轮速与球位](../../Documentation/validation/data/offset-lap.csv)
- 相机原图 x 从左到右依次约为 531、400、282、160、49，对应左挡杆、左 5 cm、O、右 5 cm、右挡杆。水平绝对编码器参考为 10260。右端最高和最低时的电机轴角分别为 −70.20°、+106.53°。[现场标定](../../Firmware/calibration/camera640-and-shaft.json)
- 发挥一采数先连 HC-08，确认收到 v7 遥测，再由现场按 SW2 起跑。停车后读取 `C5:RANGE?` 检查逐帧极值。蓝牙升级会暂时解除步进抱持，托稳水管后按烧录说明操作。[遥测字段](../../Documentation/reference/firmware-telemetry.md) · [无线升级](../../Documentation/how-to/wireless-update.md)
- MaixCAM2 设备端识别程序由操作者调过，设备脚本和模型哈希尚待比对。[相机工程](../../Firmware/26h/README.md)
