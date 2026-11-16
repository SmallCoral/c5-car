use crate::{TEST_MS, WHEEL_LIMIT, elapsed};

#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
#[repr(u8)]
pub enum Task {
    Monitor,
    Left,
    Right,
    Both,
    Stepper,
    #[default]
    All,
}
impl Task {
    pub fn from_id(id: u8) -> Option<Self> {
        Some(match id {
            0 => Self::Monitor,
            1 => Self::Left,
            2 => Self::Right,
            3 => Self::Both,
            4 => Self::Stepper,
            5 => Self::All,
            _ => return None,
        })
    }
    pub const fn name(self) -> &'static str {
        match self {
            Self::Monitor => "MONITOR",
            Self::Left => "LEFT",
            Self::Right => "RIGHT",
            Self::Both => "BOTH",
            Self::Stepper => "STEPPER",
            Self::All => "ALL",
        }
    }
    pub const fn wheels(self) -> bool {
        matches!(self, Self::Left | Self::Right | Self::Both | Self::All)
    }
    pub const fn stepper(self) -> bool {
        matches!(self, Self::Stepper | Self::All)
    }
    pub const fn continuous(self) -> bool {
        self.stepper()
    }
}
#[derive(Clone, Copy, Debug)]
#[repr(u8)]
pub enum Key {
    Task,
    Level,
    Direction,
    Run,
}
impl Key {
    pub fn from_id(id: u8) -> Option<Self> {
        Some(match id {
            0 => Self::Task,
            1 => Self::Level,
            2 => Self::Direction,
            3 => Self::Run,
            _ => return None,
        })
    }
}

#[derive(Clone, Copy, Default)]
pub struct Debounce {
    pub changed_at: u32,
    pub candidate: bool,
    pub stable: bool,
}
impl Debounce {
    pub fn held(down: bool, now: u32) -> Self {
        Self {
            changed_at: now,
            candidate: down,
            stable: down,
        }
    }
    pub fn update(&mut self, down: bool, now: u32) -> bool {
        if down != self.candidate {
            self.candidate = down;
            self.changed_at = now;
        }
        if self.stable == down || !elapsed(now, self.changed_at, 25) {
            return false;
        }
        self.stable = down;
        down
    }
}
#[derive(Clone, Copy, Debug)]
pub struct Controls {
    pub task: Task,
    pub level: u8,
    pub reverse: bool,
    pub running: bool,
    pub started_at: u32,
}
impl Default for Controls {
    fn default() -> Self {
        Self {
            task: Task::All,
            level: 3,
            reverse: false,
            running: false,
            started_at: 0,
        }
    }
}
impl Controls {
    pub fn key(&mut self, key: Key, now: u32) {
        if self.running {
            self.running = false;
            return;
        }
        match key {
            Key::Task => self.task = Task::from_id((self.task as u8 + 1) % 6).unwrap_or_default(),
            Key::Level => {
                self.level = (self.level + 1) % if self.task == Task::Stepper { 3 } else { 4 }
            }
            Key::Direction => self.reverse = !self.reverse,
            Key::Run if self.task != Task::Monitor => {
                self.running = true;
                self.started_at = now;
            }
            Key::Run => {}
        }
    }
    pub fn tick(&mut self, now: u32, fault: bool) {
        if fault || (!self.task.continuous() && elapsed(now, self.started_at, TEST_MS)) {
            self.running = false;
        }
    }
    pub fn duty(&self) -> i32 {
        let d = [100, 200, 300, WHEEL_LIMIT][usize::from(self.level % 4)];
        if self.reverse { -d } else { d }
    }
    pub fn rpm(&self) -> u16 {
        u16::from(self.level % 3 + 1) * 30
    }
}
pub fn encoder_delta(current: u32, previous: u32) -> i32 {
    current.wrapping_sub(previous) as i32
}

#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
#[repr(u8)]
pub enum Trip {
    #[default]
    Ok,
    Fault,
    Deadline,
    Stall,
    EnableLow,
}
#[derive(Default)]
pub struct WheelGuard {
    pub active: bool,
    pub continuous: bool,
    pub started: u32,
    pub heartbeat: u32,
    pub trip: Trip,
    pub faults: u8,
}
impl WheelGuard {
    pub const fn new() -> Self {
        Self {
            active: false,
            continuous: false,
            started: 0,
            heartbeat: 0,
            trip: Trip::Ok,
            faults: 0,
        }
    }
    pub fn start(&mut self, now: u32, faults: u8, continuous: bool) -> bool {
        *self = Self {
            active: faults == 0,
            continuous,
            started: now,
            heartbeat: now,
            trip: if faults == 0 { Trip::Ok } else { Trip::Fault },
            faults,
        };
        self.active
    }
    pub fn check(&mut self, now: u32, faults: u8, enable: bool) {
        if !self.active {
            return;
        }
        if elapsed(now, self.started, 5) {
            self.faults = faults;
            if !enable {
                self.trip = Trip::EnableLow;
            } else if faults != 0 {
                self.trip = Trip::Fault;
            }
        }
        if self.trip == Trip::Ok && elapsed(now, self.heartbeat, 100) {
            self.trip = Trip::Stall;
        }
        if self.trip == Trip::Ok && !self.continuous && elapsed(now, self.started, TEST_MS) {
            self.trip = Trip::Deadline;
        }
        if self.trip != Trip::Ok {
            self.active = false;
        }
    }
}
