use coral_core::ota::{self, DATA, Frame, Parser};
use coral_host::{
    Result,
    exchange::{self, Transport},
};
use std::{
    collections::VecDeque,
    future::Future,
    io,
    pin::pin,
    task::{Context, Poll, Waker},
    time::{Duration, Instant},
};

// Scripted I/O is always ready; tests never create a Bluetooth adapter or wait on time.
fn run<T>(f: impl Future<Output = T>) -> T {
    let mut cx = Context::from_waker(Waker::noop());
    let mut f = pin!(f);
    match f.as_mut().poll(&mut cx) {
        Poll::Ready(v) => v,
        Poll::Pending => panic!("mock unexpectedly blocked"),
    }
}
#[derive(Default)]
struct Wire {
    sent: Vec<Vec<u8>>,
    replies: VecDeque<Result<Frame>>,
}
impl Transport for Wire {
    async fn send(&mut self, b: &[u8]) -> Result<()> {
        self.sent.push(b.to_vec());
        Ok(())
    }
    async fn response(&mut self, _: Instant) -> Result<Frame> {
        self.replies.pop_front().unwrap_or_else(|| {
            Err(io::Error::new(io::ErrorKind::TimedOut, "injected lost ACK").into())
        })
    }
}
fn ack(command: u8, sequence: u32, status: u32) -> Result<Frame> {
    let mut b = [0; 8];
    ota::put32(&mut b, status);
    ota::put32(&mut b[4..], 128);
    Ok(Frame::new(command, sequence, &b).unwrap())
}
#[test]
fn lost_ack_retries_same_packet_and_accepts_matching_late_response() {
    let mut w = Wire::default();
    w.replies
        .push_back(Err(io::Error::new(io::ErrorKind::TimedOut, "lost").into()));
    w.replies.push_back(ack(DATA | 0x80, 76, 0));
    w.replies.push_back(ack(DATA | 0x80, 77, 0));
    let reply = run(exchange::request(
        &mut w,
        DATA,
        77,
        &[5; 132],
        4,
        Duration::from_secs(3),
    ))
    .unwrap();
    assert_eq!(ota::get32(&reply[4..]), 128);
    assert_eq!(w.sent.len(), 2);
    assert_eq!(w.sent[0], w.sent[1]);
    let mut parser = Parser::new();
    let packet = w.sent[0].iter().find_map(|&b| parser.feed(b)).unwrap();
    assert_eq!(packet.sequence, 77);
}
#[test]
fn bounded_retries_return_error_after_all_acks_lost() {
    let mut w = Wire::default();
    let result = run(exchange::request(
        &mut w,
        DATA,
        1,
        &[],
        4,
        Duration::from_secs(1),
    ));
    assert!(
        result
            .unwrap_err()
            .downcast_ref::<io::Error>()
            .is_some_and(|e| e.kind() == io::ErrorKind::TimedOut)
    );
    assert_eq!(w.sent.len(), 4);
}
#[test]
fn explicit_rejection_truncated_ack_and_disconnect_do_not_blindly_retry() {
    for reply in [
        ack(DATA | 0x80, 1, 3),
        Ok(Frame::new(DATA | 0x80, 1, &[0; 7]).unwrap()),
        Err(io::Error::new(io::ErrorKind::ConnectionReset, "disconnected").into()),
    ] {
        let mut w = Wire::default();
        w.replies.push_back(reply);
        assert!(
            run(exchange::request(
                &mut w,
                DATA,
                1,
                &[],
                4,
                Duration::from_secs(1)
            ))
            .is_err()
        );
        assert_eq!(w.sent.len(), 1);
    }
}
