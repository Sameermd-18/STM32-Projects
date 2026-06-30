#include "bt_uart.h"
#include "string.h"
#include "motor.h"

/* UART handle from main.c */
extern UART_HandleTypeDef huart1;

/* RX variables */
static uint8_t rx_char;
static char rx_buf[10];
static uint8_t idx = 0;

void BT_UART_Init(void)
{
    HAL_UART_Receive_IT(&huart1, &rx_char, 1);
}

void BT_ProcessCommand(char *cmd)
{
    if (strcmp(cmd, "f") == 0)
        Car_Right(209);
    else if (strcmp(cmd, "b") == 0)
        Car_Left(209);
    else if (strcmp(cmd, "r") == 0)
        Car_Forward(209);
    else if (strcmp(cmd, "l") == 0)
        Car_Backward(209);
    else if (strcmp(cmd, "s") == 0)
        Car_Stop();
}

/* UART RX complete callback handler */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (rx_char == '\r' || rx_char == '\n')
        {
            rx_buf[idx] = '\0';
            BT_ProcessCommand(rx_buf);
            idx = 0;
            memset(rx_buf, 0, sizeof(rx_buf));
        }
        else
        {
            if (idx < sizeof(rx_buf) - 1)
                rx_buf[idx++] = rx_char;
        }

        HAL_UART_Receive_IT(&huart1, &rx_char, 1);
    }
}
