// 电脑端镜像解析、身份检查和打包逻辑，可独立于 BLE 使用。

use coral_core::ota::{self, Header};
pub mod exchange;
use std::{fs, path::Path};
pub type Result<T> = std::result::Result<T, Box<dyn std::error::Error + Send + Sync>>;

#[derive(Debug)]
pub struct Hello {
    pub received: u32,
    pub length: u32,
    pub crc: u32,
}
impl Hello {
    // 从应答数据读取协议字段并检查其有效性。
    pub fn decode(p: &[u8]) -> Result<Self> {
        if p.len() != 32 || ota::get32(p) != 0 {
            return Err("invalid HELLO reply".into());
        }
        for (i, expected) in [
            ota::BOARD_ID,
            ota::APP_ADDR,
            ota::IMAGE_MAX,
            ota::CHUNK as u32,
        ]
        .iter()
        .enumerate()
        {
            if ota::get32(&p[8 + i * 4..]) != *expected {
                return Err("board or partition contract mismatch".into());
            }
        }
        Ok(Self {
            received: ota::get32(&p[4..]),
            length: ota::get32(&p[24..]),
            crc: ota::get32(&p[28..]),
        })
    }
}
// 检查设备返回的续传偏移是否与当前镜像相容。
pub fn resume_offset(p: &[u8], length: usize) -> Result<usize> {
    if p.len() < 8 || ota::get32(p) != 0 {
        return Err("invalid BEGIN reply".into());
    }
    let n = ota::get32(&p[4..]) as usize;
    if n > length || (!n.is_multiple_of(ota::CHUNK) && n != length) {
        return Err("invalid resume offset".into());
    }
    Ok(n)
}
// 将设备身份与预期的长度和 CRC 对照。
pub fn verify_identity(line: &str, length: u32, crc: u32) -> Result<()> {
    let fields: Vec<_> = line.trim().split(',').collect();
    if fields.len() != 4
        || fields[0] != "C5APP"
        || fields[1] != "1"
        || u32::from_str_radix(fields[2], 16)? != crc
        || fields[3].parse::<u32>()? != length
    {
        return Err("running application identity mismatch".into());
    }
    Ok(())
}

// 读取应用文件，校验包头及向量后取得载荷。
pub fn read_image(path: &Path) -> Result<(Vec<u8>, Header)> {
    let mut data = fs::read(path)?;
    match path
        .extension()
        .and_then(|x| x.to_str())
        .map(str::to_ascii_lowercase)
        .as_deref()
    {
        Some("ota") => {
            if data.len() < 32 {
                return Err("truncated OTA package".into());
            }
            let header = Header::decode(&data[..32]).map_err(|_| "invalid OTA header")?;
            data.drain(..32);
            if Header::for_image(&data) != Ok(header) {
                return Err("image CRC/length/vector mismatch".into());
            }
            Ok((data, header))
        }
        Some("bin") => {
            let h = Header::for_image(&data).map_err(|_| "invalid application vectors/size")?;
            Ok((data, h))
        }
        _ => Err("use an application .ota or .bin file".into()),
    }
}
// 生成一条带校验和的 Intel HEX 记录。
fn hex_record(address: u16, kind: u8, data: &[u8]) -> String {
    use std::fmt::Write;
    let mut bytes = vec![data.len() as u8, (address >> 8) as u8, address as u8, kind];
    bytes.extend_from_slice(data);
    let checksum = 0u8.wrapping_sub(bytes.iter().fold(0u8, |sum, &b| sum.wrapping_add(b)));
    bytes.push(checksum);
    let mut line = String::from(":");
    for b in bytes {
        write!(line, "{b:02X}").expect("String write");
    }
    line.push('\n');
    line
}
// 合并带地址的区域并输出正确的扩展地址与结束记录。
pub fn make_hex(segments: &[(u32, &[u8])]) -> Result<String> {
    let mut sorted = segments.to_vec();
    sorted.sort_by_key(|(address, _)| *address);
    let mut output = String::new();
    let mut upper = None;
    let mut previous_end = 0u64;
    for (address, data) in sorted {
        let end = u64::from(address) + data.len() as u64;
        if u64::from(address) < previous_end || end > u64::from(u32::MAX) + 1 {
            return Err("overlapping/overflowing HEX segments".into());
        }
        previous_end = end;
        let mut offset = 0;
        while offset < data.len() {
            let current = address + offset as u32;
            if upper != Some(current >> 16) {
                upper = Some(current >> 16);
                output += &hex_record(0, 4, &((current >> 16) as u16).to_be_bytes());
            }
            let n = (data.len() - offset)
                .min(16)
                .min(0x10000 - (current & 0xffff) as usize);
            output += &hex_record(current as u16, 0, &data[offset..offset + n]);
            offset += n;
        }
    }
    output += &hex_record(0, 1, &[]);
    Ok(output)
}
// 组装 OTA 和首次安装镜像文件。
pub fn package(app: &Path, boot: &Path, out: &Path) -> Result<Header> {
    let (image, header) = read_image(app)?;
    let boot = fs::read(boot)?;
    if !(512..=65536).contains(&boot.len()) {
        return Err("bootloader exceeds 64 KiB or is truncated".into());
    }
    let sp = ota::get32(&boot);
    let entry = ota::get32(&boot[4..]);
    if sp & 7 != 0
        || !(0x20000008..=ota::REQUEST_ADDR).contains(&sp)
        || entry & 1 == 0
        || entry < 0x08000000
        || entry - 0x08000000 >= boot.len() as u32
    {
        return Err("invalid bootloader vectors".into());
    }
    let mut packaged = header.encode().to_vec();
    packaged.extend_from_slice(&image);
    let hex = make_hex(&[
        (0x08000000, &boot),
        (ota::APP_ADDR, &image),
        (ota::APP_META, &header.encode()),
        (ota::STAGE_META, &[0xff; 32]),
    ])?;
    fs::create_dir_all(out)?;
    fs::write(out.join("coral_c5.ota"), packaged)?;
    fs::write(out.join("first_install.hex"), hex)?;
    Ok(header)
}
