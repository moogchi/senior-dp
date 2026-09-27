#define OTP_BASE 0x1FFF7800

#include "boot.h"
#include "hkdf.h"
#include "monocypher-ed25519.h"
#include "sha256.h"
#include "stm32f4xx_ll_bus.h"
#include "transmit.h"
#include <stdint.h>

void derive_alias_key(uint8_t *secret_key, uint8_t *public_key);

void clear_key(uint8_t *key, size_t key_len);
