#include "keygen.h"

void derive_alias_key(uint8_t *secret_key, uint8_t *public_key) {
  // disable dma
  LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_DMA1);
  LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_DMA2);

  // read uds from otp
  const uint8_t *otp = (const uint8_t *)OTP_BASE;
  uint8_t uds[32] = {0};
  for (int i = 0; i < 32; ++i) {
    uds[i] = otp[i];
  }

  // measure the firmware
  uint8_t firmware_measurement[MEASURE_DIGEST_SIZE];
  measure(firmware_measurement, (const uint8_t *)APP_BASE, APP_MEASURE_LEN);

  // derive cdi
  uint8_t cdi[MEASURE_DIGEST_SIZE];
  derive(cdi, uds, sizeof(uds), firmware_measurement,
         sizeof(firmware_measurement));

  clear_key(uds, sizeof(uds));
  clear_key(firmware_measurement, sizeof(firmware_measurement));

  // ed25519 keypair gen — wipes cdi as a side effect
  crypto_ed25519_key_pair(secret_key, public_key, cdi);
}