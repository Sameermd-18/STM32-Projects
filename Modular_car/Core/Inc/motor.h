#ifndef MOTOR_H
#define MOTOR_H

#include "stm32f1xx_hal.h"

void Motor_Init(void);

void Car_Forward(uint16_t speed);
void Car_Backward(uint16_t speed);
void Car_Left(uint16_t speed);
void Car_Right(uint16_t speed);
void Car_Stop(void);

#endif
