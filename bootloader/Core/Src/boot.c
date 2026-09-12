#include "boot.h"

void jump_to_app(uint32_t app_base) {
  uint32_t sp =
      *(volatile uint32_t *)app_base; // vector[0] = initial stack pointer
  uint32_t reset =
      *(volatile uint32_t *)(app_base + 4); // vector[1] = reset handler

  __disable_irq(); // disable interrupt as we are modify where the interrupts
  // dispatch and stack lives

  SCB->VTOR = app_base; // scb stores where the boot vector lives

  __set_MSP(sp); // set new stack pointer

  __enable_irq(); // enable interrupt back

  ((void (*)(void))reset)(); // reset
}