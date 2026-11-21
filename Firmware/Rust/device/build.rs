// Cargo 构建入口，为嵌入式应用与引导程序设置链接参数。

use std::{env, fs, path::PathBuf};
// 为 MCU 目标选用对应的链接脚本，生成正确的内存布局。
fn main() {
    println!("cargo:rerun-if-changed=link.x");
    if env::var("CARGO_CFG_TARGET_ARCH").as_deref() != Ok("arm") {
        return;
    }
    let out = PathBuf::from(env::var_os("OUT_DIR").expect("OUT_DIR"));
    let template = fs::read_to_string("link.x").expect("link.x");
    for (name, base, size) in [
        ("coral-app", "0x08010000", "448K"),
        ("coral-boot", "0x08000000", "64K"),
    ] {
        let path = out.join(format!("{name}.x"));
        fs::write(
            &path,
            template.replace("@ORIGIN@", base).replace("@LENGTH@", size),
        )
        .expect("write linker script");
        println!("cargo:rustc-link-arg-bin={name}=-T{}", path.display());
        println!("cargo:rustc-link-arg-bin={name}=--nmagic");
    }
}
