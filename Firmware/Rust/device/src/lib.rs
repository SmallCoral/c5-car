// Rust 设备层的模块入口，根据编译目标选择可用的裸机外设与运行时实现。

#![no_std]
#![cfg(target_arch = "arm")]
#![deny(unsafe_op_in_unsafe_fn)]

pub mod app;
mod board;
pub mod boot;
mod display;
mod flash;
mod registers;
mod runtime;
mod serial;
