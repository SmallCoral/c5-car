# A 题器件选型与采购

整理日期：2026-11-03。
2026-11-05 更新：用户确认两只摄像头，检测端用 MaixCAM，图传端沿用 OV2640 规划。光栅条目为此前备选，本轮未据此采购或加入生产 BOM。见 [双摄资料](cameras.md)。
以下区分已在 Motor 使用的器件、推荐新增器件和候选项。
报价来自查询到的销售页，可能含搜索缓存；币种保留USD，不按假定汇率伪装成人民币现价。
报价不含运费、税费、PCB、贴装和机械加工，库存需在下单时复核。
LCSC社区查询API本轮返回403，供货依据改用销售商商品页。

## 推荐清单

| 用途 | 具体器件和数量 | 使用方式与选择理由 | 采购依据 |
| --- | --- | --- | --- |
| 主控 | 现有 STM32C5A3VGT6 核心板×1 | 继续承担所有运动控制，不另购主控开发板 | 复用 |
| 视觉检测 | MaixCAM×1，用户确认 | 顶部俯视钢球；镜头与外壳版本、通讯和供电待核对 | 用户本轮确认；[原厂资料](https://wiki.sipeed.com/hardware/zh/maixcam/maixcam.html) |
| 左右轮 | 现有 MG513XP28_12V×2 | 编码器轮速闭环，六芯线束接J4/J6 | 复用；[接口表](motor-interface.md) |
| 直流驱动 | DRV8874PWPR×2，C1855818 | 保留硬件限流、电流反馈和故障输出，比换成无反馈廉价桥更利于调试 | [LCSC](https://www.lcsc.com/product-detail/Motor-Driver-ICs_Texas-Instruments-DRV8874PWPR_C1855818.html)：1片$1.1886，页面显示9851片 |
| 摆杆执行 | 现有42BYGH34-401A×1 | 电机固定底盘，通过曲柄连杆抬升摆杆自由端 | 复用；铭牌1.5A并非电源输入电流 |
| 步进驱动 | TMC2209-LA-T×1，C2150710 | 裸芯片集成，STEP/DIR执行、UART设电流及读诊断；回零仍靠独立限位 | [LCSC](https://www.lcsc.com/zh-CN/category/1016.html)：1片$2.8518、10片$2.523，页面显示19179片；搜索定位C2150710 |
| 摆杆角度 | AS5600-ASOT×1，C499458 | 安在摆杆铰轴，测实际摆杆角度；不只测电机轴 | [LCSC](https://www.lcsc.com/product-detail/C499458.html)：1片$1.3747，页面显示2611片 |
| 循迹 | QRE1113GR×12，C232862 | 自研模拟阵列；开/关灯采样、逐路归一化、加权中心；最低版可减至8路 | [LCSC](https://www.lcsc.com/zh-CN/product-detail/Optical-Sensors_ON-Semiconductor-ON-Semiconductor-QRE1113GR_C232862.html)：10片档$0.5167/片，页面显示9092片 |
| 循迹模拟复用 | 74HC4067PW,118×1，C179326 | 在探头板用3.3V供电，12入1出；切换后留ADC建立时间 | [Nexperia器件页](https://www.nexperia.com/products/analog-logic-ics/analog-switches-and-multiplexers/analog-switches/series/74HC4067-74HCT4067.html)、[LCSC](https://www.lcsc.com/product-detail/C179326.html)；本轮未取得可靠现货单价 |
| 钢球光栅发射 | IR333-A×32，备料按40只，C264290 | 940nm、5mm灯珠；沿杆8mm节距，4组错时发光 | [LCSC](https://www.lcsc.com/product-detail/C264290.html?is_substitute=1&original_product_code=C2936543&original_unit_price=0.1582)：10片档$0.0557/片，页面显示36500片，10只起/10只倍数 |
| 钢球光栅接收 | PT204-6B×32 | 940nm黑色3mm光敏三极管，配遮光孔；买一小批先验证光路 | [DigiKey](https://www.digikey.com/en/products/detail/everlight-electronics-co-ltd/PT204-6B/2675642)：10片档$0.236/片，页面显示14953片；国内批量报价待核 |
| 光栅读取 | SN74HCS165PWR×4，C2867845 | 在接收条串联读取32位；输入自带施密特，不把慢光电边沿直接送普通HC165 | [LCSC](https://www.lcsc.com/fr/product-detail/C2867845.html)有商品；[Mouser](https://www.mouser.com/ProductDetail/Texas-Instruments/SN74HCS165PWR?qs=DPoM0jnrROXHipdFjx%2FLiA%3D%3D)1片$0.69、10片$0.489，页面显示1903片 |
| 发挥图传 | ESP32-S3-WROOM-1-N8R8×1，C2913201 | 自研电路上的射频模组，PSRAM用于图像缓冲；不买ESP32-CAM开发板代替设计 | [LCSC](https://lcsc.com/product-detail/wifi%20modules_espressif%20systems_esp32-s3-wroom-1-n8r8_C2913201.html)显示1片$5.515、1661片，但该价格快照较旧；[原厂模组页](https://www.espressif.com/products/modules)提供销售入口 |
| 摄像头和存储 | OV2640成像组件×1、microSD插座×1、正规卡×1 | 只用镜头/传感器组件，主板自做处理、存储与供电；最终FPC针序按具体货源锁定 | [乐鑫相机驱动](https://github.com/espressif/esp32-camera)支持OV2640；本轮未锁定镜头组件商家，价格不作为已核报价 |

现有AP63205、AO4407A、SN74LVC2G17及电源保护先沿用。
M2新增AP63203WU-7独立辅助3.3V支路为候选，需重新核算开关频率、负载、滤波与热。
光栅、循迹与图传不从Core的100mA外接预算取电。

低成本优先通过复用电机和主控、取消不需要的网口、按阶段装配图传来实现。
AS5600不需要高价多圈编码器，TMC2209只用一颗。
小批新增电子部分先按人民币300～450元预留，此为预算目标，不是商城结算报价。
该目标包含光学用料、驱动和图传配套，不含现有器件、制板、加工和运费；锁定国内货源后再收敛。

## 此前光栅备选思路

沿球杆布置32对红外通道，节距8mm，通道跨度248mm。
球遮挡一个或多个通道，以遮挡区间中点估计位置。
理想几何量化误差约为半个节距，即4mm；这不是总测量精度，实际还包括孔位、光斑、球径和阈值。
先用4～8路验证，再决定是否扩到32路，不先下单整套长板。

发射与接收要装在槽的两侧，光轴接近钢球赤道高度。
使用黑色光阑、分组点亮和关灯基线，减少邻道串光及环境光影响。
灯灭仍被判定有光的通道属于基线异常，不能简单相减后当作有效球位置。
无球、球在边缘、遮挡不连续和所有通道饱和都要有无效状态。
发射支路初拟5V、每灯680Ω，按1.2V正向压降约5.6mA，仅作小样起始值。
每组8灯、4组分时，每组约45mA；实际接收裕量决定阻值和时序。
对管供货型号变化必须重新验证波长、灵敏度、脚序和光阑，不能只替换BOM文字。

PT333-3C曾作为低价候选，但[销售页](https://lcsc.com/product-detail/Phototransistors_Everlight-Elec-PT333-3C_C264295.html)显示缺货，未作为主选。
PT204-6B目前有可购依据，国内报价需要进一步压低；可比较PT334-6B同类，但不在没有稳定货源时承诺替代。

## 需要守住的电气边界

- [TMC2209原厂资料](https://www.analog.com/en/products/tmc2209.html)的2A RMS是芯片能力；板级散热、取样电阻和实测温升决定可用相电流。
- 步进先以较低RMS电流调试，测绕组电阻和温升后再设额定；不要把铭牌1.5A直接等同UART设定或电池电流。
- [AS5600资料](https://look.ams-osram.com/m/7059eac7531a86fd/original/AS5600-DS000365.pdf)的12bit分辨率不等于12bit角度准确度；检查磁场状态并做零位标定。
- [QRE1113GR](https://www.onsemi.com/download/data-sheet/pdf/qre1113-d.pdf)需要试验离地高度与黑白对比，不能按远距离反射开关使用。
- [SN74HCS165](https://www.ti.com/lit/gpn/SN74HCS165)使用3.3V电源，所有并行输入受限于该电源；光敏集电极上拉也用3.3V。
- [IR333-A](https://en.everlight.com/wp-content/plugins/ItemRelationship/product_files/pdf/IR333-A.pdf)的峰值电流有脉宽与占空比条件，本方案不采用1A脉冲宣传值。

## 暂不选择的方案

| 候选 | 当前处理 |
| --- | --- |
| 单个VL53L4CD直接测钢球 | 不作基本闭环主选；[ST资料](https://www.st.com/en/imaging-and-photonics-solutions/vl53l4cd.html)的量程不证明能稳定测槽内小钢球，需验证视场、槽壁和镜面反射 |
| OpenMV、K210、成套循迹控制模块 | 当前先不采购，避免重复主控和增加成本；算法、电路与采样链自己完成 |
| TB6612替换现有DRV8874 | 不建议为少量成本损失现有限流和电流反馈；MG513系列堵转约3.2A需要认真处理 |
| MG996R直接做摆杆 | 可作快速原型备选，正式方案优先复用现有步进，比较回差、温升和控制带宽后再决定 |
| MPU6050等IMU | 首版不必需；轮速、灰度和直接摆杆角度已能形成闭环，确认侧向补偿收益后再加 |

## 采购与规则确认

赛题对模块化成品控制方案的限制表述需要主办方澄清。
推荐自研PCB、裸驱动及传感器；ESP32射频模组和镜头成像组件是否允许，提前以具体料号确认。
比赛运行只自主控制，Wi-Fi页面用于只读画面与回放。
没有为本轮下单，也没有把未核实报价填入当前Motor生产BOM。
