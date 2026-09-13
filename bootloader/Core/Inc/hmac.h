#ifndef __HMAC_H
#define __HMAC_H

#include "sha256.h"
#include <stddef.h>
#include <stdint.h>

#define HMAC_BLOCK_SIZE 64    // need to extend the keys to the right size
#define SHA256_DIGEST_SIZE 32 // sha 256 output size
#define IPAD 0x36
#define OPAD 0x5c

void hmac_sha256(uint8_t out[32], const uint8_t *key, size_t key_len,
                 const uint8_t *msg, size_t msg_len);

#endif