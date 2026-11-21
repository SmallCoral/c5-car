// Rust 镜像存储的实物后端，集中约束可访问地址、对齐、擦写及 ECC 错误处理。

use crate::{
    board::{self, read, write},
    display::Display,
    registers::*,
    runtime,
};
use coral_core::{
    ota::*,
    store::{self, Flash},
};
use core::{
    arch::naked_asm,
    sync::atomic::{AtomicBool, Ordering},
};
static ECC_ERROR: AtomicBool = AtomicBool::new(false);
// 在允许的槽内逐字节读 Flash，并报告 ECC 读取故障。
pub fn read_bytes(address: u32, bytes: &mut [u8]) -> Result<()> {
    if !store::writable(address, bytes.len()) {
        return Err(Status::FlashError);
    }
    ECC_ERROR.store(false, Ordering::Relaxed);
    for (i, b) in bytes.iter_mut().enumerate() {
        // SAFETY: 已按 Flash 分区校验范围，逐字节读取允许地址不对齐。
        *b = unsafe { core::ptr::read_volatile((address + i as u32) as *const u8) };
    }
    runtime::dsb();
    if ECC_ERROR.load(Ordering::Relaxed) {
        Err(Status::FlashError)
    } else {
        Ok(())
    }
}
#[unsafe(no_mangle)]
// 处理 Flash 读出 ECC 错误或进入不可恢复停机。
extern "C" fn NMI() {
    let status = read(FLASH_R_BASE + FLASH_ECCDETR);
    if status & FLASH_ECCDETR_ECCD != 0 {
        ECC_ERROR.store(true, Ordering::Relaxed);
        write(FLASH_R_BASE + FLASH_ECCDETR, status | FLASH_ECCDETR_ECCD);
    } else {
        board::panic_stop();
    }
}
// 整个擦写过程及其常量池均从 SRAM 执行。此处须避免 panic、
// 边界检查、库函数调用，以及从正在擦写的 Flash 存储区取指。
#[unsafe(naked)]
#[unsafe(link_section = ".ramfunc")]
// 从 SRAM 发起 Flash 擦除或编程，等待完成并核对错误位。
unsafe extern "C" fn operation(_address: u32, _words: *const u32) -> u32 {
    naked_asm!(
        "push {{r4-r8, lr}}", "mov r6, r0", "mov r7, r1", "mrs r5, PRIMASK", "cpsid i",
        "ldr r4, ={base}", "ldr r0, ={key1}", "str r0, [r4, #{keyr}]", "ldr r0, ={key2}", "str r0, [r4, #{keyr}]",
        "ldr r0, [r4, #{cr}]", "tst r0, #{lock}", "bne 8f",
        "ldr r0, ={clear}", "str r0, [r4, #{ccr}]", "cmp r7, #0", "beq 2f",
        "mov r0, #{pg}", "str r0, [r4, #{cr}]", "ldmia r7, {{r0-r3}}", "stmia r6, {{r0-r3}}", "b 3f",
        "2:", "ubfx r0, r6, #13, #6", "lsl r0, r0, #{pnb}", "ldr r1, ={stage}", "cmp r6, r1", "it hs", "orrhs r0, r0, #{bank}",
        "ldr r1, ={erase}", "orr r0, r0, r1", "str r0, [r4, #{cr}]",
        "3:", "dsb", "ldr r8, =10000000", "ldr r2, ={busy}",
        "4:", "ldr r0, [r4, #{sr}]", "tst r0, r2", "beq 6f", "subs r8, r8, #1", "bne 4b",
        "ldr r0, =0xe000ed0c", "ldr r1, =0x05fa0004", "str r1, [r0]", "dsb", "5:", "nop", "b 5b",
        "6:", "ldr r1, ={errors}", "ands r3, r0, r1", "ldr r0, ={clear}", "str r0, [r4, #{ccr}]",
        "mov r0, #{lock}", "str r0, [r4, #{cr}]", "dsb", "isb", "movs r0, #0", "cmp r3, #0", "it eq", "moveq r0, #1", "b 9f",
        "8:", "movs r0, #0", "9:", "msr PRIMASK, r5", "pop {{r4-r8, pc}}", ".ltorg",
        base=const FLASH_R_BASE,key1=const LL_FLASH_KEY1,key2=const LL_FLASH_KEY2,keyr=const FLASH_KEYR,
        cr=const FLASH_CR,lock=const FLASH_CR_LOCK,clear=const (LL_FLASH_FLAG_ERRORS_ALL|LL_FLASH_FLAG_EOP),ccr=const FLASH_CCR,
        pg=const FLASH_CR_PG,pnb=const FLASH_CR_PNB_POS,stage=const STAGE_ADDR,bank=const FLASH_CR_BKSEL,
        erase=const (FLASH_CR_PER|FLASH_CR_STRT),busy=const LL_FLASH_FLAG_STATUS_ALL,sr=const FLASH_SR,errors=const LL_FLASH_FLAG_ERRORS_ALL,
    );
}
// 检查 Flash 实际条件能否支持固定分区。
fn ready() -> bool {
    // SAFETY: STM32 出厂容量寄存器只读，并按自然边界对齐。
    let size = unsafe { core::ptr::read_volatile(FLASHSIZE_BASE as *const u16) };
    (size == 1024 || size == 0xffff)
        && read(FLASH_R_BASE + FLASH_OPTSR_CUR) & FLASH_OPTSR_PRG_SWAP_BANK == 0
        && read(ICACHE_BASE) & ICACHE_CR_EN == 0
        && read(FLASH_R_BASE + FLASH_SR) & LL_FLASH_FLAG_STATUS_ALL == 0
}
pub struct Storage<'a> {
    pub display: &'a mut Display,
    pub screen: &'a mut coral_core::ui::State,
}
impl Flash for Storage<'_> {
    // 按存储后端接口把指定地址读入缓冲。
    fn read(&mut self, address: u32, bytes: &mut [u8]) -> Result<()> {
        read_bytes(address, bytes)
    }
    // 校验地址和页对齐后擦除目标页。
    fn erase(&mut self, page: u32) -> Result<()> {
        if !page.is_multiple_of(PAGE_SIZE) || !store::writable(page, 16) || !ready() {
            return Err(Status::FlashError);
        }
        // SAFETY: 页地址已对齐且在许可区域内，已确认存储区映射未交换且指令缓存已关闭，擦写过程位于 SRAM。
        if unsafe { operation(page, core::ptr::null()) } == 0 {
            Err(Status::FlashError)
        } else {
            Ok(())
        }
    }
    // 检查地址与长度均为 16 字节对齐后，将数据分为 16 字节一组写入允许的 Flash 区域。
    fn program(&mut self, address: u32, bytes: &[u8]) -> Result<()> {
        if bytes.is_empty()
            || !address.is_multiple_of(16)
            || !bytes.len().is_multiple_of(16)
            || !store::writable(address, bytes.len())
            || !ready()
        {
            return Err(Status::FlashError);
        }
        for (index, quad) in bytes.as_chunks::<16>().0.iter().enumerate() {
            let words = [
                get32(quad),
                get32(&quad[4..]),
                get32(&quad[8..]),
                get32(&quad[12..]),
            ];
            let dest = address + index as u32 * 16;
            // SAFETY: 源是 SRAM 中四个对齐的字，目标是已检查范围的 Flash 四字。
            if unsafe { operation(dest, words.as_ptr()) } == 0 {
                return Err(Status::FlashError);
            }
            let mut check = [0; 16];
            read_bytes(dest, &mut check)?;
            if check != *quad {
                return Err(Status::FlashError);
            }
        }
        Ok(())
    }
    // 向升级界面或上层回调报告已处理字节数。
    fn progress(&mut self, done: u32, total: u32) {
        self.screen.update_bytes = done;
        self.screen.update_total = total;
        board::heartbeat();
        self.display.poll(self.screen, board::millis());
    }
}
