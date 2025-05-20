/*
 * uart_out.c
 *
 *  Created on: Dec 17, 2024
 *      Author: frank
 */

#include "uart_print.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "stm32g0xx_hal.h"

extern UART_HandleTypeDef huart2;

static UART_HandleTypeDef *active_uart = NULL;
static uint8_t initialized = 0;

static void init_uart_print(UART_HandleTypeDef *huart) {
    huart->Init.BaudRate = 115200;
    huart->Init.WordLength = UART_WORDLENGTH_8B;
    huart->Init.StopBits = UART_STOPBITS_1;
    huart->Init.Parity = UART_PARITY_NONE;
    huart->Init.Mode = UART_MODE_TX_RX;
    huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart->Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(huart) != HAL_OK) {
        // handle error
    }
    initialized = 1;
}

// Allow user to override the default UART
void uart_print_init(UART_HandleTypeDef *huart) {
    active_uart = huart;
    initialized = 0;  // force re-init if needed
}

void uart_print(const char *str) {
    if (!active_uart) active_uart = &huart2;
    if (!initialized) init_uart_print(active_uart);

    char buffer[strlen(str) + 2];
    snprintf(buffer, sizeof(buffer), "%s\n", str);
    HAL_UART_Transmit(active_uart, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
}

/*

void uart_print(UART_HandleTypeDef *huart, char *str)
{
    if (!initialized) {
        init_uart_print(huart);
        initialized = 1;
    }

    char buffer[strlen(str) + 2];       // Buffer for the formatted message
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s\n", str);

    // Transmit the message
    HAL_UART_Transmit(huart, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
}
*/


