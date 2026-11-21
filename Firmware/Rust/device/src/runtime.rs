// Cortex-M 启动、临界区和异常入口，集中处理关中断、共享状态及故障停机。

use core::{
    arch::asm,
    cell::UnsafeCell,
    sync::atomic::{AtomicBool, Ordering},
};

core::arch::global_asm!(include_str!("startup.s"));

/// 即使闭包提前返回，也恢复进入时的 PRIMASK。适用于单核 Cortex-M。
pub struct InterruptGuard(u32);
impl InterruptGuard {
    // 保存进入时的中断掩码并屏蔽中断。
    pub fn lock() -> Self {
        let mask: u32;
        // SAFETY: 固件在特权模式运行，屏蔽中断不会使内存引用失效。
        unsafe {
            asm!("mrs {}, PRIMASK", "cpsid i",out(reg) mask,options(nostack,preserves_flags));
        }
        Self(mask)
    }
}
impl Drop for InterruptGuard {
    // 退出临界区时恢复原 PRIMASK。
    fn drop(&mut self) {
        // SAFETY: 恢复调用前的中断状态，包括从中断服务函数发起的调用。
        unsafe {
            asm!("msr PRIMASK, {}",in(reg) self.0,options(nostack,preserves_flags));
        }
    }
}
pub struct Shared<T> {
    value: UnsafeCell<T>,
    borrowed: AtomicBool,
}
// SAFETY: 所有访问均屏蔽中断并拒绝递归借用，NMI 不使用这些单元。
unsafe impl<T: Send> Sync for Shared<T> {}
impl<T> Shared<T> {
    // 初始化可在临界区借用的共享状态。
    pub const fn new(value: T) -> Self {
        Self {
            value: UnsafeCell::new(value),
            borrowed: AtomicBool::new(false),
        }
    }
    // 关闭中断并检查递归借用，再将独占引用临时交给闭包。
    pub fn with<R>(&self, f: impl FnOnce(&mut T) -> R) -> R {
        let _interrupts = InterruptGuard::lock();
        if self.borrowed.swap(true, Ordering::Relaxed) {
            crate::board::panic_stop();
        }
        // SAFETY: 临界区内中断已屏蔽且拒绝递归，引用的生命周期限于闭包 f。
        let value = f(unsafe { &mut *self.value.get() });
        self.borrowed.store(false, Ordering::Relaxed);
        value
    }
}
#[inline(always)]
// 等待先前内存访问完成。
pub fn dsb() {
    unsafe {
        asm!("dsb", options(nostack, preserves_flags));
    }
}
#[inline(always)]
// 刷新指令执行流水线。
pub fn isb() {
    unsafe {
        asm!("isb", options(nostack, preserves_flags));
    }
}
// 打开可屏蔽中断。
pub fn enable_interrupts() {
    unsafe {
        asm!("cpsie i", options(nostack, preserves_flags));
    }
}
// 让内核等待下一次中断。
pub fn wait() {
    unsafe {
        asm!("wfi", options(nomem, nostack, preserves_flags));
    }
}
// 通过系统控制寄存器请求复位。
pub fn reset() -> ! {
    dsb();
    crate::board::write(0xe000_ed0c, 0x05fa_0004);
    dsb();
    loop {
        wait();
    }
}
#[panic_handler]
// 将 panic 转入设备停机路径。
fn panic(_info: &core::panic::PanicInfo<'_>) -> ! {
    crate::board::panic_stop()
}
#[unsafe(no_mangle)]
// 统一处理没有专用服务程序的中断。
extern "C" fn DefaultHandler() -> ! {
    crate::board::panic_stop()
}
#[unsafe(no_mangle)]
// 在硬件硬故障时停止驱动。
extern "C" fn HardFault() -> ! {
    crate::board::panic_stop()
}
