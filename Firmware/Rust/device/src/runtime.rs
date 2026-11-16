use core::{
    arch::asm,
    cell::UnsafeCell,
    sync::atomic::{AtomicBool, Ordering},
};

core::arch::global_asm!(include_str!("startup.s"));

/// Restores PRIMASK even if a closure returns early. Single-core Cortex-M only.
pub struct InterruptGuard(u32);
impl InterruptGuard {
    pub fn lock() -> Self {
        let mask: u32;
        // SAFETY: privileged firmware; disabling interrupts does not invalidate memory.
        unsafe {
            asm!("mrs {}, PRIMASK", "cpsid i",out(reg) mask,options(nostack,preserves_flags));
        }
        Self(mask)
    }
}
impl Drop for InterruptGuard {
    fn drop(&mut self) {
        // SAFETY: restore exactly the entry interrupt state, including calls from an ISR.
        unsafe {
            asm!("msr PRIMASK, {}",in(reg) self.0,options(nostack,preserves_flags));
        }
    }
}
pub struct Shared<T> {
    value: UnsafeCell<T>,
    borrowed: AtomicBool,
}
// SAFETY: all access masks interrupts and rejects recursive borrowing; NMI never uses these cells.
unsafe impl<T: Send> Sync for Shared<T> {}
impl<T> Shared<T> {
    pub const fn new(value: T) -> Self {
        Self {
            value: UnsafeCell::new(value),
            borrowed: AtomicBool::new(false),
        }
    }
    pub fn with<R>(&self, f: impl FnOnce(&mut T) -> R) -> R {
        let _interrupts = InterruptGuard::lock();
        if self.borrowed.swap(true, Ordering::Relaxed) {
            crate::board::panic_stop();
        }
        // SAFETY: interrupts are masked, recursion is rejected, and a reference cannot escape f.
        let value = f(unsafe { &mut *self.value.get() });
        self.borrowed.store(false, Ordering::Relaxed);
        value
    }
}
#[inline(always)]
pub fn dsb() {
    unsafe {
        asm!("dsb", options(nostack, preserves_flags));
    }
}
#[inline(always)]
pub fn isb() {
    unsafe {
        asm!("isb", options(nostack, preserves_flags));
    }
}
pub fn enable_interrupts() {
    unsafe {
        asm!("cpsie i", options(nostack, preserves_flags));
    }
}
pub fn wait() {
    unsafe {
        asm!("wfi", options(nomem, nostack, preserves_flags));
    }
}
pub fn reset() -> ! {
    dsb();
    crate::board::write(0xe000_ed0c, 0x05fa_0004);
    dsb();
    loop {
        wait();
    }
}
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo<'_>) -> ! {
    crate::board::panic_stop()
}
#[unsafe(no_mangle)]
extern "C" fn DefaultHandler() -> ! {
    crate::board::panic_stop()
}
#[unsafe(no_mangle)]
extern "C" fn HardFault() -> ! {
    crate::board::panic_stop()
}
