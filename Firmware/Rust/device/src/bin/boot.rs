// 组装 Rust 引导程序二进制，分别选择嵌入式和主机编译入口。

#![cfg_attr(target_arch = "arm", no_std)]
#![cfg_attr(target_arch = "arm", no_main)]

#[cfg(target_arch = "arm")]
#[unsafe(no_mangle)]
// 裸机引导入口，进入镜像检查与下载流程。
pub extern "C" fn firmware_main() -> ! {
    coral_device::boot::run()
}

// 主机编译时保留入口，寄存器访问只在 MCU 目标中启用。
#[cfg(not(target_arch = "arm"))]
// 为主机检查提供可链接的入口。
fn main() -> std::process::ExitCode {
    eprintln!(
        "coral-boot is STM32 firmware and cannot run on this computer. \
         Build it from Firmware/Rust with `cargo boot --locked` \
         (target: thumbv8m.main-none-eabihf)."
    );
    std::process::ExitCode::FAILURE
}
