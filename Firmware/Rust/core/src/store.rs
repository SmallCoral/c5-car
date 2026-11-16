use crate::ota::*;

/// Backends must enforce slot boundaries and report torn reads/writes as errors.
pub trait Flash {
    fn read(&mut self, address: u32, data: &mut [u8]) -> Result<()>;
    fn erase(&mut self, page: u32) -> Result<()>;
    fn program(&mut self, address: u32, data: &[u8]) -> Result<()>;
    fn progress(&mut self, _done: u32, _total: u32) {}
}
pub fn verify(flash: &mut impl Flash, address: u32, header: Header) -> Result<()> {
    Header::decode(&header.encode())?;
    let mut data = [0; CHUNK];
    flash.read(address, &mut data[..8])?;
    if !vectors_valid(&data, header.length) {
        return Err(Status::BadImage);
    }
    let mut crc_value = u32::MAX;
    for offset in (0..header.length).step_by(CHUNK) {
        let size = (header.length - offset).min(CHUNK as u32) as usize;
        flash.read(address + offset, &mut data[..size])?;
        crc_value = crc_update(crc_value, &data[..size]);
    }
    if !crc_value != header.crc {
        return Err(Status::BadImage);
    }
    Ok(())
}
pub fn image(flash: &mut impl Flash, address: u32, meta: u32) -> Result<Header> {
    let mut b = [0; 32];
    flash.read(meta, &mut b)?;
    let h = Header::decode(&b)?;
    verify(flash, address, h)?;
    Ok(h)
}
#[derive(Default)]
pub struct Download {
    pub header: Header,
    pub received: u32,
    pub active: bool,
    pub committed: bool,
}
impl Download {
    pub fn begin(&mut self, flash: &mut impl Flash, header: Header) -> Result<()> {
        Header::decode(&header.encode())?;
        if self.active && self.header == header {
            return Ok(());
        }
        if self.committed {
            return Err(Status::BadState);
        }
        self.active = false;
        flash.erase(STAGE_META)?;
        for offset in (0..header.length).step_by(PAGE_SIZE as usize) {
            flash.erase(STAGE_ADDR + offset)?;
        }
        self.header = header;
        self.received = 0;
        self.active = true;
        Ok(())
    }
    pub fn data(&mut self, flash: &mut impl Flash, offset: u32, data: &[u8]) -> Result<()> {
        if !self.active || self.committed {
            return Err(Status::BadState);
        }
        let len = data.len();
        if len == 0
            || len > CHUNK
            || !offset.is_multiple_of(CHUNK as u32)
            || offset >= self.header.length
            || len as u32 > self.header.length - offset
            || (len != CHUNK && offset + len as u32 != self.header.length)
        {
            return Err(Status::BadArgument);
        }
        let mut buffer = [0xff; CHUNK];
        if offset < self.received {
            if offset + len as u32 > self.received {
                return Err(Status::BadOffset);
            }
            flash.read(STAGE_ADDR + offset, &mut buffer[..len])?;
            return if buffer[..len] == *data {
                Ok(())
            } else {
                Err(Status::BadOffset)
            };
        }
        if offset != self.received {
            return Err(Status::BadOffset);
        }
        buffer[..len].copy_from_slice(data);
        if flash
            .program(STAGE_ADDR + offset, &buffer[..(len + 15) & !15])
            .is_err()
        {
            self.active = false;
            return Err(Status::FlashError);
        }
        self.received += len as u32;
        Ok(())
    }
    pub fn end(&mut self, flash: &mut impl Flash) -> Result<()> {
        if self.committed {
            return Ok(());
        }
        if !self.active || self.received != self.header.length {
            return Err(Status::BadState);
        }
        verify(flash, STAGE_ADDR, self.header)?;
        if flash.program(STAGE_META, &self.header.encode()).is_err() {
            self.active = false;
            return Err(Status::FlashError);
        }
        self.committed = true;
        Ok(())
    }
    pub fn abort(&mut self) -> Result<()> {
        if self.committed {
            Err(Status::BadState)
        } else {
            self.active = false;
            Ok(())
        }
    }
}
pub fn install(flash: &mut impl Flash) -> Result<()> {
    let staged = image(flash, STAGE_ADDR, STAGE_META)?;
    if image(flash, APP_ADDR, APP_META) == Ok(staged) {
        return flash.erase(STAGE_META);
    }
    flash.erase(APP_META)?;
    for offset in (0..staged.length).step_by(PAGE_SIZE as usize) {
        flash.erase(APP_ADDR + offset)?;
    }
    let mut data = [0xff; CHUNK];
    for offset in (0..staged.length).step_by(CHUNK) {
        let size = (staged.length - offset).min(CHUNK as u32) as usize;
        data.fill(0xff);
        flash.read(STAGE_ADDR + offset, &mut data[..size])?;
        flash.program(APP_ADDR + offset, &data[..(size + 15) & !15])?;
        flash.progress(offset + size as u32, staged.length);
    }
    verify(flash, APP_ADDR, staged)?;
    flash.program(APP_META, &staged.encode())?;
    flash.erase(STAGE_META)
}

pub fn within(address: u32, len: usize, base: u32, size: u32) -> bool {
    address >= base && address - base < size && len <= (size - (address - base)) as usize
}
pub fn writable(address: u32, len: usize) -> bool {
    within(address, len, APP_ADDR, IMAGE_MAX)
        || within(address, len, STAGE_ADDR, IMAGE_MAX)
        || within(address, len, APP_META, 32)
        || within(address, len, STAGE_META, 32)
}
