#include "blake2.h"
#include "measure.h"

void measure(uint8_t out[MEASURE_DIGEST_SIZE], const uint8_t *data,
             size_t len) {
  blake2s_state s;
  blake2s_init(&s, 32);
  blake2s_update(&s, data, len);
  blake2s_final(&s, out, MEASURE_DIGEST_SIZE);
}

void derive(uint8_t out[MEASURE_DIGEST_SIZE], const uint8_t *key,
            size_t key_len, const uint8_t *info, size_t info_len) {
  blake2s_state s;
  blake2s_init_key(&s, MEASURE_DIGEST_SIZE, key, key_len);
  blake2s_update(&s, info, info_len);
  blake2s_final(&s, out, MEASURE_DIGEST_SIZE);
}