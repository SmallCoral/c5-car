use crate::board;
use coral_core::{
    elapsed,
    ui::{self, BYTES, HEIGHT, State, WIDTH},
};
use core::sync::atomic::{AtomicBool, Ordering};
static TAKEN: AtomicBool = AtomicBool::new(false);
static mut PIXELS: [u8; BYTES] = [0; BYTES];
pub struct Display {
    pixels: &'static mut [u8; BYTES],
    frame_time: u32,
    stripe_time: u32,
    next_y: usize,
    pub ready: bool,
}
fn command(cmd: u8, data: &[u8]) -> bool {
    board::lcd_write(false, &[cmd]) && (data.is_empty() || board::lcd_write(true, data))
}
impl Display {
    pub fn new() -> Self {
        if TAKEN.swap(true, Ordering::Relaxed) {
            board::panic_stop();
        }
        // SAFETY: the framebuffer is taken once, only used by the thread, never by interrupts.
        Self {
            pixels: unsafe { &mut *core::ptr::addr_of_mut!(PIXELS) },
            frame_time: 0,
            stripe_time: 0,
            next_y: HEIGHT,
            ready: false,
        }
    }
    pub fn init(&mut self) -> bool {
        const INIT: &[u8] = &[
            0xb1, 3, 5, 0x3c, 0x3c, 0xb2, 3, 5, 0x3c, 0x3c, 0xb3, 6, 5, 0x3c, 0x3c, 5, 0x3c, 0x3c,
            0xb4, 1, 3, 0xc0, 3, 0x28, 8, 4, 0xc1, 1, 0xc0, 0xc2, 2, 0x0d, 0, 0xc3, 2, 0x8d, 0x2a,
            0xc4, 2, 0x8d, 0xee, 0xc5, 1, 0x1a, 0x36, 1, 0x00, 0xe0, 16, 4, 0x22, 7, 0x0a, 0x2e,
            0x30, 0x25, 0x2a, 0x28, 0x26, 0x2e, 0x3a, 0, 1, 3, 0x13, 0xe1, 16, 4, 0x16, 6, 0x0d,
            0x2d, 0x26, 0x23, 0x27, 0x27, 0x25, 0x2d, 0x3b, 0, 1, 4, 0x13, 0x3a, 1, 5, 0x29, 0,
        ];
        board::backlight(false);
        board::lcd_reset(false);
        board::delay(100);
        board::lcd_reset(true);
        board::delay(100);
        if !command(0x11, &[]) {
            return false;
        }
        board::delay(120);
        let mut i = 0;
        while i < INIT.len() {
            let cmd = INIT[i];
            let len = usize::from(INIT[i + 1]);
            i += 2;
            if !command(cmd, &INIT[i..i + len]) {
                return false;
            }
            i += len;
        }
        self.ready = true;
        true
    }
    pub fn poll(&mut self, state: &State, now: u32) -> bool {
        if !self.ready {
            return false;
        }
        if self.next_y == HEIGHT {
            if !elapsed(now, self.frame_time, 200) {
                return true;
            }
            ui::render(self.pixels, state);
            self.frame_time = now;
            self.next_y = 0;
        }
        if !elapsed(now, self.stripe_time, 5) {
            return true;
        }
        self.stripe_time = now;
        let rows = [0, (self.next_y + 1) as u8, 0, (self.next_y + 8) as u8];
        let start = self.next_y * WIDTH * 2;
        if !command(0x2a, &[0, 2, 0, 129])
            || !command(0x2b, &rows)
            || !command(0x2c, &self.pixels[start..start + WIDTH * 8 * 2])
        {
            self.ready = false;
            board::backlight(false);
            return false;
        }
        self.next_y += 8;
        if self.next_y == HEIGHT {
            board::backlight(true);
        }
        true
    }
}
