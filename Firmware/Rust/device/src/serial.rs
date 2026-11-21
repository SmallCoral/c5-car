// 以中断与环形缓冲解耦 USART 和前台，分别向蓝牙与步进协议提供字节流。

use crate::{
    board::{self, modify, read, write, write8},
    registers::*,
    runtime::Shared,
};
use coral_core::{BT_BAUD, CLOCK_HZ, STEP_BAUD, elapsed, emm};
#[derive(Clone, Copy, Default)]
pub struct Stats {
    pub rx_bytes: u32,
    pub tx_bytes: u32,
    pub errors: u32,
    pub rx_drops: u32,
    pub tx_drops: u32,
}
struct Ring {
    bytes: [u8; 512],
    head: usize,
    tail: usize,
}
impl Ring {
    // 初始化读写索引以建立空环形缓冲。
    const fn new() -> Self {
        Self {
            bytes: [0; 512],
            head: 0,
            tail: 0,
        }
    }
    // 计算当前剩余可写容量。
    fn free(&self) -> usize {
        self.tail.wrapping_sub(self.head).wrapping_sub(1) & 511
    }
    // 从队首取出一个字节。
    fn pop(&mut self) -> Option<u8> {
        if self.head == self.tail {
            None
        } else {
            let b = self.bytes[self.tail];
            self.tail = (self.tail + 1) & 511;
            Some(b)
        }
    }
    // 将一个字节追加到队尾。
    fn push(&mut self, b: u8) -> bool {
        if self.free() == 0 {
            false
        } else {
            self.bytes[self.head] = b;
            self.head = (self.head + 1) & 511;
            true
        }
    }
}
struct Port {
    base: u32,
    tx: Ring,
    rx: Ring,
    stats: Stats,
}
impl Port {
    // 初始化串口通道的软件队列与统计。
    const fn new(base: u32) -> Self {
        Self {
            base,
            tx: Ring::new(),
            rx: Ring::new(),
            stats: Stats {
                rx_bytes: 0,
                tx_bytes: 0,
                errors: 0,
                rx_drops: 0,
                tx_drops: 0,
            },
        }
    }
}
static BT: Shared<Port> = Shared::new(Port::new(USART1_BASE));
static STEP: Shared<Port> = Shared::new(Port::new(USART6_BASE));
#[derive(Clone, Copy)]
pub enum Id {
    Bt,
    Step,
}
// 按端口编号选择相应的串口状态。
fn port(id: Id) -> &'static Shared<Port> {
    match id {
        Id::Bt => &BT,
        Id::Step => &STEP,
    }
}
// 设置串口波特率、中断和引脚。
pub fn init() -> bool {
    for (base, baud, irq) in [(USART1_BASE, BT_BAUD, 51), (USART6_BASE, STEP_BAUD, 96)] {
        write(base + USART_CR1, 0);
        write(base + USART_CR2, 0);
        write(base + USART_CR3, 0);
        write(base + USART_PRESC, 0);
        write(base + USART_BRR, (CLOCK_HZ + baud / 2) / baud);
        write(base + USART_CR1, USART_CR1_UE | USART_CR1_TE | USART_CR1_RE);
        let start = board::millis();
        let ack = USART_ISR_TEACK | USART_ISR_REACK;
        while read(base + USART_ISR) & ack != ack {
            if elapsed(board::millis(), start, 20) {
                return false;
            }
        }
        modify(base + USART_CR1, 0, USART_CR1_RXNEIE_RXFNEIE);
        modify(base + USART_CR3, 0, USART_CR3_EIE);
        write8(0xe000_e400 + irq, 2 << 4);
        write(0xe000_e100 + 4 * (irq / 32), 1 << (irq % 32));
    }
    true
}
// 将字节放入发送队列并开启发送中断。
pub fn send(id: Id, bytes: &[u8]) -> bool {
    port(id).with(|p| {
        if bytes.len() > p.tx.free() {
            p.stats.tx_drops = p.stats.tx_drops.wrapping_add(1);
            return false;
        }
        for &b in bytes {
            p.tx.push(b);
        }
        modify(p.base + USART_CR1, 0, USART_CR1_TXEIE_TXFNFIE);
        true
    })
}
// 从接收队列弹出一个可用字节。
pub fn receive(id: Id) -> Option<u8> {
    port(id).with(|p| p.rx.pop())
}
// 读取接收错误和队列溢出统计。
pub fn stats(id: Id) -> Stats {
    port(id).with(|p| p.stats)
}
// 检查软件队列及硬件发送是否已结束。
pub fn idle(id: Id) -> bool {
    port(id).with(|p| p.tx.head == p.tx.tail && read(p.base + USART_ISR) & USART_ISR_TC != 0)
}
// 处理中断标志，收发字节并累计错误计数。
fn irq(p: &mut Port) {
    let status = read(p.base + USART_ISR);
    let errors = status & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE);
    if errors != 0 {
        write(p.base + USART_ICR, errors);
        p.stats.errors = p.stats.errors.wrapping_add(1);
    }
    if status & USART_ISR_RXNE_RXFNE != 0 {
        let b = read(p.base + USART_RDR) as u8;
        p.stats.rx_bytes = p.stats.rx_bytes.wrapping_add(1);
        if errors != 0 || !p.rx.push(b) {
            p.stats.rx_drops = p.stats.rx_drops.wrapping_add(1);
        }
    }
    if read(p.base + USART_CR1) & USART_CR1_TXEIE_TXFNFIE != 0
        && read(p.base + USART_ISR) & USART_ISR_TXE_TXFNF != 0
    {
        if let Some(b) = p.tx.pop() {
            write(p.base + USART_TDR, u32::from(b));
            p.stats.tx_bytes = p.stats.tx_bytes.wrapping_add(1);
        } else {
            modify(p.base + USART_CR1, USART_CR1_TXEIE_TXFNFIE, 0);
        }
    }
}
#[unsafe(no_mangle)]
// 分发蓝牙串口的中断。
extern "C" fn USART1() {
    BT.with(irq);
}
#[unsafe(no_mangle)]
// 分发步进串口的中断。
extern "C" fn USART6() {
    STEP.with(irq);
}
pub struct StepSerial;
impl emm::Serial for StepSerial {
    // 把步进协议帧交给对应串口。
    fn write(&mut self, b: &[u8]) -> bool {
        send(Id::Step, b)
    }
    // 从步进接收队列取一个字节。
    fn read(&mut self) -> Option<u8> {
        receive(Id::Step)
    }
    // 提供步进链路的累计接收错误。
    fn rx_errors(&self) -> u32 {
        let s = stats(Id::Step);
        s.errors.wrapping_add(s.rx_drops)
    }
}
