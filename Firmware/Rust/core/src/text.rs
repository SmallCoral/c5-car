// 供串口和界面共用的定长 UTF-8 文本缓冲，在无堆环境下先检查容量再写入。

use core::fmt;

/// 为串口与屏幕提供定长 UTF-8 格式化，在 MCU 上无需堆分配。
pub struct Text<const N: usize> {
    bytes: [u8; N],
    len: usize,
}
impl<const N: usize> Default for Text<N> {
    // 建立默认的空文本缓冲。
    fn default() -> Self {
        Self::new()
    }
}
impl<const N: usize> Text<N> {
    // 创建容量由类型指定的定长缓冲区。
    pub const fn new() -> Self {
        Self {
            bytes: [0; N],
            len: 0,
        }
    }
    // 重置文本长度以复用存储。
    pub fn clear(&mut self) {
        self.len = 0;
    }
    // 返回已写入部分的字节切片。
    pub fn as_bytes(&self) -> &[u8] {
        &self.bytes[..self.len]
    }
    // 将有效 UTF-8 内容作为字符串借出。
    pub fn as_str(&self) -> &str {
        core::str::from_utf8(self.as_bytes()).unwrap_or("")
    }
}
impl<const N: usize> fmt::Write for Text<N> {
    // 在剩余容量内写入完整字符串并返回溢出错误。
    fn write_str(&mut self, value: &str) -> fmt::Result {
        if value.len() > N - self.len {
            return Err(fmt::Error);
        }
        self.bytes[self.len..self.len + value.len()].copy_from_slice(value.as_bytes());
        self.len += value.len();
        Ok(())
    }
}
