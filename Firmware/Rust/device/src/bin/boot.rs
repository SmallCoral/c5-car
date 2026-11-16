#![cfg_attr(target_arch = "arm", no_std)]
#![cfg_attr(target_arch = "arm", no_main)]

#[cfg(target_arch = "arm")]
#[unsafe(no_mangle)]
pub extern "C" fn firmware_main() -> ! {
    coral_device::boot::run()
}

// Keep native workspace checks usable without exposing MCU register access.
#[cfg(not(target_arch = "arm"))]
fn main() -> std::process::ExitCode {
    eprintln!(
        "coral-boot is STM32 firmware and cannot run on this computer. \
         Build it from Firmware/Rust with `cargo boot --locked` \
         (target: thumbv8m.main-none-eabihf)."
    );
    std::process::ExitCode::FAILURE
}
