# 无线升级布局和协议 v1

板端使用 STM32C5 LL/CMSIS，电脑端使用 Python/Bleak。HC-08 仍连接 USART1 PA6/PA7，57600 8N1，不依赖 STM32 ROM Bootloader 的 UART 引脚。

## 固定存储布局

| 区域 | 起始地址 | 大小 | 用途 |
| --- | --- | --- | --- |
| 引导程序 | `0x08000000` | 64 KiB | 校验、接收、安装、恢复；无线协议不能写入 |
| 应用 | `0x08010000` | 448 KiB | 独立向量表与应用程序 |
| 下载暂存 | `0x08080000` | 448 KiB | 完整接收并校验后才提交 |
| 预留 | `0x080F0000` | 48 KiB | 当前未使用 |
| 应用元数据 | `0x080FC000` | 8 KiB | 头部、长度、CRC |
| 暂存提交元数据 | `0x080FE000` | 8 KiB | 最后写入的已验证镜像头部 |
| SRAM 升级请求 | `0x2003FFE0` | 32 B | 魔数及反码；两个链接脚本均从栈顶排除 |

主 Flash 1 MiB、双 bank、每页 8 KiB；128 bit quadword 写入。擦除和写入关键代码及字面量在 SRAM 中运行，期间屏蔽普通中断，Flash 控制器等待超限则系统复位。保留 ECC 错误检测处理；缓存保持复位默认关闭。参数来自 [ST STM32C5A3 数据手册](https://www.st.com/content/st_com/en/technical-documents/stm32c5a3cg_asset/chunk-002-topic-stm32-ds-functional-overview.html)、[RM0522](https://www.st.com/resource/en/reference_manual/rm0522-stm32c5xxx-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) 和仓库固定的 LL 2.1.0；写入流程同时核对 [ST Flash 驱动说明](https://dev.st.com/stm32cube-docs/stm32c5xx-hal-drivers/2.1.0/en/docs/drivers/hal_drivers/flash/hal_flash_how_to_use.html)。未链接 HAL。

镜像头为 8 个小端 uint32：`magic=0x3541544F`、`version=1`、`board=0xC5A30001`、`address=0x08010000`、`length`、`image_crc32`、`reserved=0`、`header_crc32`。头部 CRC 覆盖前 28 B；镜像 CRC 只覆盖实际长度，不含末尾 Flash 对齐填充。算法为 CRC-32/ISO-HDLC，与 Python `zlib.crc32` 相同。

`.ota` 为 32 B 头部加应用 BIN。先验证头部、镜像长度、初始栈指针和 Thumb 复位入口，再写入暂存。提交前校验完整暂存内容；复位后保留暂存镜像，擦除应用及其元数据，复制并再次校验，写入应用元数据，最后擦除暂存提交页。

下载未提交时旧程序不变。安装失败且暂存完整时重试安装；两份镜像都无效时停留在无线恢复界面。已提交的暂存镜像不能被新的 BEGIN/ABORT 覆盖。没有 A/B 健康回滚：CRC 正确但应用逻辑错误时，使用 TASK 强制恢复入口上传修正版。

## 应用侧文本命令

| 命令 | 行为 |
| --- | --- |
| `!` | 立即进入原有电机停止流程 |
| `C5:OTA\n` | 停止轮电机、发送步进停止/失能、禁止按键启动；等待串口发送结束后请求系统复位 |
| `C5:INFO\n` | 回复 `C5APP,1,CRC八位十六进制,实际字节数` |

正常遥测仍使用 CSV v2。应用进入升级准备状态时暂停周期遥测。引导程序不发送 CSV，使用下述二进制应答。

## 二进制协议

帧格式：`C5 5A B0 07` + `version:u8=1` + `command:u8` + `length:u16` + `sequence:u32` + payload + `crc32:u32`。所有整数小端；帧 CRC 覆盖头部和 payload。最大 payload 132 B。UART 解析允许 BLE 分片、粘包和错误后重新同步；未完成帧间隔超过 1 秒会丢弃。

应答 command 为请求 command OR `0x80`，sequence 原样返回。基本 payload 为 `status:u32, received:u32`；status：0 成功、1 参数错误、2 状态错误、3 偏移错误、4 Flash 错误、5 镜像错误。

| 命令 | ID | 请求 payload | 说明 |
| --- | --- | --- | --- |
| HELLO | 1 | 空 | 应答另有 board/address/max_size/chunk/app_size/app_crc，共 32 B |
| BEGIN | 2 | 完整镜像头 | 擦除暂存；同一会话同一头部幂等，返回已接收偏移 |
| DATA | 3 | offset:u32 + 最多 128 B | 顺序写入；偏移 128 B 对齐；最后一包可短于 128 B |
| END | 4 | 空 | 校验暂存、最后写入提交元数据；重复 END 幂等 |
| RUN | 5 | 空 | 有旧应用或已提交镜像时允许复位；ACK 发出并等待 300 ms 后复位 |
| ABORT | 6 | 空 | 取消未提交下载；再发 RUN 可运行旧应用 |

DATA 的重发会比较已写入内容，再回 ACK，不对同一 quadword 二次编程。BEGIN 和 DATA 的所有擦写地址由板端从固定分区和偏移生成，包中不能指定任意写入地址。

电脑端每次最多写 20 B。DATA 首发逐个等待 GATT 写入返回，然后等待整包 ACK 及偏移。DATA 等待应答 1.3 s，覆盖引导程序 1 s 的残帧过期时间。重发使用同一序号、同一数据和 20 ms 分片间隔。连续两包都需要重发时，DATA 暂时采用 20 ms 分片间隔，连续 32 包首发成功后再试快速发送。控制命令沿用原节拍。单包最多 4 次尝试，连接最多 3 次尝试。重连且 MCU 未复位时可继续同一暂存下载，断电后从头上传。安装成功须回读新应用的长度和 CRC。[实车整包更新与回读](../validation/README.md)

CRC 用于检测传输和存储损坏，**不提供签名认证或固件来源鉴别**。当前是本地开发升级协议；引导程序自身不支持无线替换，也没有设置不可逆保护或更改读保护。

代码入口：[ota.h](../../Firmware/include/ota.h)、[接收和安装](../../Firmware/boot/ota_store.c)、[Flash 后端](../../Firmware/boot/flash_port.c)、[上传器](../../Firmware/tools/wireless_flash.py)。
