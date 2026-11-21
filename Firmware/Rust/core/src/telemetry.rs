// 兼容既有版本的遥测解析及 CSV 表示，从串口分片恢复完整记录。

use crate::{controls::Task, text::Text};
use core::fmt::Write;
pub const FIELDS: &str = "tag,version,ms,seq,task,running,duty_permille,left_count,right_count,left_cps,right_cps,gray_raw,encoder_ab,fault_mask,enable_request,step_online,step_phase,step_flags,step_position_raw,step_error,uart_errors,rx_drops,tx_drops,lcd_ok,wheel_trip,step_timeouts,step_position_valid,step_target_rpm,step_rpm,step_rpm_valid";
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Record {
    pub version: u8,
    pub values: [i64; 28],
}
impl Record {
    // 将遥测行拆为字段并校验版本与长度。
    pub fn parse(line: &str) -> Option<Self> {
        let mut fields = line.trim().split(',');
        if fields.next() != Some("C5") {
            return None;
        }
        let version = fields.next()?.parse::<u8>().ok()?;
        let count = match version {
            1 => 25,
            2 => 28,
            _ => return None,
        };
        let mut values = [0; 28];
        for slot in &mut values[..count] {
            *slot = fields.next()?.parse().ok()?;
        }
        if fields.next().is_some() || !(0..=5).contains(&values[2]) {
            return None;
        }
        Some(Self { version, values })
    }
    // 把解析后的遥测记录输出成统一 CSV 列。
    pub fn csv(&self) -> Text<640> {
        let mut line = Text::new();
        let _ = write!(line, "C5,{}", self.version);
        let count = if self.version == 1 { 25 } else { 28 };
        for v in &self.values[..count] {
            let _ = write!(line, ",{v}");
        }
        let _ = write!(line, "\r\n");
        line
    }
    // 将本行任务编号解释为任务枚举。
    pub fn task(&self) -> Task {
        Task::from_id(self.values[2] as u8).unwrap_or_default()
    }
}
pub struct Decoder {
    bytes: [u8; 1024],
    used: usize,
    discarding: bool,
    pub invalid: u32,
}
impl Default for Decoder {
    // 初始化空的逐行解析状态。
    fn default() -> Self {
        Self {
            bytes: [0; 1024],
            used: 0,
            discarding: false,
            invalid: 0,
        }
    }
}
impl Decoder {
    // 逐次接收一个字节，遇到换行才交出解析结果；超长行丢弃至下一个换行。
    pub fn feed(&mut self, b: u8) -> Option<Record> {
        if b == b'\n' {
            let record = if self.discarding {
                None
            } else {
                core::str::from_utf8(&self.bytes[..self.used])
                    .ok()
                    .and_then(Record::parse)
            };
            if record.is_none() && !self.discarding {
                self.invalid = self.invalid.wrapping_add(1);
            }
            self.used = 0;
            self.discarding = false;
            return record;
        }
        if self.discarding {
            return None;
        }
        if self.used == self.bytes.len() {
            self.used = 0;
            self.discarding = true;
            self.invalid = self.invalid.wrapping_add(1);
            return None;
        }
        self.bytes[self.used] = b;
        self.used += 1;
        None
    }
}
