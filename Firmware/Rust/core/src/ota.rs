// 固定容量的 OTA 编解码与镜像校验，使用与 C 工程相同的帧格式、小端字段和 CRC。

pub const BOARD_ID: u32 = 0xc5a3_0001;
pub const APP_ADDR: u32 = 0x0801_0000;
pub const STAGE_ADDR: u32 = 0x0808_0000;
pub const IMAGE_MAX: u32 = 448 * 1024;
pub const APP_META: u32 = 0x080f_c000;
pub const STAGE_META: u32 = 0x080f_e000;
pub const PAGE_SIZE: u32 = 8192;
pub const REQUEST_ADDR: u32 = 0x2003_ffe0;
pub const REQUEST_MAGIC: u32 = 0xc5b0_0701;
pub const HEADER_MAGIC: u32 = 0x3541_544f;
pub const CHUNK: usize = 128;
pub const PAYLOAD_MAX: usize = 132;
pub const FRAME_MAX: usize = 148;
pub const MAGIC: [u8; 4] = [0xc5, 0x5a, 0xb0, 0x07];
pub const HELLO: u8 = 1;
pub const BEGIN: u8 = 2;
pub const DATA: u8 = 3;
pub const END: u8 = 4;
pub const RUN: u8 = 5;
pub const ABORT: u8 = 6;
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u32)]
pub enum Status {
    BadArgument = 1,
    BadState = 2,
    BadOffset = 3,
    FlashError = 4,
    BadImage = 5,
}
pub type Result<T> = core::result::Result<T, Status>;
// 将当前字节段加入 CRC32 累计值。
pub fn crc_update(mut crc: u32, data: &[u8]) -> u32 {
    for &byte in data {
        crc ^= u32::from(byte);
        for _ in 0..8 {
            crc = (crc >> 1) ^ (0xedb8_8320 & 0u32.wrapping_sub(crc & 1));
        }
    }
    crc
}
// 为整段字节计算 CRC32。
pub fn crc(data: &[u8]) -> u32 {
    !crc_update(u32::MAX, data)
}
// 读取小端序的 32 位整数。
pub fn get32(bytes: &[u8]) -> u32 {
    u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]])
}
// 按小端序写回 32 位整数。
pub fn put32(bytes: &mut [u8], n: u32) {
    bytes[..4].copy_from_slice(&n.to_le_bytes());
}
#[derive(Clone, Copy, Default, Debug, PartialEq, Eq)]
pub struct Header {
    pub length: u32,
    pub crc: u32,
}
impl Header {
    // 将镜像元数据编码为固定长度包头。
    pub fn encode(self) -> [u8; 32] {
        let mut b = [0; 32];
        for (i, n) in [
            HEADER_MAGIC,
            1,
            BOARD_ID,
            APP_ADDR,
            self.length,
            self.crc,
            0,
        ]
        .iter()
        .enumerate()
        {
            put32(&mut b[i * 4..], *n);
        }
        let check = crc(&b[..28]);
        put32(&mut b[28..], check);
        b
    }
    // 解析并校验收到的镜像头。
    pub fn decode(b: &[u8]) -> Result<Self> {
        if b.len() != 32
            || get32(b) != HEADER_MAGIC
            || get32(&b[4..]) != 1
            || get32(&b[8..]) != BOARD_ID
            || get32(&b[12..]) != APP_ADDR
            || !(512..=IMAGE_MAX).contains(&get32(&b[16..]))
            || get32(&b[24..]) != 0
            || crc(&b[..28]) != get32(&b[28..])
        {
            return Err(Status::BadArgument);
        }
        Ok(Self {
            length: get32(&b[16..]),
            crc: get32(&b[20..]),
        })
    }
    // 根据应用内容和目标板信息构造包头。
    pub fn for_image(image: &[u8]) -> Result<Self> {
        if image.len() < 512
            || image.len() > IMAGE_MAX as usize
            || !vectors_valid(image, image.len() as u32)
        {
            return Err(Status::BadImage);
        }
        Ok(Self {
            length: image.len() as u32,
            crc: crc(image),
        })
    }
}
// 核对应用栈顶、复位入口与地址范围。
pub fn vectors_valid(b: &[u8], length: u32) -> bool {
    if b.len() < 8 {
        return false;
    }
    let sp = get32(b);
    let entry = get32(&b[4..]);
    (0x2000_0008..=REQUEST_ADDR).contains(&sp)
        && sp & 7 == 0
        && entry & 1 != 0
        && entry >= APP_ADDR
        && entry - APP_ADDR < length
}
#[derive(Clone, Debug)]
pub struct Frame {
    pub command: u8,
    pub sequence: u32,
    bytes: [u8; PAYLOAD_MAX],
    len: usize,
}
impl Frame {
    // 根据命令、序号和载荷构造一帧。
    pub fn new(command: u8, sequence: u32, payload: &[u8]) -> Result<Self> {
        if payload.len() > PAYLOAD_MAX {
            return Err(Status::BadArgument);
        }
        let mut value = Self {
            command,
            sequence,
            bytes: [0; PAYLOAD_MAX],
            len: payload.len(),
        };
        value.bytes[..payload.len()].copy_from_slice(payload);
        Ok(value)
    }
    // 取得帧内有效载荷切片。
    pub fn payload(&self) -> &[u8] {
        &self.bytes[..self.len]
    }
    // 把二进制帧写到调用方的缓冲区。
    pub fn encode(&self, b: &mut [u8; FRAME_MAX]) -> usize {
        b[..4].copy_from_slice(&MAGIC);
        b[4] = 1;
        b[5] = self.command;
        b[6..8].copy_from_slice(&(self.len as u16).to_le_bytes());
        put32(&mut b[8..], self.sequence);
        b[12..12 + self.len].copy_from_slice(self.payload());
        let check = crc(&b[..12 + self.len]);
        put32(&mut b[12 + self.len..], check);
        16 + self.len
    }
}
pub struct Parser {
    bytes: [u8; FRAME_MAX],
    used: usize,
}
impl Default for Parser {
    // 建立默认的空帧解析器。
    fn default() -> Self {
        Self::new()
    }
}
impl Parser {
    // 分配固定大小的接收缓冲并清零游标。
    pub const fn new() -> Self {
        Self {
            bytes: [0; FRAME_MAX],
            used: 0,
        }
    }
    // 舍弃已缓存的残帧。
    pub fn clear(&mut self) {
        self.used = 0;
    }
    // 判断是否还存有未消费的接收字节。
    pub fn is_empty(&self) -> bool {
        self.used == 0
    }
    // 移除失步的头部字节以重新对齐。
    fn discard(&mut self, n: usize) {
        self.bytes.copy_within(n..self.used, 0);
        self.used -= n;
    }
    // 逐字节接收并验证帧头、长度与 CRC。
    pub fn feed(&mut self, byte: u8) -> Option<Frame> {
        if self.used == FRAME_MAX {
            self.discard(1);
        }
        self.bytes[self.used] = byte;
        self.used += 1;
        while self.used >= 4 {
            if self.bytes[..4] != MAGIC {
                self.discard(1);
                continue;
            }
            if self.used < 12 {
                return None;
            }
            let len = usize::from(u16::from_le_bytes([self.bytes[6], self.bytes[7]]));
            if self.bytes[4] != 1 || len > PAYLOAD_MAX {
                self.discard(1);
                continue;
            }
            if self.used < 16 + len {
                return None;
            }
            if crc(&self.bytes[..12 + len]) != get32(&self.bytes[12 + len..]) {
                self.discard(1);
                continue;
            }
            let frame = Frame::new(
                self.bytes[5],
                get32(&self.bytes[8..]),
                &self.bytes[12..12 + len],
            )
            .ok();
            self.discard(16 + len);
            return frame;
        }
        None
    }
}
