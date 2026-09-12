#ifndef __TRANSMIT_H
#define __TRANSMIT_H

// include uart header
#include "stm32f4xx_ll_usart.h"

// standard libraries
#include <stddef.h>
#include <stdint.h>

void UART_Send_Char(USART_TypeDef *USARTx, uint8_t data);
void UART_Send_String(USART_TypeDef *USARTx, const char *str);
void UART_Send_Hex(USART_TypeDef *USARTx, const uint8_t *buf, size_t len);
#endif