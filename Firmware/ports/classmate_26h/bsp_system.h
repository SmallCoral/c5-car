#ifndef CORAL_F407_BALANCE_IO_H
#define CORAL_F407_BALANCE_IO_H

#include <math.h>
#include <stdint.h>

/* 旧 F407 控制器只从板级适配层读取球位、读取轴角、请求带方向的转速。 */
struct f407_site { int16_t X_0; };
extern struct f407_site site;
extern float Now_Angle;
/* 由 C5 适配层接收方向位和转速，之后交给步进状态机发送。 */
void Contron_X_Speed(uint8_t dir, uint16_t rpm);

#endif
