/*
 * uart_print.h
 *
 *  Created on: Apr 23, 2025
 *      Author: frank
 */

#ifndef INC_UART_PRINT_H_
#define INC_UART_PRINT_H_


#include "stm32g0xx_hal.h"


// Function declarations
// used to override default use of USART2 instance. If using built in VCOM port via a Nucleo's STLink, ignore this.
void uart_print_init(UART_HandleTypeDef *huart);
//
void uart_print(const char *str);

#endif /* INC_UART_PRINT_H_ */
