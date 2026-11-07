# M2.8 续接来源

查询日期 2026-11-07。`*-retail.html` 为立创国内独立商品页原文，`C*-record.json` 是其中公开 `__NEXT_DATA__` 商品、价格与国内仓库存字段快照。具体选中的 22 种 SKU 以 `../../procurement_m28.json` 为准；目录内也保留了查询后未选中的记录，不据目录文件名自动认定选型。

国内页面通过 PowerShell Invoke-WebRequest 取得 HTTP200。Python urllib 返回 HTTP405，搜索页有 HTTP203 挑战；没有绕过挑战或使用登录态。TPS25810 的国际页面仅用于找到商品内部编号，随后在国内独立页重新核对 MPN、C编号、现货及国内仓。国际库存未当国内库存使用。U11 的编号通过公开替代查询定位，再核对独立商品页。

`HX-PZ2.54-2x6P-ZZ.pdf` 是韩下原厂图纸的立创镜像，原链接在采购记录；第1页核对双排2.54mm、0.64方针、1.0孔及高度。板内3D仍为通用排针外形，不冒充该厂准确模型。

AP64500 原厂文件保留在相邻 `../m28/`。聚合物额定与 ESR 来自 [Panasonic16SVP150M](https://industrial.panasonic.com/ww/products/pt/os-con/models/16SVP150M)。TPS25810 原厂 [SLVSCQ5D](https://www.ti.com/lit/ds/symlink/tps25810.pdf) §8.2.1.1.1 用于输入150µF/输出10µF的源端要求。价格使用人民币快照，额定、计算和实测分别记录。
