use coral_core::{ota::*, telemetry::Record};
use coral_host::*;
use std::{
    collections::BTreeMap,
    fs,
    path::PathBuf,
    process::Command,
    sync::atomic::{AtomicU32, Ordering},
};
static NEXT: AtomicU32 = AtomicU32::new(0);
struct Temp(PathBuf);
impl Temp {
    fn new() -> Self {
        let p = std::env::temp_dir().join(format!(
            "coral-rust-test-{}-{}",
            std::process::id(),
            NEXT.fetch_add(1, Ordering::Relaxed)
        ));
        fs::create_dir(&p).unwrap();
        Self(p)
    }
}
impl Drop for Temp {
    fn drop(&mut self) {
        // The only recursive removal is the absolute temporary directory created by this test.
        let root = std::env::temp_dir().canonicalize().unwrap();
        let path = self.0.canonicalize().unwrap();
        assert_eq!(path.parent(), Some(root.as_path()));
        assert!(
            path.file_name()
                .unwrap()
                .to_str()
                .unwrap()
                .starts_with("coral-rust-test-")
        );
        fs::remove_dir_all(path).unwrap();
    }
}
fn binary(base: u32) -> Vec<u8> {
    let mut b = vec![0xa5; 777];
    put32(&mut b, REQUEST_ADDR);
    put32(&mut b[4..], base + 0x241);
    b
}
fn parse_hex(hex: &str) -> BTreeMap<u32, u8> {
    let mut upper = 0;
    let mut map = BTreeMap::new();
    let mut eof = false;
    for line in hex.lines() {
        assert!(line.starts_with(':'));
        assert!(!eof);
        let bytes: Vec<u8> = line.as_bytes()[1..]
            .as_chunks::<2>()
            .0
            .iter()
            .map(|b| u8::from_str_radix(std::str::from_utf8(b).unwrap(), 16).unwrap())
            .collect();
        assert_eq!(bytes.len(), usize::from(bytes[0]) + 5);
        assert_eq!(bytes.iter().fold(0u8, |sum, &b| sum.wrapping_add(b)), 0);
        let address = u32::from(u16::from_be_bytes([bytes[1], bytes[2]]));
        let data = &bytes[4..bytes.len() - 1];
        match bytes[3] {
            0 => {
                assert!(address + data.len() as u32 <= 65536);
                for (i, &b) in data.iter().enumerate() {
                    assert!(map.insert(upper + address + i as u32, b).is_none());
                }
            }
            4 => {
                assert_eq!(data.len(), 2);
                upper = u32::from(u16::from_be_bytes([data[0], data[1]])) << 16;
            }
            1 => {
                assert!(data.is_empty());
                eof = true;
            }
            _ => panic!("unexpected record"),
        }
    }
    assert!(eof);
    map
}
#[test]
fn hex_checksums_cross_bank_and_overlap_rejection() {
    let b = [0xab; 33];
    let map = parse_hex(&make_hex(&[(0x0800fff8, &b)]).unwrap());
    for i in 0..33 {
        assert_eq!(map[&(0x0800fff8 + i)], 0xab);
    }
    assert!(make_hex(&[(100, &b), (110, &b)]).is_err());
    assert!(make_hex(&[(u32::MAX, &b)]).is_err());
}
#[test]
fn packager_roundtrip_and_partition_metadata() {
    let t = Temp::new();
    let app = binary(APP_ADDR);
    let boot = binary(0x08000000);
    fs::write(t.0.join("app.bin"), &app).unwrap();
    fs::write(t.0.join("boot.bin"), &boot).unwrap();
    let h = package(
        &t.0.join("app.bin"),
        &t.0.join("boot.bin"),
        &t.0.join("out"),
    )
    .unwrap();
    assert_eq!(
        read_image(&t.0.join("out/coral_c5.ota")).unwrap(),
        (app.clone(), h)
    );
    let map = parse_hex(&fs::read_to_string(t.0.join("out/first_install.hex")).unwrap());
    for (start, bytes) in [
        (0x08000000, boot),
        (APP_ADDR, app),
        (APP_META, h.encode().to_vec()),
        (STAGE_META, vec![0xff; 32]),
    ] {
        for (i, b) in bytes.iter().enumerate() {
            assert_eq!(map[&(start + i as u32)], *b);
        }
    }
    let mut bad = fs::read(t.0.join("out/coral_c5.ota")).unwrap();
    bad[100] ^= 1;
    fs::write(t.0.join("bad.ota"), bad).unwrap();
    assert!(read_image(&t.0.join("bad.ota")).is_err());
    fs::write(t.0.join("boot.bin"), binary(STAGE_ADDR)).unwrap();
    assert!(
        package(
            &t.0.join("app.bin"),
            &t.0.join("boot.bin"),
            &t.0.join("out")
        )
        .is_err()
    );
}
#[test]
fn wrong_board_and_bad_resume_offsets_fail_closed() {
    let mut p = [0; 32];
    for (i, n) in [
        0,
        128,
        BOARD_ID,
        APP_ADDR,
        IMAGE_MAX,
        CHUNK as u32,
        777,
        0x1234,
    ]
    .into_iter()
    .enumerate()
    {
        put32(&mut p[i * 4..], n);
    }
    assert_eq!(Hello::decode(&p).unwrap().received, 128);
    for i in [8, 12, 16, 20] {
        p[i] ^= 1;
        assert!(Hello::decode(&p).is_err());
        p[i] ^= 1;
    }
    assert!(Hello::decode(&p[..31]).is_err());
    assert_eq!(resume_offset(&p[..8], 777).unwrap(), 128);
    for n in [1, 129, 778, 1024] {
        put32(&mut p[4..], n);
        assert!(resume_offset(&p[..8], 777).is_err());
    }
    put32(&mut p[4..], 777);
    assert_eq!(resume_offset(&p[..8], 777).unwrap(), 777);
    assert!(verify_identity("C5APP,1,12345678,777", 777, 0x12345678).is_ok());
    assert!(verify_identity("C5APP,1,12345679,777", 777, 0x12345678).is_err());
    assert!(verify_identity("C5APP,1,12345678,778", 777, 0x12345678).is_err());
}
#[test]
fn offline_cli_replay_preserves_rows_and_reports_invalid_input() {
    let t = Temp::new();
    let mut r = Record {
        version: 2,
        values: [0; 28],
    };
    r.values[2] = 5;
    r.values[5] = -9876;
    fs::write(t.0.join("input.txt"), format!("junk\n{}", r.csv().as_str())).unwrap();
    let result = Command::new(env!("CARGO_BIN_EXE_coral-host"))
        .args(["replay"])
        .arg(t.0.join("input.txt"))
        .arg("--out")
        .arg(t.0.join("out.csv"))
        .output()
        .unwrap();
    assert!(
        result.status.success(),
        "{}",
        String::from_utf8_lossy(&result.stderr)
    );
    let csv = fs::read_to_string(t.0.join("out.csv")).unwrap();
    let lines: Vec<_> = csv.lines().collect();
    assert_eq!(lines.len(), 2);
    assert_eq!(lines[1].split(',').count(), 31);
    assert!(lines[1].contains(",-9876,"));
    fs::write(t.0.join("input.txt"), "junk\n").unwrap();
    assert!(
        !Command::new(env!("CARGO_BIN_EXE_coral-host"))
            .arg("replay")
            .arg(t.0.join("input.txt"))
            .output()
            .unwrap()
            .status
            .success()
    );
}
#[test]
fn offline_cli_never_needs_radio_to_inspect_or_render() {
    let t = Temp::new();
    fs::write(t.0.join("app.bin"), binary(APP_ADDR)).unwrap();
    assert!(
        Command::new(env!("CARGO_BIN_EXE_coral-host"))
            .arg("inspect")
            .arg(t.0.join("app.bin"))
            .output()
            .unwrap()
            .status
            .success()
    );
    assert!(
        Command::new(env!("CARGO_BIN_EXE_coral-host"))
            .args(["render", "--out"])
            .arg(t.0.join("ui.ppm"))
            .output()
            .unwrap()
            .status
            .success()
    );
    let ppm = fs::read(t.0.join("ui.ppm")).unwrap();
    assert!(ppm.starts_with(b"P6\n128 160\n255\n"));
    assert_eq!(ppm.len(), 15 + 128 * 160 * 3);
}
