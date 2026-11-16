use crate::{STEP_ADDRESS, elapsed};

pub trait Serial {
    fn write(&mut self, bytes: &[u8]) -> bool;
    fn read(&mut self) -> Option<u8>;
    fn rx_errors(&self) -> u32;
}
pub fn enable(on: bool) -> [u8; 6] {
    [STEP_ADDRESS, 0xf3, 0xab, u8::from(on), 0, 0x6b]
}
pub fn stop() -> [u8; 5] {
    [STEP_ADDRESS, 0xfe, 0x98, 0, 0x6b]
}
pub fn velocity(reverse: bool, rpm: u16) -> [u8; 8] {
    [
        STEP_ADDRESS,
        0xf6,
        u8::from(reverse),
        (rpm >> 8) as u8,
        rpm as u8,
        10,
        0,
        0x6b,
    ]
}
#[derive(Debug, PartialEq, Eq)]
pub struct Reply {
    pub command: u8,
    pub status: u8,
    pub value: i32,
}
#[derive(Default)]
pub struct Parser {
    bytes: [u8; 8],
    used: usize,
    last_byte: u32,
    pub rejected: u32,
}
impl Parser {
    pub fn clear(&mut self) {
        self.used = 0;
    }
    pub fn feed(&mut self, byte: u8, now: u32) -> Option<Reply> {
        if self.used != 0 && elapsed(now, self.last_byte, 30) {
            self.clear();
            self.rejected = self.rejected.wrapping_add(1);
        }
        self.last_byte = now;
        self.bytes[self.used] = byte;
        self.used += 1;
        while self.used != 0 {
            if self.bytes[0] == STEP_ADDRESS {
                if self.used < 2 {
                    return None;
                }
                let n = match self.bytes[1] {
                    0 | 0xf3 | 0xf6 | 0xfe | 0x3a => 4,
                    0x35 => 6,
                    0x36 => 8,
                    _ => 0,
                };
                if n != 0 {
                    if self.used < n {
                        return None;
                    }
                    let mut magnitude = 0u32;
                    for &b in &self.bytes[3..n - 1] {
                        magnitude = (magnitude << 8) | u32::from(b);
                    }
                    if self.bytes[n - 1] == 0x6b
                        && (n == 4 || self.bytes[2] <= 1)
                        && magnitude <= i32::MAX as u32
                    {
                        let reply = Reply {
                            command: self.bytes[1],
                            status: self.bytes[2],
                            value: if self.bytes[2] != 0 {
                                -(magnitude as i32)
                            } else {
                                magnitude as i32
                            },
                        };
                        self.clear();
                        return Some(reply);
                    }
                }
            }
            self.bytes.copy_within(1..self.used, 0);
            self.used -= 1;
            self.rejected = self.rejected.wrapping_add(1);
        }
        None
    }
}
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
#[repr(u8)]
pub enum Phase {
    #[default]
    Boot,
    Idle,
    Enable,
    Speed,
    Moving,
    Stop,
    Disable,
    Offline,
}
#[derive(Default)]
pub struct Stepper {
    parser: Parser,
    pub phase: Phase,
    pending: u8,
    pub flags: u8,
    pub error: u8,
    pub reverse: bool,
    seen: bool,
    sent_at: u32,
    last_reply: u32,
    last_poll: u32,
    rx_errors: u32,
    pub replies: u32,
    pub timeouts: u32,
    pub position: i32,
    pub position_valid: bool,
    poll_index: usize,
    cleanup_only: bool,
    pub target_rpm: u16,
    pub rpm: i32,
    pub rpm_valid: bool,
}
impl Stepper {
    pub fn online(&self, now: u32) -> bool {
        self.seen && !elapsed(now, self.last_reply, 1000)
    }
    pub fn running(&self) -> bool {
        matches!(self.phase, Phase::Enable | Phase::Speed | Phase::Moving)
    }
    pub fn name(&self, now: u32) -> &'static str {
        if self.phase == Phase::Boot {
            "BOOT"
        } else if !self.online(now) || self.phase == Phase::Offline {
            "NO LINK"
        } else if self.error != 0 || self.flags & 4 != 0 {
            "ERROR"
        } else if self.running() {
            "MOVING"
        } else if matches!(self.phase, Phase::Stop | Phase::Disable) {
            "STOPPING"
        } else {
            "READY"
        }
    }
    fn send(&mut self, io: &mut impl Serial, bytes: &[u8], now: u32) -> bool {
        if !io.write(bytes) {
            self.error = 0xe1;
            return false;
        }
        self.pending = bytes[1];
        self.sent_at = now;
        true
    }
    fn disable(&mut self, io: &mut impl Serial, now: u32) {
        self.phase = Phase::Disable;
        if !self.send(io, &enable(false), now) {
            self.phase = Phase::Offline;
            self.sent_at = now;
        }
    }
    pub fn stop(&mut self, io: &mut impl Serial, now: u32) {
        self.cleanup_only = matches!(self.phase, Phase::Boot | Phase::Offline);
        self.phase = Phase::Stop;
        self.pending = 0;
        if !self.send(io, &stop(), now) {
            self.disable(io, now);
        }
    }
    pub fn start(&mut self, io: &mut impl Serial, reverse: bool, rpm: u16, now: u32) -> bool {
        if self.phase != Phase::Idle
            || !self.online(now)
            || self.flags & 4 != 0
            || !(1..=90).contains(&rpm)
        {
            return false;
        }
        self.reverse = reverse;
        self.target_rpm = rpm;
        self.rpm_valid = false;
        self.error = 0;
        if !self.send(io, &enable(true), now) {
            return false;
        }
        self.phase = Phase::Enable;
        true
    }
    fn reply(&mut self, io: &mut impl Serial, r: Reply, now: u32) {
        self.replies = self.replies.wrapping_add(1);
        if self.pending == 0 || r.command != self.pending {
            return;
        }
        self.pending = 0;
        self.seen = true;
        self.last_reply = now;
        match r.command {
            0x3a => {
                self.flags = r.status;
                if self.running() && self.flags & 4 != 0 {
                    self.error = 0xe2;
                    self.stop(io, now);
                }
                return;
            }
            0x35 => {
                self.rpm = r.value;
                self.rpm_valid = true;
                return;
            }
            0x36 => {
                self.position = r.value;
                self.position_valid = true;
                return;
            }
            _ => {}
        }
        if r.status != 2 {
            self.error = if r.status == 0 { 0xee } else { r.status };
            match self.phase {
                Phase::Stop => self.disable(io, now),
                Phase::Disable => {
                    self.phase = Phase::Offline;
                    self.sent_at = now;
                }
                _ => self.stop(io, now),
            }
            return;
        }
        match self.phase {
            Phase::Enable => {
                self.phase = Phase::Speed;
                if !self.send(io, &velocity(self.reverse, self.target_rpm), now) {
                    self.stop(io, now);
                }
            }
            Phase::Speed => {
                self.phase = Phase::Moving;
                self.last_poll = now;
                self.poll_index = 0;
            }
            Phase::Stop => self.disable(io, now),
            Phase::Disable => {
                self.phase = Phase::Idle;
                self.flags &= !1;
                if self.cleanup_only {
                    self.error = 0;
                }
            }
            _ => {}
        }
    }
    pub fn poll(&mut self, io: &mut impl Serial, now: u32) {
        let errors = io.rx_errors();
        if errors != self.rx_errors {
            self.rx_errors = errors;
            self.parser.clear();
            while io.read().is_some() {}
            if self.running() {
                self.error = 0xe3;
                self.stop(io, now);
            }
        }
        while let Some(b) = io.read() {
            if let Some(r) = self.parser.feed(b, now) {
                self.reply(io, r, now);
            }
        }
        if self.pending != 0 && elapsed(now, self.sent_at, 200) {
            self.pending = 0;
            self.timeouts = self.timeouts.wrapping_add(1);
            self.error = 0xe0;
            match self.phase {
                Phase::Stop => self.disable(io, now),
                Phase::Disable | Phase::Idle => {
                    self.phase = Phase::Offline;
                    self.sent_at = now;
                }
                _ => self.stop(io, now),
            }
        }
        if (self.phase == Phase::Boot && now >= 1000)
            || (self.phase == Phase::Offline && elapsed(now, self.sent_at, 1000))
        {
            self.stop(io, now);
        }
        if self.pending != 0
            || !matches!(self.phase, Phase::Idle | Phase::Moving)
            || !elapsed(now, self.last_poll, 200)
        {
            return;
        }
        let query = [STEP_ADDRESS, [0x3a, 0x35, 0x36][self.poll_index], 0x6b];
        self.poll_index = (self.poll_index + 1) % 3;
        self.last_poll = now;
        if !self.send(io, &query, now) {
            self.stop(io, now);
        }
    }
}
