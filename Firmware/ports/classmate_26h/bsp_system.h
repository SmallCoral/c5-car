#ifndef CORAL_F407_BALANCE_IO_H
#define CORAL_F407_BALANCE_IO_H

#include <math.h>
#include <stdint.h>

/* The three board dependencies used by Partical/balance.c in the F407 archive. */
struct f407_site { int16_t X_0; };
extern struct f407_site site;
extern float Now_Angle;
void Contron_X_Speed(uint8_t dir, uint16_t rpm);

#endif
