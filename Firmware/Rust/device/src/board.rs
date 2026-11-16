use crate::{
    registers::*,
    runtime::{self, InterruptGuard, Shared},
};
use coral_core::{
    CLOCK_HZ, WHEEL_LIMIT,
    controls::{Trip, WheelGuard},
    elapsed,
};
use core::sync::atomic::{AtomicU32, Ordering};

// Only internal driver modules may access MMIO. Addresses are pinned ST register constants.
#[inline(always)]
pub(crate) fn read(address: u32) -> u32 {
    unsafe { core::ptr::read_volatile(address as *const u32) }
}
#[inline(always)]
pub(crate) fn write(address: u32, value: u32) {
    unsafe { core::ptr::write_volatile(address as *mut u32, value) }
}
#[inline(always)]
pub(crate) fn modify(address: u32, clear: u32, set: u32) {
    write(address, (read(address) & !clear) | set);
}
pub(crate) fn write8(address: u32, value: u8) {
    unsafe { core::ptr::write_volatile(address as *mut u8, value) }
}
fn pin(port: u32, n: u32, high: bool) {
    write(port + GPIO_BSRR, 1 << (n + if high { 0 } else { 16 }));
}
fn mode(port: u32, n: u32, mode: u32, pull: u32) {
    modify(port + GPIO_PUPDR, 3 << (n * 2), pull << (n * 2));
    modify(port + GPIO_OTYPER, 1 << n, 0);
    modify(port + GPIO_OSPEEDR, 3 << (n * 2), 0);
    modify(port + GPIO_MODER, 3 << (n * 2), mode << (n * 2));
}
fn alternate(port: u32, n: u32, af: u32, pull: u32) {
    modify(
        port + GPIO_AFR + (n / 8) * 4,
        15 << ((n % 8) * 4),
        af << ((n % 8) * 4),
    );
    mode(port, n, 2, pull);
    modify(port + GPIO_OSPEEDR, 3 << (n * 2), 2 << (n * 2));
}
fn encoder_init(base: u32) {
    write(base + TIM_PSC, 0);
    write(base + TIM_ARR, u32::MAX);
    // Direct TI1/TI2, N8 digital filtering, rising polarity, quadrature x4.
    // LL encodes IC configuration in the upper halfword; CCMR1 uses bits 7:4.
    let filter = LL_TIM_IC_FILTER_FDIV1_N8 >> 16;
    write(base + TIM_CCMR1, 1 | (1 << 8) | filter | (filter << 8));
    write(base + TIM_SMCR, LL_TIM_CLK_ENCODER_X4_TI12);
    write(base + TIM_CCER, TIM_CCER_CC1E | TIM_CCER_CC2E);
    write(base + TIM_EGR, TIM_EGR_UG);
    write(base + TIM_CNT, 0);
    write(base + TIM_CR1, TIM_CR1_CEN);
}
static TICKS: AtomicU32 = AtomicU32::new(0);
struct Wheels {
    guard: WheelGuard,
    left: u32,
    right: u32,
}
static WHEELS: Shared<Wheels> = Shared::new(Wheels {
    guard: WheelGuard::new(),
    left: 0,
    right: 0,
});
pub fn init() -> bool {
    modify(RCC_BASE + RCC_AHB2ENR, 0, GPIO_CLOCKS);
    let _ = read(RCC_BASE + RCC_AHB2ENR);
    for (p, n) in [
        (GPIOA_BASE, 3),
        (GPIOA_BASE, 4),
        (GPIOC_BASE, 3),
        (GPIOC_BASE, 11),
        (GPIOE_BASE, 3),
    ] {
        pin(p, n, false);
        mode(p, n, 1, 0);
    }
    modify(
        RCC_BASE + RCC_CFGR2,
        RCC_CFGR2_HPRE | RCC_CFGR2_PPRE1 | RCC_CFGR2_PPRE2 | RCC_CFGR2_PPRE3,
        0,
    );
    if read(RCC_BASE + RCC_CFGR1) & RCC_CFGR1_SWS != 0
        || read(RCC_BASE + RCC_CR1) & RCC_CR1_HSIDIV3RDY == 0
    {
        return false;
    }
    for (p, n) in [
        (GPIOH_BASE, 4),
        (GPIOC_BASE, 10),
        (GPIOD_BASE, 15),
        (GPIOA_BASE, 8),
        (GPIOA_BASE, 10),
        (GPIOH_BASE, 3),
    ] {
        mode(p, n, 0, 1);
    }
    for n in 7..=14 {
        mode(GPIOE_BASE, n, 0, 0);
    }
    mode(GPIOH_BASE, 5, 3, 0);
    mode(GPIOC_BASE, 0, 3, 0);
    modify(RCC_BASE + RCC_APB1LENR, 0, APB1_CLOCKS);
    modify(RCC_BASE + RCC_APB2ENR, 0, APB2_CLOCKS);
    let _ = read(RCC_BASE + RCC_APB2ENR);
    modify(RCC_BASE + RCC_APB1LRSTR, 0, APB1_CLOCKS);
    modify(RCC_BASE + RCC_APB1LRSTR, APB1_CLOCKS, 0);
    modify(RCC_BASE + RCC_APB2RSTR, 0, APB2_CLOCKS);
    modify(RCC_BASE + RCC_APB2RSTR, APB2_CLOCKS, 0);
    alternate(GPIOA_BASE, 0, 2, 0);
    alternate(GPIOA_BASE, 1, 2, 0);
    alternate(GPIOB_BASE, 3, 1, 0);
    alternate(GPIOA_BASE, 15, 1, 0);
    encoder_init(TIM5_BASE);
    encoder_init(TIM2_BASE);
    write(TIM4_BASE + TIM_PSC, 0);
    write(TIM4_BASE + TIM_ARR, CLOCK_HZ / 20000 - 1);
    write(TIM4_BASE + TIM_CCMR1, LL_TIM_OCMODE_PWM1 | TIM_CCMR1_OC1PE);
    write(
        TIM4_BASE + TIM_CCMR2,
        (LL_TIM_OCMODE_PWM1 << 8) | TIM_CCMR2_OC4PE,
    );
    write(TIM4_BASE + TIM_CCR1, 0);
    write(TIM4_BASE + TIM_CCR4, 0);
    write(TIM4_BASE + TIM_CCER, TIM_CCER_CC1E | TIM_CCER_CC4E);
    write(TIM4_BASE + TIM_EGR, TIM_EGR_UG);
    write(TIM4_BASE + TIM_CR1, TIM_CR1_CEN);
    alternate(GPIOC_BASE, 2, 2, 0);
    alternate(GPIOB_BASE, 6, 2, 0);
    modify(
        RCC_BASE + RCC_CCIPR1,
        RCC_CCIPR1_USART1SEL | RCC_CCIPR1_USART6SEL | RCC_CCIPR1_SPI3SEL,
        0,
    );
    alternate(GPIOA_BASE, 6, 7, 0);
    alternate(GPIOA_BASE, 7, 7, 1);
    alternate(GPIOC_BASE, 6, 7, 0);
    alternate(GPIOC_BASE, 7, 7, 1);
    for (p, n, high) in [
        (GPIOC_BASE, 4, true),
        (GPIOC_BASE, 5, false),
        (GPIOB_BASE, 0, true),
    ] {
        pin(p, n, high);
        mode(p, n, 1, 0);
    }
    alternate(GPIOB_BASE, 1, 4, 0);
    alternate(GPIOB_BASE, 2, 7, 0);
    write(SPI3_BASE + SPI_CR1, SPI_CR1_SSI);
    write(SPI3_BASE + SPI_CFG1, SPI_CONFIG1);
    write(SPI3_BASE + SPI_CFG2, SPI_CONFIG2 | SPI_CFG2_AFCNTR);
    write8(0xe000_ed23, 0);
    write(0xe000_e014, CLOCK_HZ / 1000 - 1);
    write(0xe000_e018, 0);
    write(0xe000_e010, 7);
    runtime::enable_interrupts();
    true
}
pub fn millis() -> u32 {
    TICKS.load(Ordering::Relaxed)
}
pub fn delay(ms: u32) {
    let start = millis();
    while !elapsed(millis(), start, ms) {
        runtime::wait();
    }
}
pub fn keys() -> u8 {
    let low = |p: u32, n: u32| u8::from(read(p + GPIO_IDR) & (1u32 << n) == 0);
    low(GPIOD_BASE, 15)
        | (low(GPIOA_BASE, 8) << 1)
        | (low(GPIOA_BASE, 10) << 2)
        | (low(GPIOH_BASE, 3) << 3)
}
pub fn faults() -> u8 {
    u8::from(read(GPIOH_BASE + GPIO_IDR) & (1 << 4) == 0)
        | (u8::from(read(GPIOC_BASE + GPIO_IDR) & (1 << 10) == 0) << 1)
}
pub fn gray() -> u8 {
    (read(GPIOE_BASE + GPIO_IDR) >> 7) as u8
}
pub fn phases() -> u8 {
    let a = read(GPIOA_BASE + GPIO_IDR);
    (a & 3) as u8
        | (u8::from(read(GPIOB_BASE + GPIO_IDR) & 8 != 0) << 2)
        | (u8::from(a & (1 << 15) != 0) << 3)
}
pub fn encoder(side: usize) -> u32 {
    let base = if side == 0 { TIM5_BASE } else { TIM2_BASE };
    read(base + TIM_CNT)
}
pub fn led(on: bool) {
    pin(GPIOE_BASE, 3, on);
}
fn stop_outputs() {
    pin(GPIOA_BASE, 3, false);
    write(TIM4_BASE + TIM_CCR4, 0);
    write(TIM4_BASE + TIM_CCR1, 0);
}
pub fn wheels_stop() {
    WHEELS.with(|w| {
        stop_outputs();
        w.guard.active = false;
    });
}
pub fn heartbeat() {
    WHEELS.with(|w| w.guard.heartbeat = millis());
}
pub fn wheel_state() -> (bool, Trip, u8) {
    WHEELS.with(|w| (w.guard.active, w.guard.trip, w.guard.faults))
}
pub fn wheels_start(left: i32, right: i32, continuous: bool) -> bool {
    if !(-WHEEL_LIMIT..=WHEEL_LIMIT).contains(&left)
        || !(-WHEEL_LIMIT..=WHEEL_LIMIT).contains(&right)
    {
        return false;
    }
    WHEELS.with(|w| {
        stop_outputs();
        if !w.guard.start(millis(), faults(), continuous) {
            return false;
        }
        pin(GPIOC_BASE, 3, left >= 0);
        pin(GPIOC_BASE, 11, right >= 0);
        w.left = (CLOCK_HZ / 20000) * left.unsigned_abs() / 1000;
        w.right = (CLOCK_HZ / 20000) * right.unsigned_abs() / 1000;
        pin(GPIOA_BASE, 3, true);
        true
    })
}
fn check(w: &mut Wheels) {
    w.guard
        .check(millis(), faults(), read(GPIOA_BASE + GPIO_IDR) & 8 != 0);
    if !w.guard.active {
        stop_outputs();
    }
}
pub fn wheels_poll() {
    WHEELS.with(|w| {
        check(w);
        if w.guard.active && elapsed(millis(), w.guard.started, 5) {
            write(TIM4_BASE + TIM_CCR4, w.left);
            write(TIM4_BASE + TIM_CCR1, w.right);
        }
    });
}
#[unsafe(no_mangle)]
extern "C" fn SysTick() {
    TICKS.fetch_add(1, Ordering::Relaxed);
    WHEELS.with(check);
}
pub fn lcd_reset(on: bool) {
    pin(GPIOB_BASE, 0, on);
}
pub fn backlight(on: bool) {
    pin(GPIOA_BASE, 4, on);
}
pub fn lcd_write(data: bool, bytes: &[u8]) -> bool {
    if bytes.is_empty() || bytes.len() > 65535 {
        return false;
    }
    let start = millis();
    pin(GPIOC_BASE, 5, data);
    pin(GPIOC_BASE, 4, false);
    write(SPI3_BASE + SPI_CR2, bytes.len() as u32);
    modify(SPI3_BASE + SPI_CR1, 0, SPI_CR1_SPE);
    modify(SPI3_BASE + SPI_CR1, 0, SPI_CR1_CSTART);
    let mut offset = 0;
    while offset < bytes.len() && !elapsed(millis(), start, 20) {
        if read(SPI3_BASE + SPI_SR) & SPI_SR_TXP != 0 {
            write8(SPI3_BASE + SPI_TXDR, bytes[offset]);
            offset += 1;
        }
    }
    while read(SPI3_BASE + SPI_SR) & SPI_SR_EOT == 0 && !elapsed(millis(), start, 20) {
        core::hint::spin_loop();
    }
    let ok = offset == bytes.len() && read(SPI3_BASE + SPI_SR) & SPI_SR_EOT != 0;
    modify(SPI3_BASE + SPI_CR1, SPI_CR1_SPE, 0);
    write(SPI3_BASE + SPI_IFCR, SPI_IFCR_EOTC | SPI_IFCR_TXTFC);
    pin(GPIOC_BASE, 4, true);
    ok
}
pub fn panic_stop() -> ! {
    let _interrupts = InterruptGuard::lock();
    modify(RCC_BASE + RCC_AHB2ENR, 0, 1);
    let _ = read(RCC_BASE + RCC_AHB2ENR);
    pin(GPIOA_BASE, 3, false);
    mode(GPIOA_BASE, 3, 1, 0);
    loop {
        runtime::wait();
    }
}
