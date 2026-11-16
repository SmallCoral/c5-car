//! Stop-and-wait OTA exchange, independent of the operating system BLE backend.
use crate::Result;
use coral_core::ota::{self, FRAME_MAX, Frame};
use std::{
    future::Future,
    io,
    time::{Duration, Instant},
};

pub trait Transport {
    fn send(&mut self, bytes: &[u8]) -> impl Future<Output = Result<()>>;
    fn response(&mut self, deadline: Instant) -> impl Future<Output = Result<Frame>>;
}
fn timeout() -> Box<dyn std::error::Error + Send + Sync> {
    io::Error::new(io::ErrorKind::TimedOut, "no matching OTA acknowledgement").into()
}
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
        // Keep the same sequence and bytes: the device can acknowledge a duplicate safely.
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
