#ifndef __HKDF_H
#define __HKDF_H

#include "hmac.h"
#include <stddef.h>
#include <stdint.h>

#define HKDF_MAX_INFO_LEN 32

void hkdf_extract(uint8_t prk[32], const uint8_t *salt, size_t salt_len,
                  const uint8_t *ikm, size_t ikm_len);

void hkdf_expand(uint8_t okm[32], const uint8_t *prk, size_t prk_len,
                 const uint8_t *info, size_t info_len);

#endif