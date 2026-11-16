use coral_core::{
    ota,
    telemetry::{Decoder, FIELDS},
    ui,
};
use coral_host::Result;
use std::{
    collections::BTreeMap,
    fs,
    io::{self, Read, Write},
    path::Path,
    time::{SystemTime, UNIX_EPOCH},
};
#[cfg(feature = "radio")]
mod radio;

pub struct Options {
    pub command: String,
    pub positional: Vec<String>,
    pub values: BTreeMap<String, String>,
    pub stop: bool,
}
impl Options {
    fn parse() -> Result<Self> {
        let mut args = std::env::args().skip(1);
        let command = args.next().unwrap_or_else(|| "help".into());
        let mut options = Self {
            command,
            positional: Vec::new(),
            values: BTreeMap::new(),
            stop: false,
        };
        while let Some(arg) = args.next() {
            if arg == "--stop" {
                options.stop = true;
            } else if arg.starts_with("--") {
                if ![
                    "--app",
                    "--boot",
                    "--out",
                    "--ble",
                    "--uuid",
                    "--port",
                    "--baud",
                    "--seconds",
                ]
                .contains(&arg.as_str())
                {
                    return Err(format!("unknown option: {arg}").into());
                }
                let value = args.next().ok_or("option needs a value")?;
                if value.starts_with("--") || options.values.insert(arg, value).is_some() {
                    return Err("missing or duplicate option".into());
                }
            } else {
                options.positional.push(arg);
            }
        }
        Ok(options)
    }
    pub fn get(&self, key: &str) -> Option<&str> {
        self.values.get(key).map(String::as_str)
    }
    fn required(&self, key: &str) -> Result<&str> {
        self.get(key).ok_or_else(|| format!("missing {key}").into())
    }
    fn image(&self) -> Result<&Path> {
        if self.positional.len() != 1 {
            return Err("provide exactly one .ota/.bin image path".into());
        }
        Ok(Path::new(&self.positional[0]))
    }
}
pub struct Sink {
    decoder: Decoder,
    output: Box<dyn Write>,
    pub count: u64,
    last: Option<(u32, u32)>,
}
impl Sink {
    fn new(path: Option<&str>) -> Result<Self> {
        let mut output: Box<dyn Write> = match path {
            Some(p) => Box::new(fs::File::create(p)?),
            None => Box::new(io::stdout()),
        };
        writeln!(output, "received_unix_ms,{FIELDS}")?;
        Ok(Self {
            decoder: Decoder::default(),
            output,
            count: 0,
            last: None,
        })
    }
    pub fn accept(&mut self, bytes: &[u8]) -> Result<()> {
        for &b in bytes {
            if let Some(mut record) = self.decoder.feed(b) {
                let ms = record.values[0] as u32;
                let seq = record.values[1] as u32;
                if let Some((old_ms, old_seq)) = self.last {
                    let gap = seq.wrapping_sub(old_seq).wrapping_sub(1);
                    if ms < old_ms && seq < old_seq {
                        eprintln!("MCU restarted or counters wrapped");
                    } else if gap != 0 {
                        eprintln!("Telemetry gap: {gap} frame(s)");
                    }
                }
                self.last = Some((ms, seq));
                self.count += 1;
                let stamp = SystemTime::now().duration_since(UNIX_EPOCH)?.as_millis();
                // Normalize v1's missing velocity fields to the same 30-column CSV schema.
                let version = record.version;
                record.version = 2;
                let csv = record.csv();
                write!(self.output, "{stamp},C5,{version},{}", &csv.as_str()[5..])?;
                self.output.flush()?;
            }
        }
        Ok(())
    }
}
fn run(options: Options) -> Result<()> {
    match options.command.as_str() {
        "package" => {
            let h = coral_host::package(
                Path::new(options.required("--app")?),
                Path::new(options.required("--boot")?),
                Path::new(options.required("--out")?),
            )?;
            println!("OTA: {} bytes CRC32={:08X}", h.length, h.crc);
        }
        "inspect" => {
            let (_, h) = coral_host::read_image(options.image()?)?;
            println!(
                "board={:08X} address={:08X} bytes={} CRC32={:08X}",
                ota::BOARD_ID,
                ota::APP_ADDR,
                h.length,
                h.crc
            );
        }
        "replay" => {
            let mut input = fs::File::open(options.image()?)?;
            let mut sink = Sink::new(options.get("--out"))?;
            let mut bytes = [0; 256];
            loop {
                let n = input.read(&mut bytes)?;
                if n == 0 {
                    break;
                }
                sink.accept(&bytes[..n])?;
            }
            eprintln!(
                "{} records, {} invalid lines",
                sink.count, sink.decoder.invalid
            );
            if sink.count == 0 {
                return Err("no valid telemetry".into());
            }
        }
        "render" => {
            let mut pixels = vec![0; ui::BYTES];
            let state = ui::State {
                step_online: true,
                step_status: "READY",
                ..ui::State::default()
            };
            ui::render(
                pixels
                    .as_mut_slice()
                    .try_into()
                    .map_err(|_| "framebuffer size")?,
                &state,
            );
            let mut file = fs::File::create(options.required("--out")?)?;
            write!(file, "P6\n128 160\n255\n")?;
            for pair in pixels.as_chunks::<2>().0 {
                let p = u16::from_be_bytes([pair[0], pair[1]]);
                file.write_all(&[
                    ((p >> 11) * 255 / 31) as u8,
                    (((p >> 5) & 63) * 255 / 63) as u8,
                    ((p & 31) * 255 / 31) as u8,
                ])?;
            }
        }
        "scan" | "monitor" | "upload" | "info" | "exit-recovery" => {
            #[cfg(feature = "radio")]
            {
                radio::run(options)?;
            }
            #[cfg(not(feature = "radio"))]
            {
                return Err("build with --features radio for BLE/serial commands".into());
            }
        }
        "help" | "--help" | "-h" => println!(
            "coral-host commands:\n  package --app APP.bin --boot BOOT.bin --out DIR\n  inspect APP.ota\n  replay RAW_CAPTURE --out telemetry.csv\n  render --out ui.ppm\n  scan\n  monitor --ble ADDRESS|--port COMx [--seconds N] [--out CSV] [--stop]\n  upload APP.ota [--ble ADDRESS]\n  info [--ble ADDRESS]\n  exit-recovery [--ble ADDRESS]\nBLE/serial require --features radio. Only explicit radio commands connect to hardware."
        ),
        _ => return Err("unknown command; use help".into()),
    }
    Ok(())
}
fn main() {
    if let Err(e) = Options::parse().and_then(run) {
        eprintln!("Error: {e}");
        std::process::exit(1);
    }
}
