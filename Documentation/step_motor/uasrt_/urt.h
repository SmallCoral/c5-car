#ifndef __URT_H
#define __URT_H

#include <main.h>

extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart4;

extern uint8_t serial_Motor_X_Flag;
extern uint8_t Motor_X_RxBuffer[10];

extern uint8_t serial_Motor_Y_Flag;
extern uint8_t Motor_Y_RxBuffer[10];

extern uint8_t Serial_RxFlag;
extern char Serial_RxPacket[50];

void Motor_Y_Init(void) ;
void Motor_X_Init(void);
void Maxicam_Init(void);
void LY_Init(void);

#endif

