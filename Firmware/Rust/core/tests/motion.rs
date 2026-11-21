// 以模拟时钟、驱动应答和编码器核对 Rust 设备控制的启停与故障保护。

use coral_core::{
    controls::*,
    emm::{self, Phase, Serial, Stepper},
};
use std::collections::VecDeque;

#[test]
// 检查上电默认状态和按键启动、停止的约定。
fn power_on_and_toggle_contract() {
    let mut c = Controls::default();
    assert_eq!(
        (c.task, c.duty(), c.rpm(), c.running),
        (Task::All, 450, 30, false)
    );
    c.key(Key::Run, 100);
    for time in [3100, 60_000, 86_400_000] {
        c.tick(time, false);
        assert!(c.running);
    }
    c.key(Key::Run, 86_400_001);
    assert!(!c.running);
    for key in [Key::Task, Key::Level, Key::Direction, Key::Run] {
        c.key(Key::Run, 10);
        c.key(key, 20);
        assert!(!c.running);
        assert_eq!((c.task, c.level, c.reverse), (Task::All, 3, false));
    }
}
#[test]
// 核对任务选择、档位方向及限时运行。
fn tasks_levels_direction_and_timed_tests() {
    let mut c = Controls::default();
    c.key(Key::Task, 0);
    assert_eq!(c.task, Task::Monitor);
    c.key(Key::Run, 0);
    assert!(!c.running);
    for task in [
        Task::Left,
        Task::Right,
        Task::Both,
        Task::Stepper,
        Task::All,
    ] {
        c.key(Key::Task, 0);
        assert_eq!(c.task, task);
        c.key(Key::Run, u32::MAX - 1500);
        c.tick(1498, false);
        assert!(c.running);
        c.tick(1499, false);
        assert_eq!(c.running, task.continuous());
        c.running = false;
    }
    c.key(Key::Level, 0);
    assert_eq!(c.duty(), 100);
    c.key(Key::Direction, 0);
    assert_eq!(c.duty(), -100);
    c.key(Key::Run, 0);
    c.tick(5, true);
    assert!(!c.running);
}
#[test]
// 模拟上电按住和触点抖动，确认不会误启动。
fn held_and_bouncing_key_does_not_start() {
    let mut d = Debounce::held(true, 0);
    assert!(!d.update(true, 1000));
    assert!(!d.update(false, 1001));
    assert!(!d.update(false, 1026));
    assert!(!d.update(true, 1030));
    assert!(!d.update(false, 1040));
    assert!(!d.update(true, 1045));
    assert!(!d.update(true, 1069));
    assert!(d.update(true, 1070));
    assert!(!d.update(true, 2000));
    let mut wrap = Debounce::default();
    assert!(!wrap.update(true, u32::MAX - 10));
    assert!(wrap.update(true, 14));
}
#[test]
// 覆盖编码器越界回绕时的带符号增量。
fn encoder_wrap_is_signed_modular_difference() {
    assert_eq!(encoder_delta(3, u32::MAX - 2), 6);
    assert_eq!(encoder_delta(u32::MAX - 2, 3), -6);
    assert_eq!(encoder_delta(0x80000000, 0x7fffffff), 1);
}
#[test]
// 检查唤醒等待、故障锁存和运行期限。
fn wheel_wake_fault_latching_and_deadlines() {
    let mut w = WheelGuard::new();
    assert!(w.start(0, 0, true));
    w.check(4, 3, false);
    assert!(w.active);
    w.check(5, 0, false);
    assert_eq!(w.trip, Trip::EnableLow);
    w.check(6, 0, true);
    assert!(!w.active);
    assert!(!w.start(7, 1, true));
    assert_eq!(w.trip, Trip::Fault);
    assert!(w.start(10, 0, true));
    w.check(15, 2, true);
    assert_eq!(w.trip, Trip::Fault);
    w.start(0, 0, true);
    w.check(99, 0, true);
    assert!(w.active);
    w.check(100, 0, true);
    assert_eq!(w.trip, Trip::Stall);
    w.start(u32::MAX - 1500, 0, false);
    w.heartbeat = 1499;
    w.check(1499, 0, true);
    assert_eq!(w.trip, Trip::Deadline);
    w.start(0, 0, true);
    w.heartbeat = 1_000_000;
    w.check(1_000_000, 0, true);
    assert!(w.active);
}

#[derive(Default)]
struct Wire {
    tx: Vec<Vec<u8>>,
    rx: VecDeque<u8>,
    errors: u32,
    fail: bool,
}
impl Serial for Wire {
    // 捕获驱动向模拟串口写出的字节。
    fn write(&mut self, b: &[u8]) -> bool {
        if self.fail {
            return false;
        }
        self.tx.push(b.to_vec());
        true
    }
    // 从模拟接收队列弹出应答。
    fn read(&mut self) -> Option<u8> {
        self.rx.pop_front()
    }
    // 返回测试指定的串口错误计数。
    fn rx_errors(&self) -> u32 {
        self.errors
    }
}
impl Wire {
    // 注入指定命令的匹配 ACK。
    fn ack(&mut self, s: &mut Stepper, command: u8, status: u8, time: u32) {
        self.rx.extend([1, command, status, 0x6b]);
        s.poll(self, time);
    }
}
// 将模拟步进推进到可启动状态。
fn ready() -> (Stepper, Wire) {
    let mut s = Stepper::default();
    let mut w = Wire::default();
    s.poll(&mut w, 999);
    assert!(w.tx.is_empty());
    s.poll(&mut w, 1000);
    assert_eq!(w.tx.last().unwrap(), &emm::stop());
    w.ack(&mut s, 0xfe, 2, 1001);
    assert_eq!(w.tx.last().unwrap(), &emm::enable(false));
    w.ack(&mut s, 0xf3, 2, 1002);
    w.ack(&mut s, 0x3a, 0, 1003);
    assert_eq!(s.phase, Phase::Idle);
    (s, w)
}
#[test]
// 在有效 Emm 帧前后插入噪声，检查解析恢复。
fn emm_wire_and_noise_recovery() {
    assert_eq!(emm::velocity(true, 30), [1, 0xf6, 1, 0, 30, 10, 0, 0x6b]);
    let mut p = emm::Parser::default();
    let mut replies = vec![];
    for b in [
        0x99, 1, 0x99, 1, 0x35, 2, 0, 30, 0x6b, 1, 0x35, 1, 0, 30, 0x6b,
    ] {
        if let Some(r) = p.feed(b, 0) {
            replies.push(r);
        }
    }
    assert_eq!(replies.len(), 1);
    assert_eq!(replies[0].value, -30);
    assert!(p.rejected > 0);
    p.feed(1, 0);
    p.feed(0x36, 1);
    assert!(p.feed(0, 40).is_none());
    for b in [1, 0x36, 0, 0x7f, 0xff, 0xff, 0xff, 0x6b] {
        if let Some(r) = p.feed(b, 41) {
            assert_eq!(r.value, i32::MAX);
        }
    }
}
#[test]
// 检查持续速度模式启停均等待对应应答。
fn stepper_continuous_start_stop_requires_acks() {
    let (mut s, mut w) = ready();
    assert!(s.start(&mut w, false, 30, 1010));
    assert_eq!(s.phase, Phase::Enable);
    assert_eq!(w.tx.last().unwrap(), &emm::enable(true));
    w.ack(&mut s, 0xf3, 2, 1011);
    assert_eq!(w.tx.last().unwrap(), &emm::velocity(false, 30));
    assert_eq!(s.phase, Phase::Speed);
    w.ack(&mut s, 0xf6, 2, 1012);
    assert_eq!(s.phase, Phase::Moving);
    s.poll(&mut w, 1212);
    w.ack(&mut s, 0x3a, 3, 1213);
    assert_eq!(s.phase, Phase::Moving);
    s.stop(&mut w, 1214);
    w.ack(&mut s, 0xfe, 2, 1215);
    w.ack(&mut s, 0xf3, 2, 1216);
    assert_eq!(s.phase, Phase::Idle);
    assert!(!s.running());
}
#[test]
// 在取消后送入迟到使能 ACK，确认机构不会重启。
fn delayed_enable_ack_cannot_restart_after_stop() {
    let (mut s, mut w) = ready();
    s.start(&mut w, false, 30, 1010);
    s.stop(&mut w, 1011);
    w.ack(&mut s, 0xf3, 2, 1012);
    assert_eq!(s.phase, Phase::Stop);
    assert!(!w.tx.iter().any(|b| b[1] == 0xf6));
}
#[test]
// 注入超时、接收中断和停转反馈，检查保护停止。
fn stepper_timeout_rx_gap_and_stall_force_stop() {
    let (mut s, mut w) = ready();
    assert!(!s.start(&mut w, false, 91, 1010));
    s.start(&mut w, false, 30, 1010);
    s.poll(&mut w, 1210);
    assert_eq!(s.phase, Phase::Stop);
    assert_eq!(s.timeouts, 1);
    s.poll(&mut w, 1410);
    assert_eq!(s.phase, Phase::Disable);
    s.poll(&mut w, 1610);
    assert_eq!(s.phase, Phase::Offline);
    let (mut s, mut w) = ready();
    s.start(&mut w, false, 30, 1010);
    w.errors += 1;
    s.poll(&mut w, 1011);
    assert_eq!((s.phase, s.error), (Phase::Stop, 0xe3));
    let (mut s, mut w) = ready();
    s.start(&mut w, false, 30, 1010);
    w.ack(&mut s, 0xf3, 2, 1011);
    w.ack(&mut s, 0xf6, 2, 1012);
    s.poll(&mut w, 1212);
    w.ack(&mut s, 0x3a, 4, 1213);
    assert_eq!((s.phase, s.error), (Phase::Stop, 0xe2));
}
