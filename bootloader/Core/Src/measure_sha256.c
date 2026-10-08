#include "hkdf.h"
#include "measure.h"
#include "sha256.h"
#include "utils.h"

void measure(uint8_t out[MEASURE_DIGEST_SIZE], const uint8_t *data,
             size_t len) {
  SHA256_CTX ctx;
  sha256_init(&ctx);
  sha256_update(&ctx, data, len);
  sha256_final(&ctx, out);
}

void derive(uint8_t out[MEASURE_DIGEST_SIZE], const uint8_t *key,
            size_t key_len, const uint8_t *info, size_t info_len) {
  uint8_t zero_salt[32] = {0};
  uint8_t prk[32];

  hkdf_extract(prk, zero_salt, sizeof(zero_salt), key, key_len);
  hkdf_expand(out, prk, sizeof(prk), info, info_len);

  clear_key(prk, sizeof(prk));
}