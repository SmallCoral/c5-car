use crate::{
    board,
    display::Display,
    flash, runtime,
    serial::{self, Id, StepSerial},
};
use coral_core::{
    TEST_MS,
    controls::{Controls, Debounce, Key, Task, Trip, encoder_delta},
    elapsed,
    emm::{Phase, Stepper},
    ota,
    telemetry::Record,
    text::Text,
    ui::State,
};
use core::fmt::Write;
#[cfg(debug_assertions)]
use core::sync::atomic::{AtomicU32, Ordering};
#[cfg(debug_assertions)]
#[allow(non_upper_case_globals)]
#[unsafe(no_mangle)]
pub static debug_key_request: AtomicU32 = AtomicU32::new(0);
#[cfg(debug_assertions)]
#[allow(non_upper_case_globals)]
#[unsafe(no_mangle)]
pub static debug_stop_reason: AtomicU32 = AtomicU32::new(0);
fn trace(reason: u32) {
    #[cfg(debug_assertions)]
    debug_stop_reason.store(reason, Ordering::Relaxed);
    #[cfg(not(debug_assertions))]
    let _ = reason;
}

#[derive(Default)]
struct App {
    controls: Controls,
    keys: [Debounce; 4],
    step: Stepper,
    ui: State,
    previous: [u32; 2],
    sampled_at: u32,
    telemetry_at: u32,
    sequence: u32,
    wheels_started: bool,
    upgrade: bool,
    upgrade_at: u32,
    remote: [u8; 24],
    remote_used: usize,
    remote_overflow: bool,
}
impl App {
    fn stop(&mut self, now: u32) {
        board::wheels_stop();
        if self.step.running() {
            self.step.stop(&mut StepSerial, now);
        }
        self.controls.running = false;
        self.wheels_started = false;
    }
    fn start_wheels(&mut self) -> bool {
        let c = &self.controls;
        let duty = c.duty();
        self.wheels_started = board::wheels_start(
            if c.task == Task::Right { 0 } else { duty },
            if c.task == Task::Left { 0 } else { duty },
            c.task == Task::All,
        );
        self.wheels_started
    }
    fn keys(&mut self, now: u32) {
        let raw = board::keys();
        let mut event = None;
        for (i, key) in self.keys.iter_mut().enumerate() {
            if key.update(raw & (1 << i) != 0, now) {
                event = Key::from_id(i as u8);
            }
        }
        #[cfg(debug_assertions)]
        {
            let request = debug_key_request.swap(0, Ordering::Relaxed);
            if event.is_none() && raw == 0 && (0xc5000001..0xc5000005).contains(&request) {
                event = Key::from_id((request - 0xc5000001) as u8);
            }
        }
        let Some(event) = event else {
            return;
        };
        let was_running = self.controls.running;
        self.controls.key(event, now);
        if was_running {
            trace(1);
            self.stop(now);
            return;
        }
        if !self.controls.running {
            return;
        }
        trace(0);
        if self.controls.task.stepper() {
            if self.controls.task == Task::All && board::faults() != 0 {
                trace(3 | (u32::from(board::faults()) << 8));
                self.stop(now);
                return;
            }
            if !self.step.start(
                &mut StepSerial,
                self.controls.reverse,
                self.controls.rpm(),
                now,
            ) {
                self.stop(now);
            }
        } else if self.controls.task.wheels() && !self.start_wheels() {
            self.stop(now);
        }
    }
    fn motors(&mut self, now: u32) {
        if !self.controls.running {
            return;
        }
        if self.controls.task.stepper() && !self.step.running() {
            trace(6);
            self.stop(now);
            return;
        }
        if self.controls.task == Task::All
            && !self.wheels_started
            && self.step.phase == Phase::Moving
            && !self.start_wheels()
        {
            trace(5 | ((board::wheel_state().1 as u32) << 8));
            self.stop(now);
            return;
        }
        if self.wheels_started && !board::wheel_state().0 {
            trace(5 | ((board::wheel_state().1 as u32) << 8));
            self.stop(now);
        }
    }
    fn remote(&mut self, b: u8, now: u32) {
        if b == b'!' {
            if self.controls.running {
                trace(2);
            }
            self.stop(now);
            self.remote_used = 0;
            self.remote_overflow = false;
            return;
        }
        if b == b'\r' {
            return;
        }
        if b != b'\n' {
            if self.remote_used < self.remote.len() {
                self.remote[self.remote_used] = b;
                self.remote_used += 1;
            } else {
                self.remote_overflow = true;
            }
            return;
        }
        let ota_command = !self.remote_overflow && &self.remote[..self.remote_used] == b"C5:OTA";
        let info_command = !self.remote_overflow && &self.remote[..self.remote_used] == b"C5:INFO";
        self.remote_used = 0;
        self.remote_overflow = false;
        if ota_command && !self.upgrade {
            self.stop(now);
            self.step.stop(&mut StepSerial, now);
            self.upgrade = true;
            self.upgrade_at = now;
            self.ui.updating = true;
            self.ui.update_status = "PREPARING";
            serial::send(Id::Bt, b"C5OTA,REBOOT\r\n");
        } else if info_command {
            let mut bytes = [0; 32];
            if flash::read_bytes(ota::APP_META, &mut bytes).is_err() {
                return;
            }
            if let Ok(header) = ota::Header::decode(&bytes) {
                let mut line = Text::<64>::new();
                if write!(line, "C5APP,1,{:08X},{}\r\n", header.crc, header.length).is_ok() {
                    serial::send(Id::Bt, line.as_bytes());
                }
            }
        }
    }
    fn update(&mut self, now: u32) {
        let interval = now.wrapping_sub(self.sampled_at);
        if interval >= 100 {
            for i in 0..2 {
                let count = board::encoder(i);
                let delta = encoder_delta(count, self.previous[i]);
                self.previous[i] = count;
                self.ui.encoder[i] = encoder_delta(count, 0);
                self.ui.counts_per_second[i] =
                    (i64::from(delta) * 1000 / i64::from(interval)) as i32;
            }
            self.sampled_at = now;
        }
        let c = &self.controls;
        let (active, trip, faults) = board::wheel_state();
        self.ui.task = c.task;
        self.ui.running = c.running;
        self.ui.reverse = c.reverse;
        self.ui.duty = c.duty().unsigned_abs();
        self.ui.remaining_ms = if c.running && !c.task.continuous() {
            TEST_MS.saturating_sub(now.wrapping_sub(c.started_at))
        } else {
            0
        };
        self.ui.gray = board::gray();
        self.ui.phases = board::phases();
        self.ui.faults = board::faults();
        self.ui.wheel_faults = faults;
        self.ui.wheel_requested = active;
        self.ui.wheel_enable_fault = trip == Trip::EnableLow;
        self.ui.step_online = self.step.online(now);
        self.ui.step_status = self.step.name(now);
        self.ui.step_error = self.step.error;
        self.ui.step_target_rpm = c.rpm();
        self.ui.step_rpm = self.step.rpm;
        self.ui.step_rpm_valid = self.step.rpm_valid && self.ui.step_online;
    }
    fn telemetry(&mut self, now: u32, lcd: bool) {
        if !elapsed(now, self.telemetry_at, 500) {
            return;
        }
        self.telemetry_at = now;
        let a = serial::stats(Id::Bt);
        let b = serial::stats(Id::Step);
        let u = &self.ui;
        let s = &self.step;
        let c = &self.controls;
        let r = Record {
            version: 2,
            values: [
                i64::from(now),
                i64::from(self.sequence),
                c.task as i64,
                c.running as i64,
                if c.running && c.task.wheels() {
                    i64::from(c.duty())
                } else {
                    0
                },
                i64::from(u.encoder[0]),
                i64::from(u.encoder[1]),
                i64::from(u.counts_per_second[0]),
                i64::from(u.counts_per_second[1]),
                i64::from(u.gray),
                i64::from(u.phases),
                i64::from(u.faults),
                u.wheel_requested as i64,
                u.step_online as i64,
                s.phase as i64,
                i64::from(s.flags),
                i64::from(s.position),
                i64::from(s.error),
                i64::from(a.errors.wrapping_add(b.errors)),
                i64::from(a.rx_drops.wrapping_add(b.rx_drops)),
                i64::from(a.tx_drops.wrapping_add(b.tx_drops)),
                lcd as i64,
                board::wheel_state().1 as i64,
                i64::from(s.timeouts),
                s.position_valid as i64,
                if s.running() {
                    i64::from(s.target_rpm) * if s.reverse { -1 } else { 1 }
                } else {
                    0
                },
                i64::from(s.rpm),
                u.step_rpm_valid as i64,
            ],
        };
        self.sequence = self.sequence.wrapping_add(1);
        self.ui.bt_tx = serial::send(Id::Bt, r.csv().as_bytes());
    }
}
pub fn run() -> ! {
    if !board::init() || !serial::init() {
        board::panic_stop();
    }
    let mut app = App::default();
    let mut display = Display::new();
    display.init();
    let raw = board::keys();
    for (i, key) in app.keys.iter_mut().enumerate() {
        *key = Debounce::held(raw & (1 << i) != 0, board::millis());
    }
    loop {
        let now = board::millis();
        board::heartbeat();
        app.step.poll(&mut StepSerial, now);
        if !app.upgrade {
            app.keys(now);
        }
        while let Some(b) = serial::receive(Id::Bt) {
            app.remote(b, now);
        }
        board::wheels_poll();
        let was_running = app.controls.running;
        let fault_stop = app.controls.task == Task::Stepper && board::faults() != 0;
        app.controls.tick(now, fault_stop);
        if was_running && !app.controls.running {
            trace(if fault_stop {
                3 | (u32::from(board::faults()) << 8)
            } else {
                4
            });
        }
        app.motors(now);
        if !app.controls.running {
            app.stop(now);
        }
        app.update(now);
        if !app.upgrade {
            app.telemetry(now, display.ready);
        }
        if app.upgrade
            && elapsed(now, app.upgrade_at, 1200)
            && serial::idle(Id::Bt)
            && serial::idle(Id::Step)
        {
            board::write(ota::REQUEST_ADDR, ota::REQUEST_MAGIC);
            board::write(ota::REQUEST_ADDR + 4, !ota::REQUEST_MAGIC);
            runtime::reset();
        }
        display.poll(&app.ui, now);
        board::led((now / if app.controls.running { 125 } else { 500 }) & 1 != 0);
    }
}
