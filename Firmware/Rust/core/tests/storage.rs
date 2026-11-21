// 把 Flash 擦页与写入替换为可注入断电的内存模型，验证下载、提交与恢复。

use coral_core::{
    ota::*,
    store::{self, Download, Flash},
};

const BASE: u32 = 0x08000000;
#[derive(Clone)]
struct Memory {
    bytes: Vec<u8>,
    programmed: Vec<bool>,
    operations: usize,
    cut: Option<usize>,
    torn: bool,
}
impl Memory {
    // 创建模拟 Flash 及掉电注入状态。
    fn new() -> Self {
        Self {
            bytes: vec![0xff; 1024 * 1024],
            programmed: vec![false; 65536],
            operations: 0,
            cut: None,
            torn: false,
        }
    }
    // 在指定擦写步骤触发模拟掉电。
    fn event(&mut self) -> bool {
        let cut = self.cut == Some(self.operations);
        self.operations += 1;
        cut
    }
    // 重置易失控制状态以模拟设备重新上电。
    fn recover(&mut self) {
        self.cut = None;
        self.operations = 0;
        self.torn = false;
    }
    // 把 Flash 物理地址换成模拟数组偏移。
    fn offset(address: u32, len: usize) -> Result<usize> {
        if !store::writable(address, len) {
            return Err(Status::FlashError);
        }
        Ok((address - BASE) as usize)
    }
}
impl Flash for Memory {
    // 从模拟存储中读取字节。
    fn read(&mut self, address: u32, data: &mut [u8]) -> Result<()> {
        let n = Self::offset(address, data.len())?;
        data.copy_from_slice(&self.bytes[n..n + data.len()]);
        Ok(())
    }
    // 模拟擦页及其中断。
    fn erase(&mut self, address: u32) -> Result<()> {
        let n = Self::offset(address, 16)?;
        assert!(address.is_multiple_of(PAGE_SIZE));
        let failed = self.event();
        let size = if failed {
            if self.torn { PAGE_SIZE as usize / 2 } else { 0 }
        } else {
            PAGE_SIZE as usize
        };
        self.bytes[n..n + size].fill(0xff);
        self.programmed[n / 16..(n + size) / 16].fill(false);
        if failed {
            Err(Status::FlashError)
        } else {
            Ok(())
        }
    }
    // 模拟四字编程及其中断。
    fn program(&mut self, address: u32, data: &[u8]) -> Result<()> {
        let base = Self::offset(address, data.len())?;
        assert!(address.is_multiple_of(16) && data.len().is_multiple_of(16));
        for (i, q) in data.as_chunks::<16>().0.iter().enumerate() {
            let n = base + i * 16;
            assert!(
                !self.programmed[n / 16],
                "quadword programmed twice at {n:x}"
            );
            let failed = self.event();
            let size = if failed {
                if self.torn { 8 } else { 0 }
            } else {
                16
            };
            for (dst, src) in self.bytes[n..n + size].iter_mut().zip(q) {
                assert_eq!(*dst, 0xff);
                *dst = *src;
            }
            if size != 0 {
                self.programmed[n / 16] = true;
            }
            if failed {
                return Err(Status::FlashError);
            }
        }
        Ok(())
    }
}
// 构造向量和内容有效的测试应用。
fn firmware(size: usize, pattern: u8) -> Vec<u8> {
    let mut b = vec![pattern; size];
    put32(&mut b, REQUEST_ADDR);
    put32(&mut b[4..], APP_ADDR + 0x241);
    b
}
// 通过真实下载状态机分块安装到暂存槽。
fn download(memory: &mut Memory, bytes: &[u8]) -> Result<Download> {
    let mut d = Download::default();
    d.begin(memory, Header::for_image(bytes)?)?;
    for (i, chunk) in bytes.chunks(CHUNK).enumerate() {
        d.data(memory, (i * CHUNK) as u32, chunk)?;
    }
    d.end(memory)?;
    Ok(d)
}
// 准备包含旧应用的测试基线。
fn baseline() -> Memory {
    let mut m = Memory::new();
    download(&mut m, &firmware(1024, 0x55)).unwrap();
    store::install(&mut m).unwrap();
    m.recover();
    m
}
#[test]
// 验证重复包幂等，且已提交暂存区不会被新数据改写。
fn duplicate_packets_are_idempotent_and_committed_stage_is_immutable() {
    let mut m = baseline();
    let b = firmware(777, 0x33);
    let h = Header::for_image(&b).unwrap();
    let mut d = Download::default();
    d.begin(&mut m, h).unwrap();
    d.data(&mut m, 0, &b[..CHUNK]).unwrap();
    let ops = m.operations;
    d.begin(&mut m, h).unwrap();
    d.data(&mut m, 0, &b[..CHUNK]).unwrap();
    assert_eq!(m.operations, ops);
    assert_eq!(d.received, 128);
    let mut different = b[..CHUNK].to_vec();
    different[12] ^= 1;
    assert_eq!(d.data(&mut m, 0, &different), Err(Status::BadOffset));
    assert_eq!(d.data(&mut m, 256, &b[..CHUNK]), Err(Status::BadOffset));
    assert_eq!(d.data(&mut m, 128, &b[..17]), Err(Status::BadArgument));
    for (i, c) in b.chunks(CHUNK).enumerate().skip(1) {
        d.data(&mut m, (i * CHUNK) as u32, c).unwrap();
    }
    d.end(&mut m).unwrap();
    let ops = m.operations;
    d.end(&mut m).unwrap();
    d.begin(&mut m, h).unwrap();
    assert_eq!(m.operations, ops);
    assert_eq!(d.abort(), Err(Status::BadState));
    assert_eq!(
        d.begin(
            &mut m,
            Header {
                crc: h.crc ^ 1,
                ..h
            }
        ),
        Err(Status::BadState)
    );
    assert_eq!(store::image(&mut m, STAGE_ADDR, STAGE_META), Ok(h));
}
#[test]
// 损坏镜像不能获得有效提交头。
fn invalid_image_never_publishes_metadata() {
    let mut m = baseline();
    let b = firmware(1024, 0x23);
    let mut d = Download::default();
    let mut h = Header::for_image(&b).unwrap();
    h.crc ^= 1;
    d.begin(&mut m, h).unwrap();
    for (i, c) in b.chunks(CHUNK).enumerate() {
        d.data(&mut m, (i * CHUNK) as u32, c).unwrap();
    }
    assert_eq!(d.end(&mut m), Err(Status::BadImage));
    assert!(store::image(&mut m, STAGE_ADDR, STAGE_META).is_err());
    assert_eq!(
        store::image(&mut m, APP_ADDR, APP_META).unwrap(),
        Header::for_image(&firmware(1024, 0x55)).unwrap()
    );
}
#[test]
// 遍历下载期间各个擦写断点，旧应用仍可用。
fn every_download_write_and_erase_can_lose_power_without_losing_old_app() {
    let base = baseline();
    let bytes = firmware(PAGE_SIZE as usize + 1157, 0x79);
    let old = Header::for_image(&firmware(1024, 0x55)).unwrap();
    let mut complete = base.clone();
    download(&mut complete, &bytes).unwrap();
    let operations = complete.operations;
    assert_eq!(operations, 590);
    for torn in [false, true] {
        for cut in 0..operations {
            let mut m = base.clone();
            m.cut = Some(cut);
            m.torn = torn;
            assert!(download(&mut m, &bytes).is_err(), "cut={cut} torn={torn}");
            m.recover();
            assert_eq!(store::image(&mut m, APP_ADDR, APP_META), Ok(old));
            assert!(store::image(&mut m, STAGE_ADDR, STAGE_META).is_err());
            download(&mut m, &bytes).unwrap();
            store::install(&mut m).unwrap();
            assert_eq!(
                store::image(&mut m, APP_ADDR, APP_META),
                Ok(Header::for_image(&bytes).unwrap())
            );
        }
    }
}
#[test]
// 逐个中断安装写操作，检查已提交暂存镜像可以恢复。
fn every_install_cut_recovers_from_committed_stage() {
    let bytes = firmware(PAGE_SIZE as usize + 1157, 0x79);
    let h = Header::for_image(&bytes).unwrap();
    let mut base = baseline();
    download(&mut base, &bytes).unwrap();
    base.recover();
    let mut complete = base.clone();
    store::install(&mut complete).unwrap();
    let operations = complete.operations;
    assert_eq!(operations, 591);
    for torn in [false, true] {
        for cut in 0..operations {
            let mut m = base.clone();
            m.cut = Some(cut);
            m.torn = torn;
            assert!(store::install(&mut m).is_err(), "cut={cut} torn={torn}");
            m.recover();
            if store::image(&mut m, STAGE_ADDR, STAGE_META).is_ok() {
                store::install(&mut m).unwrap();
            }
            assert_eq!(
                store::image(&mut m, APP_ADDR, APP_META),
                Ok(h),
                "cut={cut} torn={torn}"
            );
            assert!(store::image(&mut m, STAGE_ADDR, STAGE_META).is_err());
        }
    }
}
#[test]
// 尝试越界写引导区、保留间隙和请求邮箱，确认后端拒绝。
fn boot_partition_gap_and_request_ram_are_not_flash_write_targets() {
    for (address, len) in [
        (BASE, 16),
        (APP_ADDR - 16, 32),
        (APP_ADDR + IMAGE_MAX - 16, 32),
        (STAGE_ADDR + IMAGE_MAX, 16),
        (APP_META + 32, 16),
        (STAGE_META + 32, 16),
        (REQUEST_ADDR, 16),
        (u32::MAX, usize::MAX),
    ] {
        assert!(!store::writable(address, len));
    }
    assert!(store::writable(APP_ADDR, IMAGE_MAX as usize));
    assert!(store::writable(STAGE_META, 32));
}
