#include "keygen.h"
#include "transmit.h"

void derive_alias_key(uint8_t *secret_key, uint8_t *public_key) {
  // disable dma
  LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_DMA1);
  LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_DMA2);

  UART_Send_String(USART2, "Check if DMA bits are cleared");
  UART_Send_Hex(USART2, (uint8_t *)&RCC->AHB1ENR, 4);
  // find otp key and save it to uds
  const uint8_t *otp = (const uint8_t *)OTP_BASE;
  uint8_t uds[32] = {0};
  for (int i = 0; i < 32; ++i) {
    uds[i] = *otp;
    otp++;
  }
  UART_Send_String(USART2, "UDS: ");
  UART_Send_Hex(USART2, uds, 32);

  // uds is now saved, measure the firmware
  SHA256_CTX ctx;
  uint8_t firmware_measurement[32];
  sha256_init(&ctx);
  sha256_update(&ctx, (uint8_t *)APP_BASE, APP_MEASURE_LEN);
  sha256_final(&ctx, firmware_measurement);
  UART_Send_String(USART2, "Firmware Measurement: ");
  UART_Send_Hex(USART2, firmware_measurement, 32);

  // hkdf extract
  uint8_t prk[32] = {0};
  uint8_t zero_salt[32] = {0};
  hkdf_extract(prk, zero_salt, 32, uds, sizeof(uds));

  // uds is unnecessary
  clear_key(uds, 32);

  // hkdf expand
  uint8_t cdi[32];
  hkdf_expand(cdi, prk, sizeof(prk), firmware_measurement, 32);

  UART_Send_String(USART2, "CDI: ");
  UART_Send_Hex(USART2, cdi, 32);

  // firmware_measurement and prk is unnecessary
  clear_key(prk, 32);
  clear_key(firmware_measurement, 32);

  // ed25519 keypair gen
  crypto_ed25519_key_pair(secret_key, public_key, cdi);

  // cdi clear key is handled by monocypher's ed25519 key pair function
}

void clear_key(uint8_t *key, size_t key_len) {
  volatile uint8_t *p = key;
  for (size_t i = 0; i < key_len; ++i) {
    p[i] = 0;
  }
}
