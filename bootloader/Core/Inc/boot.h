#ifndef __BOOT_H
#define __BOOT_H

#include <stdint.h>
#include <stm32f4xx.h>

#define APP_BASE 0x08008000U // where firmware starts
#define APP_MEASURE_LEN 491520

void jump_to_app(uint32_t app_base);
#endif