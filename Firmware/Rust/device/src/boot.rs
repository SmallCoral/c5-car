use crate::{
    board,
    display::Display,
    flash::Storage,
    runtime,
    serial::{self, Id},
};
use coral_core::{
    emm,
    ota::*,
    store::{self, Download, Flash},
    ui::State,
};
unsafe extern "C" {
    fn enter_application(sp: u32, entry: u32) -> !;
}
fn jump() -> ! {
    let _interrupts = runtime::InterruptGuard::lock();
    board::write(0xe000_e010, 0);
    board::write(0xe000_e014, 0);
    board::write(0xe000_e018, 0);
    for i in 0..8 {
        board::write(0xe000_e180 + i * 4, u32::MAX);
        board::write(0xe000_e280 + i * 4, u32::MAX);
    }
    board::write(0xe000_ed04, (1 << 25) | (1 << 27));
    board::write(0xe000_ed08, APP_ADDR);
    runtime::dsb();
    runtime::isb();
    // SAFETY: caller verified the application CRC/vectors; assembly restores MSP and CONTROL before branch.
    unsafe { enter_application(board::read(APP_ADDR), board::read(APP_ADDR + 4)) }
}
fn stop_stepper() {
    serial::send(Id::Step, &emm::stop());
    serial::send(Id::Step, &emm::enable(false));
}
fn command(
    frame: &Frame,
    download: &mut Download,
    flash: &mut Storage<'_>,
    app: Option<Header>,
) -> bool {
    let data = frame.payload();
    let mut reboot = false;
    let result = match frame.command {
        HELLO if data.is_empty() => Ok(()),
        BEGIN => match Header::decode(data) {
            Ok(h) => {
                flash.screen.update_status = "ERASING STAGE";
                let r = download.begin(flash, h);
                flash.screen.update_status = if r.is_ok() {
                    "RECEIVING"
                } else {
                    "BEGIN FAILED"
                };
                r
            }
            Err(e) => Err(e),
        },
        DATA if data.len() > 4 => download.data(flash, get32(data), &data[4..]),
        END if data.is_empty() => {
            flash.screen.update_status = "VERIFYING";
            let r = download.end(flash);
            flash.screen.update_status = if r.is_ok() {
                "READY TO INSTALL"
            } else {
                "VERIFY FAILED"
            };
            r
        }
        RUN if data.is_empty() => {
            if download.committed || app.is_some() {
                reboot = true;
                Ok(())
            } else {
                Err(Status::BadState)
            }
        }
        ABORT if data.is_empty() && !download.committed => {
            flash.screen.update_status = "WAITING FOR PC";
            download.abort()
        }
        _ => Err(Status::BadArgument),
    };
    let mut payload = [0; 32];
    put32(&mut payload, result.err().map(|s| s as u32).unwrap_or(0));
    put32(&mut payload[4..], download.received);
    let len = if frame.command == HELLO {
        for (i, n) in [
            BOARD_ID,
            APP_ADDR,
            IMAGE_MAX,
            CHUNK as u32,
            app.map(|h| h.length).unwrap_or(0),
            app.map(|h| h.crc).unwrap_or(0),
        ]
        .iter()
        .enumerate()
        {
            put32(&mut payload[8 + i * 4..], *n);
        }
        32
    } else {
        8
    };
    if let Ok(response) = Frame::new(frame.command | 0x80, frame.sequence, &payload[..len]) {
        let mut bytes = [0; FRAME_MAX];
        let len = response.encode(&mut bytes);
        serial::send(Id::Bt, &bytes[..len]);
    }
    reboot
}
pub fn run() -> ! {
    let requested = board::read(REQUEST_ADDR) == REQUEST_MAGIC
        && board::read(REQUEST_ADDR + 4) == !REQUEST_MAGIC;
    board::write(REQUEST_ADDR, 0);
    board::write(REQUEST_ADDR + 4, 0);
    if !board::init() {
        board::panic_stop();
    }
    let mut display = Display::new();
    let mut screen = State {
        updating: true,
        ..State::default()
    };
    let mut download = Download::default();
    let mut flash = Storage {
        display: &mut display,
        screen: &mut screen,
    };
    let mut app = store::image(&mut flash, APP_ADDR, APP_META).ok();
    if let Ok(stage) = store::image(&mut flash, STAGE_ADDR, STAGE_META) {
        if !serial::init() {
            board::panic_stop();
        }
        stop_stepper();
        flash.display.init();
        flash.screen.update_status = "INSTALLING";
        if store::install(&mut flash).is_ok() {
            runtime::reset();
        }
        download.header = stage;
        download.received = stage.length;
        download.committed = true;
        app = store::image(&mut flash, APP_ADDR, APP_META).ok();
        flash.screen.update_status = "INSTALL FAILED";
    } else if app.is_some() && !requested && board::keys() & 1 == 0 {
        jump();
    } else {
        if !serial::init() {
            board::panic_stop();
        }
        stop_stepper();
        flash.display.init();
    }
    let mut parser = Parser::new();
    let mut last_byte = 0;
    let mut reset_at = 0;
    let mut reboot = false;
    loop {
        let now = board::millis();
        board::heartbeat();
        if !parser.is_empty() && now.wrapping_sub(last_byte) > 1000 {
            parser.clear();
        }
        while !reboot {
            let Some(b) = serial::receive(Id::Bt) else {
                break;
            };
            last_byte = now;
            if let Some(frame) = parser.feed(b)
                && command(&frame, &mut download, &mut flash, app)
            {
                reboot = true;
                reset_at = now;
            }
        }
        if reboot
            && now.wrapping_sub(reset_at) >= 300
            && serial::idle(Id::Bt)
            && serial::idle(Id::Step)
        {
            runtime::reset();
        }
        flash.progress(download.received, download.header.length);
        board::led((now / 150) & 1 != 0);
    }
}
