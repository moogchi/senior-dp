#ifndef KEYGEN_H
#define KEYGEN_H

#define OTP_BASE 0x1FFF7800

#include "boot.h"
#include "measure.h"
#include "monocypher-ed25519.h"
#include "stm32f4xx_ll_bus.h"
#include "transmit.h"
#include "utils.h"

void derive_alias_key(uint8_t *secret_key, uint8_t *public_key);

#endif