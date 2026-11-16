#![no_std]
#![forbid(unsafe_code)]

pub mod controls;
pub mod emm;
pub mod ota;
pub mod store;
pub mod telemetry;
pub mod text;
pub mod ui;

pub const CLOCK_HZ: u32 = 48_000_000;
pub const BT_BAUD: u32 = 57_600;
pub const STEP_BAUD: u32 = 115_200;
pub const STEP_ADDRESS: u8 = 1;
pub const WHEEL_LIMIT: i32 = 450;
pub const TEST_MS: u32 = 3000;

pub const fn elapsed(now: u32, then: u32, interval: u32) -> bool {
    now.wrapping_sub(then) >= interval
}
