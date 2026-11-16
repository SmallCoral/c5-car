use crate::{controls::Task, text::Text};
use core::fmt::Write;
mod font {
    include!("font.rs");
}
pub const WIDTH: usize = 128;
pub const HEIGHT: usize = 160;
pub const BYTES: usize = WIDTH * HEIGHT * 2;
const BG: u16 = 0x0862;
const PANEL: u16 = 0x10e4;
const EDGE: u16 = 0x21a7;
const MUTED: u16 = 0x8cb3;
const WHITE: u16 = 0xef9e;
const CYAN: u16 = 0x2e7d;
const GREEN: u16 = 0x47b2;
const AMBER: u16 = 0xfd48;
const RED: u16 = 0xfacc;
const DARK_GREEN: u16 = 0x1249;
#[derive(Clone)]
pub struct State {
    pub task: Task,
    pub running: bool,
    pub reverse: bool,
    pub duty: u32,
    pub remaining_ms: u32,
    pub encoder: [i32; 2],
    pub counts_per_second: [i32; 2],
    pub phases: u8,
    pub gray: u8,
    pub faults: u8,
    pub wheel_faults: u8,
    pub wheel_requested: bool,
    pub wheel_enable_fault: bool,
    pub step_online: bool,
    pub step_status: &'static str,
    pub step_error: u8,
    pub bt_tx: bool,
    pub step_target_rpm: u16,
    pub step_rpm: i32,
    pub step_rpm_valid: bool,
    pub updating: bool,
    pub update_status: &'static str,
    pub update_bytes: u32,
    pub update_total: u32,
}
impl Default for State {
    fn default() -> Self {
        Self {
            task: Task::All,
            running: false,
            reverse: false,
            duty: 450,
            remaining_ms: 0,
            encoder: [0; 2],
            counts_per_second: [0; 2],
            phases: 0,
            gray: 0,
            faults: 0,
            wheel_faults: 0,
            wheel_requested: false,
            wheel_enable_fault: false,
            step_online: false,
            step_status: "BOOT",
            step_error: 0,
            bt_tx: false,
            step_target_rpm: 30,
            step_rpm: 0,
            step_rpm_valid: false,
            updating: false,
            update_status: "WAITING FOR PC",
            update_bytes: 0,
            update_total: 0,
        }
    }
}
struct Canvas<'a>(&'a mut [u8; BYTES]);
impl Canvas<'_> {
    fn rect(&mut self, x: i32, y: i32, w: i32, h: i32, c: u16) {
        for row in y.max(0)..(y + h).min(HEIGHT as i32) {
            for col in x.max(0)..(x + w).min(WIDTH as i32) {
                let offset = (row as usize * WIDTH + col as usize) * 2;
                self.0[offset..offset + 2].copy_from_slice(&c.to_be_bytes());
            }
        }
    }
    fn card(&mut self, x: i32, y: i32, w: i32, h: i32, c: u16) {
        self.rect(x + 2, y, w - 4, h, c);
        self.rect(x, y + 2, w, h - 4, c);
    }
    fn text(&mut self, mut x: i32, y: i32, s: &str, scale: i32, c: u16) {
        for byte in s.bytes() {
            if x >= WIDTH as i32 {
                break;
            }
            let index = font::CHARS
                .iter()
                .position(|&b| b == byte)
                .unwrap_or(font::CHARS.len() - 2);
            for row in 0..7 {
                for col in 0..5 {
                    if font::GLYPHS[index][row] & (1 << (4 - col)) != 0 {
                        self.rect(x + col * scale, y + row as i32 * scale, scale, scale, c);
                    }
                }
            }
            x += 6 * scale;
        }
    }
    fn formatted(&mut self, x: i32, y: i32, args: core::fmt::Arguments<'_>, scale: i32, c: u16) {
        let mut text = Text::<48>::new();
        if text.write_fmt(args).is_ok() {
            self.text(x, y, text.as_str(), scale, c);
        }
    }
    fn wheel(&mut self, s: &State, side: usize) {
        let x = if side == 0 { 4 } else { 66 };
        let color = if s.faults & (1 << side) != 0 {
            RED
        } else {
            CYAN
        };
        self.card(x, 66, 58, 44, PANEL);
        self.rect(x + 6, 72, 2, 6, color);
        self.text(
            x + 12,
            72,
            if side == 0 { "LEFT" } else { "RIGHT" },
            1,
            MUTED,
        );
        for phase in 0..2 {
            self.rect(
                x + 49 + phase * 4,
                73,
                3,
                3,
                if s.phases & (1 << (side * 2 + phase as usize)) != 0 {
                    CYAN
                } else {
                    EDGE
                },
            );
        }
        let mut label = Text::<24>::new();
        let _ = write!(label, "{}", s.encoder[side]);
        let scale = if label.as_bytes().len() <= 4 { 2 } else { 1 };
        self.text(
            x + 6,
            83,
            if label.as_bytes().len() > 8 {
                "SEE PC"
            } else {
                label.as_str()
            },
            scale,
            WHITE,
        );
        label.clear();
        let _ = write!(label, "{}/S", s.counts_per_second[side]);
        self.text(
            x + 6,
            101,
            if label.as_bytes().len() > 8 {
                "FAST"
            } else {
                label.as_str()
            },
            1,
            color,
        );
    }
}
pub fn render(pixels: &mut [u8; BYTES], s: &State) {
    let mut p = Canvas(pixels);
    p.rect(0, 0, WIDTH as i32, HEIGHT as i32, BG);
    if s.updating {
        let percent = if s.update_total == 0 {
            0
        } else {
            (u64::from(s.update_bytes) * 100 / u64::from(s.update_total)).min(100) as u32
        };
        p.text(6, 8, "CORAL / WIRELESS", 1, CYAN);
        p.text(6, 29, "UPDATE", 2, WHITE);
        p.card(4, 54, 120, 63, PANEL);
        p.text(10, 62, s.update_status, 1, CYAN);
        p.formatted(10, 78, format_args!("{percent}%"), 2, WHITE);
        p.rect(10, 102, 108, 5, EDGE);
        p.rect(10, 102, (108 * percent / 100) as i32, 5, GREEN);
        p.text(6, 127, "MOTORS STOPPED", 1, MUTED);
        p.text(6, 145, "KEEP POWER ON", 1, AMBER);
        return;
    }
    for (index, mut label) in ["RUN", "DIR", "PWR", "TASK"].into_iter().enumerate() {
        if index == 0 && s.running {
            label = "STOP";
        }
        if index == 2 {
            if s.task == Task::Stepper {
                label = "SPD";
            } else if s.task == Task::All {
                label = "LVL";
            }
        }
        p.card(index as i32 * 32, 0, 30, 13, PANEL);
        p.text(index as i32 * 32 + 3, 3, label, 1, MUTED);
    }
    let faults = s.faults | s.wheel_faults;
    let fault = faults != 0 || s.wheel_enable_fault;
    let status = if fault {
        "ERR"
    } else if s.running {
        "RUN"
    } else {
        "IDLE"
    };
    p.text(5, 19, "CORAL / C5", 1, CYAN);
    p.text(105, 19, "BT", 1, if s.bt_tx { CYAN } else { MUTED });
    p.rect(121, 20, 3, 3, if s.bt_tx { CYAN } else { EDGE });
    p.text(4, 34, s.task.name(), 2, WHITE);
    p.card(94, 34, 30, 13, if s.running { DARK_GREEN } else { PANEL });
    p.text(
        97,
        37,
        status,
        1,
        if fault {
            RED
        } else if s.running {
            GREEN
        } else {
            AMBER
        },
    );
    match s.task {
        Task::All => p.formatted(
            5,
            53,
            format_args!(
                "{}% {}RPM {}",
                s.duty / 10,
                s.step_target_rpm,
                if s.reverse { "R" } else { "F" }
            ),
            1,
            MUTED,
        ),
        Task::Stepper => p.formatted(
            5,
            53,
            format_args!(
                "{}RPM {}",
                s.step_target_rpm,
                if s.reverse { "CCW" } else { "CW" }
            ),
            1,
            MUTED,
        ),
        _ => p.formatted(
            5,
            53,
            format_args!(
                "{}%  {}",
                s.duty / 10,
                if s.reverse { "REV" } else { "FWD" }
            ),
            1,
            MUTED,
        ),
    }
    if s.task.continuous() {
        p.text(97, 53, "CONT", 1, if s.running { GREEN } else { MUTED });
    } else if s.running {
        p.formatted(
            97,
            53,
            format_args!("{}.{}S", s.remaining_ms / 1000, s.remaining_ms / 100 % 10),
            1,
            GREEN,
        );
    }
    p.wheel(s, 0);
    p.wheel(s, 1);
    p.rect(5, 117, 3, 5, if s.step_online { GREEN } else { AMBER });
    p.text(13, 116, "EMM", 1, MUTED);
    if s.task.stepper() && s.running && s.step_rpm_valid && s.step_error == 0 {
        p.formatted(38, 116, format_args!("{:+}RPM", s.step_rpm), 1, GREEN);
    } else if s.step_error != 0 {
        p.formatted(38, 116, format_args!("ERR {:02X}", s.step_error), 1, RED);
    } else {
        p.text(
            38,
            116,
            s.step_status,
            1,
            if s.step_online { GREEN } else { AMBER },
        );
    }
    p.text(5, 129, "GRAY", 1, MUTED);
    for i in 0..8 {
        let x = 36 + i * 11;
        let low = s.gray & (1 << i) == 0;
        p.card(x, 127, 9, 12, if low { CYAN } else { PANEL });
        p.formatted(
            x + 2,
            130,
            format_args!("{}", i + 1),
            1,
            if low { BG } else { MUTED },
        );
    }
    p.rect(4, 144, 120, 1, EDGE);
    let c = if fault { RED } else { MUTED };
    if faults != 0 {
        p.formatted(
            5,
            150,
            format_args!("FAULT L:{} R:{}", faults & 1, (faults >> 1) & 1),
            1,
            c,
        );
    } else if s.wheel_enable_fault {
        p.text(5, 150, "ENABLE LOW / PA3", 1, c);
    } else if s.task.continuous() && s.running {
        p.text(5, 150, "ANY KEY TO STOP", 1, c);
    } else if s.wheel_requested {
        p.formatted(
            5,
            150,
            format_args!(
                "EN REQ  AB:{}{} {}{}",
                s.phases & 1,
                (s.phases >> 1) & 1,
                (s.phases >> 2) & 1,
                (s.phases >> 3) & 1
            ),
            1,
            c,
        );
    } else {
        p.text(
            5,
            150,
            if s.task == Task::All {
                "RUN: ALL MOTORS"
            } else {
                "WHEELS OFF / J10 ?"
            },
            1,
            c,
        );
    }
}
