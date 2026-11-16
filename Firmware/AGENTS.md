# 固件更新约定

用户于 2026-11-16 要求日常改用无线烧录。

- 以下烧录约定适用于已实测的 C/LL 工程。`Rust/` 为独立代码迁移版；用户要求仅修改代码，不进行实际烧录。Rust 工作只做主机测试、静态检查和 ARM 构建，后续实物验证需用户另行安排。

- 普通应用修改后，构建并使用 HC-08 BLE 更新。项目根目录运行 `./Firmware/flash-wireless.ps1`；Release 加 `-Preset release`。
- CMake 的 `wireless_flash` 目标执行同一流程。默认模块地址为 `F8:2E:0C:F3:68:37`，连接前关闭占用模块的遥测客户端。
- `coral_c5.ota` 是无线更新文件；应用地址固定为 `0x08010000`。无线协议不得擦写引导区或修改 Option Bytes。
- J-Link 用于首次安装 `first_install.hex`、引导程序更新、调试和失效救援；普通应用更新不依赖它。不要单独下载应用后忽略匹配的镜像元数据。
- 保留“上电停止、按键启动”行为；升级入口先停止电机。Flash 与恢复相关修改需运行主机恢复测试并记录板上验证范围。
- 使用与恢复步骤见 `../Documentation/how-to/wireless-update.md`，布局和协议见 `../Documentation/reference/wireless-update.md`。
