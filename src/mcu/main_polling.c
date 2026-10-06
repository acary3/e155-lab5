// main_polling.c
// Lab 5: polling version of the encoder reader, used only for comparison with main.c.

#include <stdio.h>
#include <stdint.h>
#include "stm32l432xx.h"

#define ENC_A_PIN       9
#define ENC_B_PIN       10
#define PPR             408
#define COUNTS_PER_REV  (4 * PPR)
#define SYSCLK_HZ       4000000UL
#define TIMER_FREQ_HZ   1000000UL
#define UPDATE_US       500000UL

static const int8_t QUAD_TABLE[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};

int _write(int file, char *ptr, int len) {
  int i = 0;
  for (i = 0; i < len; i++) {
    ITM_SendChar((*ptr++));
  }
  return len;
}

static uint8_t readEncoderState(void) {
  uint32_t idr = GPIOA->IDR;
  return (((idr >> ENC_A_PIN) & 1) << 1) | ((idr >> ENC_B_PIN) & 1);
}

int main(void) {
  int32_t count = 0;
  uint32_t lastUpdate = 0;
  uint8_t state = 0;

  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
  GPIOA->MODER &= ~(GPIO_MODER_MODE9 | GPIO_MODER_MODE10);
  GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD9 | GPIO_PUPDR_PUPD10);

  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
  TIM2->PSC = (SYSCLK_HZ / TIMER_FREQ_HZ) - 1;
  TIM2->ARR = 0xFFFFFFFF;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->CR1 |= TIM_CR1_CEN;

  state = readEncoderState();
  lastUpdate = TIM2->CNT;

  while (1) {
    uint8_t newState = readEncoderState();
    count += QUAD_TABLE[(state << 2) | newState];
    state = newState;

    if ((uint32_t)(TIM2->CNT - lastUpdate) >= UPDATE_US) {
      lastUpdate += UPDATE_US;
      float velocity = ((float)count * TIMER_FREQ_HZ) / ((float)COUNTS_PER_REV * UPDATE_US);
      int32_t milliRev = (int32_t)(velocity * 1000.0f);
      const char *dir = (milliRev > 0) ? "CW" : (milliRev < 0) ? "CCW" : "STOPPED";
      if (milliRev < 0) {
        milliRev = -milliRev;
      }
      printf("Speed: %ld.%03ld rev/s  Direction: %s\n", milliRev / 1000, milliRev % 1000, dir);
      count = 0;
    }
  }
}
