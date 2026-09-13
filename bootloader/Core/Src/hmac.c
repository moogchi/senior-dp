#include "hmac.h"

/*
HMAC-SHA256 (RFC 2104 / FIPS 198-1)
Main Idea: proves that this output could only be computed by someone holding the
key(uds).

Math:
HMAC(key,msg) = H((Key ⊕ opad ) || H((key ⊕ ipad ) || msg) )

ipad = 0x36 repeated to blocksize(64)
opad = 0x5c repeated to blocksize(64)
key = initialized to empty array with size of blocksize (64)

*/

static void bytecpy(uint8_t *dest, const uint8_t *src, size_t n);

void hmac_sha256(uint8_t out[32], const uint8_t *key, size_t key_len,
                 const uint8_t *msg, size_t msg_len) {
  uint8_t key_block[HMAC_BLOCK_SIZE] = {0};

  bytecpy(key_block, key, key_len); // currently under the assumption that
  // key_len is less than or equal to 64

  uint8_t ipad_key[HMAC_BLOCK_SIZE], opad_key[HMAC_BLOCK_SIZE];

  for (int i = 0; i < HMAC_BLOCK_SIZE; ++i) {
    ipad_key[i] = key_block[i] ^ IPAD;
    opad_key[i] = key_block[i] ^ OPAD;
  } // generates opad and ipad keys

  // inner hashing
  SHA256_CTX ctx;
  uint8_t inner_hash[SHA256_DIGEST_SIZE];
  sha256_init(&ctx);
  sha256_update(&ctx, ipad_key, HMAC_BLOCK_SIZE);
  sha256_update(&ctx, msg, msg_len);
  sha256_final(&ctx, inner_hash);

  // outer hashing
  sha256_init(&ctx);
  sha256_update(&ctx, opad_key, HMAC_BLOCK_SIZE);
  sha256_update(&ctx, inner_hash, SHA256_DIGEST_SIZE);
  sha256_final(&ctx, out);
}

static void bytecpy(uint8_t *dest, const uint8_t *src, size_t n) {
  for (int i = 0; i < n; ++i) {
    dest[i] = src[i];
  }
}