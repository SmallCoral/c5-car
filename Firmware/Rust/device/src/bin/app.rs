#![cfg_attr(target_arch = "arm", no_std)]
#![cfg_attr(target_arch = "arm", no_main)]

#[cfg(target_arch = "arm")]
#[unsafe(no_mangle)]
pub extern "C" fn firmware_main() -> ! {
    coral_device::app::run()
}

// Keep native workspace checks usable without exposing MCU register access.
#[cfg(not(target_arch = "arm"))]
fn main() -> std::process::ExitCode {
    eprintln!(
        "coral-app is STM32 firmware and cannot run on this computer. \
         Build it from Firmware/Rust with `cargo app --locked` \
         (target: thumbv8m.main-none-eabihf)."
    );
    std::process::ExitCode::FAILURE
}
