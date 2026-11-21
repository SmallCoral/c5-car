// 用固定字节与 C 版像素 CRC 校验 Rust 遥测、OTA 和屏幕输出的兼容性。

use coral_core::{
    ota::*,
    telemetry::{Decoder, Record},
    text::Text,
    ui,
};
use std::fmt::Write;

// 构造字段齐全的测试遥测样本。
fn sample() -> Vec<u8> {
    let mut v = vec![0x55; 777];
    put32(&mut v, REQUEST_ADDR);
    put32(&mut v[4..], APP_ADDR + 0x241);
    v
}
#[test]
// 损坏向量和 CRC，验证包头与向量检查。
fn crc_and_header_reject_corruption_and_wrong_vectors() {
    assert_eq!(crc(b"123456789"), 0xcbf43926);
    let mut b = sample();
    let h = Header::for_image(&b).unwrap();
    let mut encoded = h.encode();
    assert_eq!(Header::decode(&encoded), Ok(h));
    for i in 0..32 {
        encoded[i] ^= 1;
        assert!(Header::decode(&encoded).is_err());
        encoded[i] ^= 1;
    }
    for (sp, entry) in [
        (0, APP_ADDR + 0x241),
        (REQUEST_ADDR + 8, APP_ADDR + 0x241),
        (REQUEST_ADDR - 1, APP_ADDR + 0x241),
        (REQUEST_ADDR, APP_ADDR + 0x240),
        (REQUEST_ADDR, STAGE_ADDR + 1),
    ] {
        put32(&mut b, sp);
        put32(&mut b[4..], entry);
        assert!(Header::for_image(&b).is_err());
    }
    assert!(Header::for_image(&[0; 511]).is_err());
}
#[test]
// 跨边界分割帧，并混入噪声、坏 CRC 及超长字段。
fn frames_survive_fragmentation_noise_crc_and_oversized_lengths() {
    let f = Frame::new(DATA, 0x89abcdef, &[0xaa; PAYLOAD_MAX]).unwrap();
    let mut b = [0; FRAME_MAX];
    let n = f.encode(&mut b);
    assert_eq!(n, FRAME_MAX);
    let mut p = Parser::new();
    let mut received = vec![];
    let mut invalid = b;
    invalid[6..8].copy_from_slice(&u16::MAX.to_le_bytes());
    let mut bad_crc = b;
    bad_crc[n - 1] ^= 1;
    for byte in [
        vec![0, 0xc5, 0xc5],
        invalid.to_vec(),
        bad_crc.to_vec(),
        b.to_vec(),
    ]
    .concat()
    {
        if let Some(frame) = p.feed(byte) {
            received.push(frame);
        }
    }
    assert_eq!(received.len(), 1);
    assert_eq!(received[0].sequence, 0x89abcdef);
    assert_eq!(received[0].payload(), &[0xaa; PAYLOAD_MAX]);
    assert!(Frame::new(DATA, 0, &[0; PAYLOAD_MAX + 1]).is_err());
}
#[test]
// 用多种版本、分片以及超长行检验逐行遥测解析。
fn telemetry_versions_fragmented_and_overlong_lines() {
    let mut r = Record {
        version: 2,
        values: [0; 28],
    };
    r.values[2] = 5;
    r.values[5] = -123;
    r.values[27] = 1;
    assert_eq!(Record::parse(r.csv().as_str()), Some(r.clone()));
    let mut d = Decoder::default();
    let mut records = vec![];
    for b in [
        vec![b'x'; 1100],
        b"\nnoise\n".to_vec(),
        r.csv().as_bytes().to_vec(),
    ]
    .concat()
    {
        if let Some(record) = d.feed(b) {
            records.push(record);
        }
    }
    assert_eq!(records, vec![r.clone()]);
    assert_eq!(d.invalid, 2);
    r.version = 1;
    let old = Record::parse(r.csv().as_str()).unwrap();
    assert_eq!(old.values[..25], r.values[..25]);
    assert_eq!(old.values[25..], [0; 3]);
    assert!(Record::parse("C5,2,1").is_none());
    r.values[2] = 6;
    assert!(Record::parse(r.csv().as_str()).is_none());
}
#[test]
// 在容量边界测试 UTF-8 写入仍保持字符完整。
fn bounded_text_preserves_utf8_on_capacity_error() {
    let mut s = Text::<6>::new();
    write!(s, "中文").unwrap();
    assert!(write!(s, "a").is_err());
    assert_eq!(s.as_str(), "中文");
    s.clear();
    assert_eq!(s.as_str(), "");
}

#[test]
// 对照旧 Python 生成的固定字节验证协议兼容。
fn wire_format_matches_legacy_python_golden_bytes() {
    let h = Header::for_image(&sample()).unwrap();
    assert_eq!(&h.encode(), include_bytes!("fixtures/v1_header.bin"));
    let f = Frame::new(BEGIN, 0x89abcdef, &h.encode()).unwrap();
    let mut b = [0; FRAME_MAX];
    let n = f.encode(&mut b);
    assert_eq!(&b[..n], include_bytes!("fixtures/v1_begin.bin"));
}

#[test]
// 覆盖字段数值极限，确认定长行缓冲容量。
fn all_numeric_telemetry_values_fit_without_truncation() {
    let mut r = Record {
        version: 2,
        values: [i64::MIN; 28],
    };
    r.values[2] = 5;
    assert_eq!(Record::parse(r.csv().as_str()), Some(r));
}

#[test]
// 将 Rust 画面与已归档的 27 个 C 版画面 CRC 对照。
fn ui_matches_27_legacy_c_renderings() {
    let expected: Vec<_> = include_str!("fixtures/ui_crc32.txt")
        .lines()
        .map(|s| u32::from_str_radix(s, 16).unwrap())
        .collect();
    let mut actual = vec![];
    let mut pixels = [0; ui::BYTES];
    for task in 0..6 {
        for mode in 0..4 {
            let s = ui::State {
                task: coral_core::controls::Task::from_id(task).unwrap(),
                running: mode & 1 != 0,
                reverse: mode & 2 != 0,
                duty: 450,
                remaining_ms: 2400,
                encoder: [1284, -1279],
                counts_per_second: [456, -452],
                gray: 0xe7,
                phases: 9,
                faults: if mode == 2 { 1 } else { 0 },
                wheel_requested: mode & 1 != 0,
                step_online: true,
                step_status: "MOVING",
                bt_tx: true,
                step_target_rpm: 30,
                step_rpm: if mode & 2 != 0 { -30 } else { 30 },
                step_rpm_valid: true,
                step_error: if mode == 3 { 0xe2 } else { 0 },
                update_status: "RECEIVING",
                ..ui::State::default()
            };
            ui::render(&mut pixels, &s);
            actual.push(crc(&pixels));
        }
    }
    for i in 0..3 {
        let s = ui::State {
            updating: true,
            update_status: "RECEIVING",
            update_bytes: i * 450,
            update_total: 1000,
            ..ui::State::default()
        };
        ui::render(&mut pixels, &s);
        actual.push(crc(&pixels));
    }
    assert_eq!(actual, expected);
}
#[test]
// 用极端计数和升级进度覆盖各任务的绘制边界。
fn ui_handles_extreme_counters_progress_and_all_tasks() {
    let mut pixels = [0; ui::BYTES];
    let mut s = ui::State {
        encoder: [i32::MIN, i32::MAX],
        counts_per_second: [i32::MIN, i32::MAX],
        ..ui::State::default()
    };
    for task in 0..6 {
        s.task = coral_core::controls::Task::from_id(task).unwrap();
        for running in [false, true] {
            s.running = running;
            ui::render(&mut pixels, &s);
        }
    }
    s.updating = true;
    s.update_bytes = u32::MAX;
    s.update_total = 1;
    ui::render(&mut pixels, &s);
    s.update_total = 0;
    ui::render(&mut pixels, &s);
    assert!(pixels.iter().any(|&b| b != 0));
}
