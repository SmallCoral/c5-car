/* LCD 初始化与增量刷屏接口，同时提供最近显示的球位供蓝牙诊断核对。 */

#ifndef CORAL_DISPLAY_H
#define CORAL_DISPLAY_H

#include "ui.h"

/* 按厂家时序复位屏幕并配置扫描方向、像素格式和显示窗口。 */
bool display_init(void);
/* 分次取两行进行绘制和传输，完成当前快照后再换到新状态。 */
bool display_poll(const struct ui_state *state, uint32_t now);
/* 读取最近实际绘到长度条上的球位快照和更新计数。 */
uint32_t display_last_ball(struct ball_view *shown);

#endif
