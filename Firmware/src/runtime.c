#include "board.h"

int main(void);

/* CMSIS startup initializes .data and .bss before entering this C-only app. */
_Noreturn void _start(void)
{
	(void)main();
	board_panic();
}
