# 机械参考几何来源

日期：2026-11-05。以下文件来自用户指定的本机资料及当前工程，只用于本项目机械参考。原始文件保持只读。

| 派生文件 | 原始来源及处理 |
| --- | --- |
| `reused-hardware/Motor*.brep`、`Rear*.brep`、`Caster*.brep` | `C:/Users/sc/Documents/Dev/Hardware/EdgeMind-Robot/model/chassis/v4-lidar-imu/EdgeMind_Chassis.FCStd` 中的 R3X 命名零件；原比例提取 |
| `reused-hardware/manifest.json` | 原生 V4、R3X STEP、独立 MG513X STEP 的 SHA-256、零件名、包络和体积 |
| `reused-hardware/BatteryBay.brep` | 用户 `C:/Users/sc/Downloads/EdgeMind_Assembly.step` 对应 V5 原生总装的 `LowerTray` 定位凸框；以几何交集提取 |
| `reused-hardware/battery-bay-source.json` | 用户文件、原生 V5 哈希、提取边界、91×53 外框与 87×49 内框尺寸 |
| `CoralC5-Core-current.step`、`core-edge-pads.json` | 当前 Core PCB 的导出实体和实际 98 触点位置；3 个 VRML 模型未导出 |
| `CoralC5-Motor99-planning.step` | 当前 Motor PCB 的临时 99 mm 外形副本；仅保留范围内 53 个封装，不保存回原 PCB |

原始 EdgeMind 及厂家模型的许可尚未独立确认，复制到本地用于本次授权任务不代表可公开再分发；发布这些第三方几何前应核实来源许可。本目录不赋予其新的开源许可。

交互预览的 `preview/vendor/three.min.js` 从同一 EdgeMind V5 本地预览器复制，原 [MIT 许可](../preview/vendor/three.LICENSE.txt) 随文件保留。当前 Core、Motor 的其他元件模型来源继续以原硬件工程的 [THIRD_PARTY.md](../../Hardware/stm32c5a3/THIRD_PARTY.md) 为准。
