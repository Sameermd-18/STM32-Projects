#include "motor.h"

/* TIM handle from main.c */
extern TIM_HandleTypeDef htim3;

/* ----------- private helpers ----------- */

static void right_motor_forward(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
}

static void left_motor_forward(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
}

static void right_motor_backward(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
}

static void left_motor_backward(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
}

static void set_speed(uint16_t speed)
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, speed);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, speed);
}

/* ----------- public APIs ----------- */

void Motor_Init(void)
{
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
}

void Car_Forward(uint16_t speed)
{
    right_motor_forward();
    left_motor_forward();
    set_speed(speed);
}

void Car_Backward(uint16_t speed)
{
    right_motor_backward();
    left_motor_backward();
    set_speed(speed);
}

void Car_Right(uint16_t speed)
{
    right_motor_backward();
    left_motor_forward();
    set_speed(speed);
}

void Car_Left(uint16_t speed)
{
    right_motor_forward();
    left_motor_backward();
    set_speed(speed);
}

void Car_Stop(void)
{
    set_speed(0);
}
