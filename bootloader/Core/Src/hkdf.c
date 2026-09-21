#include "hkdf.h"
#include "hmac.h"

static void bytecpy(uint8_t *dest, const uint8_t *src, size_t n);

void hkdf_extract(uint8_t prk[32], const uint8_t *salt, size_t salt_len,
                  const uint8_t *ikm, size_t ikm_len) {
  hmac_sha256(prk, salt, salt_len, ikm, ikm_len);
}

void hkdf_expand(uint8_t okm[32], const uint8_t *prk, size_t prk_len,
                 const uint8_t *info, size_t info_len) {
  uint8_t counter = 0x01;
  uint8_t
      buf[HKDF_MAX_INFO_LEN + 1]; // gonna add counter to info so one more byte
  bytecpy(buf, info, info_len);
  buf[info_len] = counter;

  hmac_sha256(okm, prk, prk_len, buf, info_len + 1);
}

static void bytecpy(uint8_t *dest, const uint8_t *src, size_t n) {
  for (int i = 0; i < n; ++i) {
    dest[i] = src[i];
  }
}