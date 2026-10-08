#ifndef __MEASURE_H
#define __MEASURE_H

#include <stddef.h>
#include <stdint.h>

#define MEASURE_DIGEST_SIZE 32

// hash a region of memory
void measure(uint8_t out[MEASURE_DIGEST_SIZE], const uint8_t *data, size_t len);

// derive key material from a secret and a context value
void derive(uint8_t out[MEASURE_DIGEST_SIZE], const uint8_t *key,
            size_t key_len, const uint8_t *info, size_t info_len);

#endif