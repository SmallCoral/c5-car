//! OTA 每次发一包、等一次应答，收发逻辑独立于操作系统的蓝牙后端。
use crate::Result;
use coral_core::ota::{self, FRAME_MAX, Frame};
use std::{
    future::Future,
    io,
    time::{Duration, Instant},
};

pub trait Transport {
    // 异步发送同一条已编码请求，便于重试时保持字节不变。
    fn send(&mut self, bytes: &[u8]) -> impl Future<Output = Result<()>>;
    // 在给定期限前取回应答帧，超时或断连时交回错误。
    fn response(&mut self, deadline: Instant) -> impl Future<Output = Result<Frame>>;
}
// 选择当前报文应答的等待期限。
fn timeout() -> Box<dyn std::error::Error + Send + Sync> {
    io::Error::new(io::ErrorKind::TimedOut, "no matching OTA acknowledgement").into()
}
// 对同一帧重试，按命令与序号匹配确认或拒绝。
pub async fn request(
    io: &mut impl Transport,
    command: u8,
    sequence: u32,
    payload: &[u8],
    attempts: u32,
    wait: Duration,
) -> Result<Vec<u8>> {
    let frame = Frame::new(command, sequence, payload).map_err(|_| "oversized OTA command")?;
    let mut bytes = [0; FRAME_MAX];
    let length = frame.encode(&mut bytes);
    for attempt in 0..attempts {
        if attempt != 0 {
            eprintln!("Retry command {command}, sequence {sequence}");
        }
        // 重试沿用同一序号和内容，设备收到重复包可直接再次确认。
        io.send(&bytes[..length]).await?;
        let deadline = Instant::now()
            .checked_add(wait)
            .ok_or("invalid acknowledgement timeout")?;
        while Instant::now() < deadline {
            let response = match io.response(deadline).await {
                Ok(frame) => frame,
                Err(e)
                    if e.downcast_ref::<io::Error>()
                        .is_some_and(|e| e.kind() == io::ErrorKind::TimedOut) =>
                {
                    break;
                }
                Err(e) => return Err(e),
            };
            if response.command != command | 0x80 || response.sequence != sequence {
                continue;
            }
            let p = response.payload();
            if p.len() < 8 {
                return Err("truncated OTA reply".into());
            }
            let status = ota::get32(p);
            if status != 0 {
                return Err(
                    format!("bootloader rejected command {command}: status {status}").into(),
                );
            }
            return Ok(p.to_vec());
        }
    }
    Err(timeout())
}
