#ifndef __UARTAPP_H__
#define __UARTAPP_H__

#include "bsp_system.h"

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void uart_rx_proc(void);

#endif
