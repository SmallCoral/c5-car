# 3300 / 2200 CPS 实车稳定版

该镜像在 2026-11-18 实车记录了 18.872 秒和 18.905 秒两次 `DONE`，现场确认回到 A、过弯稳定。对应源码和记录保存在本地 Git 标签 `lap-stable-3300-BF33A9F7`。

从项目根目录执行以下命令，可直接把存档镜像刷回小车。

```powershell
& .scratch/firmware-tools/Scripts/python.exe Firmware/tools/wireless_flash.py Firmware/releases/lap-3300-BF33A9F7/coral_c5.ota
```

成功后板上身份应为 `C5APP,1,BF33A9F7,25892`。OTA 文件的 SHA-256 为 `bdca85a5c0ade3b1694683566b7b98d59867a6d02813cc56a8423e435d7bf0c4`。构建参数与圈速记录见[实车验证](../../../Documentation/validation/2026-11-18-lap-centering.md)。
