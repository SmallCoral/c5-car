// 电脑端 BLE 传输，负责发现 HC-08、订阅通知、连接恢复和分包确认。

use crate::{Options, Sink};
use btleplug::{
    api::{
        Central, Characteristic, Manager as _, Peripheral as _, ScanFilter, ValueNotification,
        WriteType,
    },
    platform::{Manager, Peripheral},
};
use coral_core::ota::{self, ABORT, BEGIN, CHUNK, DATA, END, Frame, HELLO, Header, Parser, RUN};
use coral_host::Result;
use futures::{StreamExt, stream::BoxStream};
use std::{
    collections::VecDeque,
    io::{self, Read, Write},
    time::{Duration, Instant},
};
const DEFAULT_ADDRESS: &str = "F8:2E:0C:F3:68:37";
// 构造蓝牙断连错误供重试层识别。
fn disconnected() -> Box<dyn std::error::Error + Send + Sync> {
    io::Error::new(io::ErrorKind::ConnectionReset, "BLE disconnected").into()
}
struct Link {
    device: Peripheral,
    characteristic: Characteristic,
    notifications: BoxStream<'static, ValueNotification>,
    parser: Parser,
    frames: VecDeque<Frame>,
    lines: VecDeque<String>,
    text: Vec<u8>,
    sequence: u32,
    received: u32,
}
impl Link {
    // 扫描并连接目标设备，定位适用特征并订阅通知。
    async fn connect(options: &Options) -> Result<Self> {
        let manager = Manager::new().await?;
        let adapter = manager
            .adapters()
            .await?
            .into_iter()
            .next()
            .ok_or("no Bluetooth adapter")?;
        adapter.start_scan(ScanFilter::default()).await?;
        let address = options
            .get("--ble")
            .unwrap_or(DEFAULT_ADDRESS)
            .to_ascii_uppercase();
        let deadline = Instant::now() + Duration::from_secs(15);
        let device = loop {
            let candidates = adapter.peripherals().await?;
            if let Some(p) = candidates
                .into_iter()
                .find(|p| p.address().to_string().to_ascii_uppercase() == address)
            {
                break p;
            }
            if Instant::now() >= deadline {
                adapter.stop_scan().await?;
                return Err(io::Error::new(io::ErrorKind::TimedOut, "HC-08 not found").into());
            }
            tokio::time::sleep(Duration::from_millis(250)).await;
        };
        adapter.stop_scan().await?;
        tokio::time::timeout(Duration::from_secs(15), device.connect()).await??;
        let setup = async {
            device.discover_services().await?;
            let uuid = options
                .get("--uuid")
                .unwrap_or("0000ffe1-0000-1000-8000-00805f9b34fb");
            let characteristic = device
                .characteristics()
                .into_iter()
                .find(|c| c.uuid.to_string().eq_ignore_ascii_case(uuid))
                .ok_or("FFE1 characteristic not found")?;
            let notifications = device.notifications().await?;
            device.subscribe(&characteristic).await?;
            Ok::<_, Box<dyn std::error::Error + Send + Sync>>((characteristic, notifications))
        }
        .await;
        let (characteristic, notifications) = match setup {
            Ok(v) => v,
            Err(e) => {
                let _ = device.disconnect().await;
                return Err(e);
            }
        };
        tokio::time::sleep(Duration::from_millis(500)).await;
        Ok(Self {
            device,
            characteristic,
            notifications,
            parser: Parser::new(),
            frames: VecDeque::new(),
            lines: VecDeque::new(),
            text: Vec::new(),
            sequence: 0,
            received: 0,
        })
    }
    // 按特征允许的片段写入数据。
    async fn write(&self, bytes: &[u8]) -> Result<()> {
        for part in bytes.chunks(20) {
            self.device
                .write(&self.characteristic, part, WriteType::WithoutResponse)
                .await?;
            tokio::time::sleep(Duration::from_millis(20)).await;
        }
        Ok(())
    }
    // 把通知送入二进制帧和文本行解析。
    fn accept(&mut self, data: &[u8]) {
        for &b in data {
            if let Some(frame) = self.parser.feed(b) {
                if self.frames.len() == 64 {
                    self.frames.pop_front();
                }
                self.frames.push_back(frame);
            }
            if b == b'\n' {
                if self.lines.len() == 64 {
                    self.lines.pop_front();
                }
                self.lines
                    .push_back(String::from_utf8_lossy(&self.text).trim().to_owned());
                self.text.clear();
            } else if b == b'\r' || (32..127).contains(&b) {
                self.text.push(b);
                if self.text.len() > 512 {
                    self.text.clear();
                }
            } else {
                self.text.clear();
            }
        }
    }
    // 等待下一条通知或对应的截止时间。
    async fn next(&mut self, deadline: Instant) -> Result<()> {
        if Instant::now() >= deadline {
            return Err(
                io::Error::new(io::ErrorKind::TimedOut, "BLE notification deadline").into(),
            );
        }
        let timeout = deadline.saturating_duration_since(Instant::now());
        let n = tokio::time::timeout(timeout, self.notifications.next())
            .await?
            .ok_or_else(disconnected)?;
        if n.uuid == self.characteristic.uuid {
            self.accept(&n.value);
        }
        Ok(())
    }
    // 通过命令应答机制发出一次协议请求。
    async fn request(&mut self, command: u8, payload: &[u8], attempts: u32) -> Result<Vec<u8>> {
        self.sequence = self.sequence.wrapping_add(1);
        let sequence = self.sequence;
        coral_host::exchange::request(
            self,
            command,
            sequence,
            payload,
            attempts,
            Duration::from_secs(3),
        )
        .await
    }
    // 请求设备进入引导程序并读取其下载状态。
    async fn enter(&mut self) -> Result<(u32, u32)> {
        self.write(b"!\nC5:OTA\n").await?;
        tokio::time::sleep(Duration::from_secs(2)).await;
        let p = self.request(HELLO, &[], 4).await?;
        let hello = coral_host::Hello::decode(&p)?;
        self.received = hello.received;
        eprintln!(
            "Bootloader ready, current app {} bytes CRC32={:08X}, received={}",
            hello.length, hello.crc, self.received
        );
        Ok((hello.length, hello.crc))
    }
    // 查询已运行应用的长度和校验值。
    async fn verify(&mut self, length: u32, crc: u32) -> Result<()> {
        let deadline = Instant::now() + Duration::from_secs(20);
        // 丢弃安装前残留的旧身份通知，随后核对新应用。
        self.lines.clear();
        while Instant::now() < deadline {
            self.write(b"\nC5:INFO\n").await?;
            let stop = deadline.min(Instant::now() + Duration::from_millis(1500));
            loop {
                while let Some(line) = self.lines.pop_front() {
                    if !line.starts_with("C5APP,1,") {
                        continue;
                    }
                    coral_host::verify_identity(&line, length, crc)?;
                    println!("Verified running application: {length} bytes CRC32={crc:08X}");
                    return Ok(());
                }
                match self.next(stop).await {
                    Ok(()) => {}
                    Err(e) if is_timeout(e.as_ref()) => break,
                    Err(e) => return Err(e),
                }
            }
        }
        Err(io::Error::new(
            io::ErrorKind::TimedOut,
            "application confirmation missing; reconnect and query info",
        )
        .into())
    }
    // 读取并显示目标设备的身份。
    async fn info(&mut self) -> Result<()> {
        self.write(b"\nC5:INFO\n").await?;
        let deadline = Instant::now() + Duration::from_secs(4);
        loop {
            while let Some(line) = self.lines.pop_front() {
                if line.starts_with("C5APP,1,") {
                    println!("{line}");
                    return Ok(());
                }
            }
            match self.next(deadline).await {
                Ok(()) => {}
                Err(e) if is_timeout(e.as_ref()) => break,
                Err(e) => return Err(e),
            }
        }
        let p = self.request(HELLO, &[], 2).await?;
        let h = coral_host::Hello::decode(&p)?;
        println!(
            "Bootloader: app {} bytes CRC32={:08X}, received={}",
            h.length, h.crc, h.received
        );
        Ok(())
    }
    // 按暂存进度分块上传镜像，确认提交与启动。
    async fn upload(&mut self, image: &[u8], header: Header) -> Result<()> {
        let (size, crc) = self.enter().await?;
        if self.received == 0 && (size, crc) == (header.length, header.crc) {
            self.request(RUN, &[], 1).await?;
            tokio::time::sleep(Duration::from_secs(2)).await;
            return self.verify(size, crc).await;
        }
        let p = self.request(BEGIN, &header.encode(), 4).await?;
        let mut offset = coral_host::resume_offset(&p, image.len())?;
        while offset < image.len() {
            let chunk = &image[offset..(offset + CHUNK).min(image.len())];
            let mut bytes = Vec::from((offset as u32).to_le_bytes());
            bytes.extend_from_slice(chunk);
            let ack = self.request(DATA, &bytes, 4).await?;
            let next = ota::get32(&ack[4..]) as usize;
            if next != offset + chunk.len() {
                return Err("unexpected DATA ACK offset".into());
            }
            offset = next;
            eprint!("\r{offset}/{} bytes", image.len());
            io::stderr().flush()?;
        }
        eprintln!();
        self.request(END, &[], 4).await?;
        if let Err(e) = self.request(RUN, &[], 1).await
            && !e
                .downcast_ref::<io::Error>()
                .is_some_and(|e| e.kind() == io::ErrorKind::TimedOut)
        {
            return Err(e);
        }
        tokio::time::sleep(Duration::from_secs(2)).await;
        self.verify(header.length, header.crc).await
    }
}
// 检查错误是否由等待超时引起。
fn is_timeout(e: &(dyn std::error::Error + Send + Sync + 'static)) -> bool {
    e.is::<tokio::time::error::Elapsed>()
        || e.downcast_ref::<io::Error>()
            .is_some_and(|e| e.kind() == io::ErrorKind::TimedOut)
}
impl coral_host::exchange::Transport for Link {
    // 把预编码的命令写入蓝牙特征。
    async fn send(&mut self, bytes: &[u8]) -> Result<()> {
        tokio::time::timeout(Duration::from_secs(10), self.write(bytes)).await??;
        Ok(())
    }
    // 读取并匹配引导程序回包。
    async fn response(&mut self, deadline: Instant) -> Result<Frame> {
        loop {
            if let Some(frame) = self.frames.pop_front() {
                return Ok(frame);
            }
            if let Err(e) = self.next(deadline).await {
                return Err(if is_timeout(e.as_ref()) {
                    io::Error::new(io::ErrorKind::TimedOut, "OTA response deadline").into()
                } else {
                    e
                });
            }
        }
    }
}
// 从命令行选项取得以秒为单位的时限。
fn seconds(options: &Options) -> Result<Option<Duration>> {
    let n = options.get("--seconds").unwrap_or("0").parse::<f64>()?;
    if !n.is_finite() || n < 0.0 {
        return Err("seconds must be finite and nonnegative".into());
    }
    if n == 0.0 {
        Ok(None)
    } else {
        Ok(Some(Duration::try_from_secs_f64(n)?))
    }
}
// 持续订阅、解码和输出蓝牙遥测。
async fn monitor(link: &mut Link, options: &Options) -> Result<()> {
    let duration = seconds(options)?;
    let deadline = duration.map(|d| Instant::now() + d);
    let mut sink = Sink::new(options.get("--out"))?;
    if options.stop {
        link.write(b"!").await?;
    }
    loop {
        let remaining = deadline
            .map(|t| t.saturating_duration_since(Instant::now()))
            .unwrap_or(Duration::from_secs(3600));
        if remaining.is_zero() {
            break;
        }
        tokio::select! {
            _=tokio::signal::ctrl_c()=>break,
            result=tokio::time::timeout(remaining,link.notifications.next())=>match result {
                Ok(Some(n))=>{if n.uuid==link.characteristic.uuid {sink.accept(&n.value)?;}},Ok(None)=>return Err(disconnected()),Err(_)=>if deadline.is_some() {break;}
            }
        }
    }
    if sink.count == 0 {
        return Err("no valid C5 telemetry received".into());
    }
    Ok(())
}
// 按用户所选子命令执行查询或更新。
async fn execute(options: Options) -> Result<()> {
    if options.command == "scan" {
        let manager = Manager::new().await?;
        let adapters = manager.adapters().await?;
        let adapter = adapters.first().ok_or("no Bluetooth adapter")?;
        adapter.start_scan(ScanFilter::default()).await?;
        tokio::time::sleep(Duration::from_secs(8)).await;
        let devices = adapter.peripherals().await?;
        adapter.stop_scan().await?;
        for p in devices {
            let name = p
                .properties()
                .await?
                .and_then(|x| x.local_name)
                .unwrap_or_default();
            println!("{} {name}", p.address());
        }
        return Ok(());
    }
    if options.command == "monitor"
        && let Some(port) = options.get("--port")
    {
        if options.get("--ble").is_some() {
            return Err("choose --port or --ble".into());
        }
        let baud = options.get("--baud").unwrap_or("57600").parse::<u32>()?;
        if baud == 0 {
            return Err("baud must be positive".into());
        }
        let deadline = seconds(&options)?.map(|d| Instant::now() + d);
        let mut sink = Sink::new(options.get("--out"))?;
        let mut port = serialport::new(port, baud)
            .timeout(Duration::from_millis(200))
            .open()?;
        if options.stop {
            port.write_all(b"!")?;
        }
        let mut bytes = [0; 256];
        while deadline.is_none_or(|d| Instant::now() < d) {
            match port.read(&mut bytes) {
                Ok(n) => sink.accept(&bytes[..n])?,
                Err(e) if e.kind() == io::ErrorKind::TimedOut => {}
                Err(e) => return Err(e.into()),
            }
        }
        if sink.count == 0 {
            return Err("no valid C5 telemetry received".into());
        }
        return Ok(());
    }
    let image = if options.command == "upload" {
        Some(coral_host::read_image(options.image()?)?)
    } else {
        None
    };
    for attempt in 0..3 {
        let result = async {
            let mut link = Link::connect(&options).await?;
            let result = match options.command.as_str() {
                "monitor" => monitor(&mut link, &options).await,
                "info" => link.info().await,
                "exit-recovery" => {
                    async {
                        let (size, crc) = link.enter().await?;
                        if size == 0 {
                            return Err("no valid application".into());
                        }
                        link.request(ABORT, &[], 4).await?;
                        link.request(RUN, &[], 1).await?;
                        tokio::time::sleep(Duration::from_secs(2)).await;
                        link.verify(size, crc).await
                    }
                    .await
                }
                "upload" => {
                    let (bytes, header) = image.as_ref().ok_or("missing image")?;
                    link.upload(bytes, *header).await
                }
                _ => Err("unknown radio command".into()),
            };
            let disconnected = link.device.disconnect().await;
            result?;
            disconnected?;
            Ok::<_, Box<dyn std::error::Error + Send + Sync>>(())
        }
        .await;
        match result {
            Ok(()) => return Ok(()),
            Err(e)
                if attempt < 2
                    && options.command != "monitor"
                    && (e.is::<btleplug::Error>()
                        || e.is::<io::Error>()
                        || e.is::<tokio::time::error::Elapsed>()) =>
            {
                eprintln!("BLE interrupted ({e}); reconnecting {}/2", attempt + 1);
                tokio::time::sleep(Duration::from_secs(2)).await;
            }
            Err(e) => return Err(e),
        }
    }
    Err("reconnection attempts exhausted".into())
}
// 建立异步运行环境并执行蓝牙操作。
pub fn run(options: Options) -> Result<()> {
    tokio::runtime::Runtime::new()?.block_on(execute(options))
}
