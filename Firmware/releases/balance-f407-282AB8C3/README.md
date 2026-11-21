# 同学 F407 转速稳球基线

2026-11-18 从同学的 26H 工程移植第 4 题固定回中分支。板上 C/LL Debug 应用为 `282AB8C3`，长度 41420 字节。[无线镜像](coral_c5.ota) · [控制源码](../../ports/classmate_26h/balance.c) · [对照验证](../../validation/2026-11-18/f407-speed.json)

后续稳球优化以这份源码和镜像为基线。画面和控制均按实物左负右正，把相机 x=135 对齐同学任务 160。默认右侧静止球发 CCW 抬右端，左侧发 CW。软件回放与板上静态回读已经记录，实物稳住时间待测。

在项目根目录恢复这份镜像可运行以下命令。驱动抱持水管时，固件会拒绝重启。

```powershell
& '.scratch/firmware-tools/Scripts/python.exe' 'Firmware/tools/wireless_flash.py' 'Firmware/releases/balance-f407-282AB8C3/coral_c5.ota'
```
