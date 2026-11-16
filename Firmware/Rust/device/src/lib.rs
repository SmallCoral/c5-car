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
