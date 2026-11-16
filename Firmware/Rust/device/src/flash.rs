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
pub fn read_bytes(address: u32, bytes: &mut [u8]) -> Result<()> {
    if !store::writable(address, bytes.len()) {
        return Err(Status::FlashError);
    }
    ECC_ERROR.store(false, Ordering::Relaxed);
    for (i, b) in bytes.iter_mut().enumerate() {
        // SAFETY: bounds checked against flash slots; byte loads do not require alignment.
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
extern "C" fn NMI() {
    let status = read(FLASH_R_BASE + FLASH_ECCDETR);
    if status & FLASH_ECCDETR_ECCD != 0 {
        ECC_ERROR.store(true, Ordering::Relaxed);
        write(FLASH_R_BASE + FLASH_ECCDETR, status | FLASH_ECCDETR_ECCD);
    } else {
        board::panic_stop();
    }
}
// The complete operation and literal pool execute from SRAM. No Rust panics,
// bounds checks, libc calls or flash-bank instruction fetches are permitted here.
#[unsafe(naked)]
#[unsafe(link_section = ".ramfunc")]
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
fn ready() -> bool {
    // SAFETY: read-only STM32 factory flash-size register, naturally aligned.
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
    fn read(&mut self, address: u32, bytes: &mut [u8]) -> Result<()> {
        read_bytes(address, bytes)
    }
    fn erase(&mut self, page: u32) -> Result<()> {
        if !page.is_multiple_of(PAGE_SIZE) || !store::writable(page, 16) || !ready() {
            return Err(Status::FlashError);
        }
        // SAFETY: aligned permitted page, no bank swap/cache, operation lives in SRAM.
        if unsafe { operation(page, core::ptr::null()) } == 0 {
            Err(Status::FlashError)
        } else {
            Ok(())
        }
    }
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
            // SAFETY: four aligned SRAM words and a range-checked flash quadword.
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
    fn progress(&mut self, done: u32, total: u32) {
        self.screen.update_bytes = done;
        self.screen.update_total = total;
        board::heartbeat();
        self.display.poll(self.screen, board::millis());
    }
}
