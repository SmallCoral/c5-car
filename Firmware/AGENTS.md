# 固件更新约定

- 按 2026-11-21 的现场要求，后续题目在当前合并比赛固件上开发。保留已调好题目的菜单入口、标定、控制参数和实车轨迹回归。日常烧录走 `flash-jlink.ps1` 或 `flash-wireless.ps1` 的默认构建入口；每交付一道新题，将其入口和回归加入 `competition_image`。通过实物菜单切题。单独恢复历史版本时才使用指定的旧存档。

用户于 2026-11-20 恢复实地测试使用的 HC-08 无线远程烧录。日常 C/LL 应用走蓝牙，保留遥测、状态查询与停车。结项的板上版本与校验值见[实车固件记录](releases/README.md)。

- 按 2026-11-25 用户要求，比赛主控和引导程序只维护 C/LL 工程。

- 这台车的杆子在步进解除抱持后也不会掉落。从项目根目录直接执行 `./Firmware/flash-wireless.ps1 -PipeSupported`，或构建 `wireless_flash_supported`。上传后以应用返回的长度和 CRC 核对镜像。
- 有线更新应用及匹配元数据使用 `./Firmware/flash-jlink.ps1`。按用户要求，直接调用 J-Link Commander 时也传 `-NoGui 1`，保留命令输出和错误退出码。PyLink 采集在每次 `open()` 前调用 `exec_command("SuppressGUI 1")`。[SEGGER 参数说明](https://kb.segger.com/J-Link_Commander)
- `coral_c5.ota` 是无线更新文件；应用地址固定为 `0x08010000`。无线协议不得擦写引导区或修改 Option Bytes。
- 首次安装包含引导程序的 `first_install.hex`、引导程序更新、调试和失效救援也使用 J-Link。
- 按用户 2026-11-19 的最新要求，上电进入校赛 A 题主页。SW2 进入题目，松开后再按 SW2 确认启动；SW5 返回。BALANCE 的自动寻球流程从调试页手动启动。Flash 与恢复相关修改需运行主机恢复测试并记录板上验证范围。
- 使用与恢复步骤见 `../Documentation/how-to/wireless-update.md`，布局和协议见 `../Documentation/reference/wireless-update.md`。
