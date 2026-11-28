/* 连接 CMSIS 启动代码与应用入口，使复位后的执行流进入主循环。 */

#include "board.h"

int main(void);

/* CMSIS 已完成 .data 和 .bss 初始化，这里直接转入应用 main。 */
_Noreturn void _start(void)
{
	(void)main();
	board_panic();
}
