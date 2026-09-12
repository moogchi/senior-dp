#include "transmit.h"

void UART_Send_Char(USART_TypeDef *USARTx, uint8_t data) {
  while (!LL_USART_IsActiveFlag_TXE(USARTx)) {
    // do nothing till txe buf is empty
  }

  LL_USART_TransmitData8(USARTx, data);
}

void UART_Send_String(USART_TypeDef *USARTx, const char *str) {
  while (*str) { // while string is still not complete
    UART_Send_Char(USARTx, *str++);
  }

  // wait till tx line is empty
  while (!LL_USART_IsActiveFlag_TC(USARTx)) {
  }
}

void UART_Send_Hex(USART_TypeDef *USARTx, const uint8_t *buf, size_t len) {
  const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < len; ++i) {
    UART_Send_Char(USARTx, hex[buf[i] >> 4]);   // most significant 4 bits
    UART_Send_Char(USARTx, hex[buf[i] & 0x0F]); // least significant 4 bits
  }
  UART_Send_String(USARTx, "\r\n");
}