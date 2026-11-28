# Coral-C5 电路板

用 KiCad 10 打开下表的工程文件。各板的本地符号、封装、三维模型和库表按工程相对路径放置，两个主板共用的资源位于 `Hardware/Shared/`。

| 电路板 | 用途 | 工程与说明 |
| --- | --- | --- |
| CoralC5-Core | STM32C5A3 核心板，实现主控、时钟、电源及板间接口 | [KiCad 工程](Hardware/CoralC5-Core/CoralC5-Core.kicad_pro) · [板级说明](Hardware/CoralC5-Core/README.md) · [选型表与数据](Hardware/CoralC5-Core/docs/bom) |
| CoralC5-Motor | 电源、轮电机驱动和外设接线底板 | [KiCad 工程](Hardware/CoralC5-Motor/CoralC5-Motor.kicad_pro) · [板级说明](Hardware/CoralC5-Motor/README.md) · [采购表](../../Documentation/reference/motor-bom.md) |
| Transfer | 修正 XT60 电源输入极性的转接板 | [KiCad 工程](Hardware/Transfer/Transfer.kicad_pro) · [针脚定义与核验记录](Hardware/Transfer/README.md) |

Core 与 Motor 通过自定义的 98 触点连接。实车 J6 接左轮，J4 接右轮，灰度 CH1 到 CH8 按车身从左向右排列。[98 触点定义](Hardware/Shared/interface.csv) · [外设接线](../../Documentation/reference/m2-interfaces.md) · [GPIO 分配](../../Documentation/reference/motor-core-pins.md) · [第三方资源与许可](THIRD_PARTY.md)
