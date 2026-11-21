// 将 Rust 应用二进制接到 MCU 的复位入口，并为主机检查保留独立入口。

#![cfg_attr(target_arch = "arm", no_std)]
#![cfg_attr(target_arch = "arm", no_main)]

#[cfg(target_arch = "arm")]
#[unsafe(no_mangle)]
// 裸机应用入口，把控制权交给设备应用主循环。
pub extern "C" fn firmware_main() -> ! {
    coral_device::app::run()
}

// 主机编译时保留入口，寄存器访问只在 MCU 目标中启用。
#[cfg(not(target_arch = "arm"))]
// 为主机检查提供可链接的入口。
fn main() -> std::process::ExitCode {
    eprintln!(
        "coral-app is STM32 firmware and cannot run on this computer. \
         Build it from Firmware/Rust with `cargo app --locked` \
         (target: thumbv8m.main-none-eabihf)."
    );
    std::process::ExitCode::FAILURE
}
