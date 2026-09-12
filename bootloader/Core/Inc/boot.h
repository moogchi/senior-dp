#ifndef __BOOT_H
#define __BOOT_H

#include <stdint.h>
#include <stm32f4xx.h>

#define APP_BASE 0x08004000U // where firmware starts

void jump_to_app(uint32_t app_base);
#endif