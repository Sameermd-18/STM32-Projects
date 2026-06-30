#ifndef BT_UART_H
#define BT_UART_H

#include "stm32f1xx_hal.h"

/* Initialize Bluetooth UART reception */
void BT_UART_Init(void);

/* Process received Bluetooth commands */
void BT_ProcessCommand(char *cmd);

#endif
