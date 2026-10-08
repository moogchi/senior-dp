#include "utils.h"

void clear_key(uint8_t *key, size_t key_len) {
  volatile uint8_t *p = key;
  for (size_t i = 0; i < key_len; ++i) {
    p[i] = 0;
  }
}
