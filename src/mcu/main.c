// main.c
// Lab 5: measures motor speed and direction from a quadrature encoder using interrupts.

#include <stdio.h>
#include <stdint.h>
#include "stm32l432xx.h"

#define ENC_A_PIN        9      // PA9  (5V tolerant)
#define ENC_B_PIN        10     // PA10 (5V tolerant)
#define PPR              408
#define COUNTS_PER_REV   (4 * PPR)
#define SYSCLK_HZ        4000000UL
#define TIMER_FREQ_HZ    1000000UL
#define UPDATE_US        500000UL
#define STOP_TIMEOUT_US  2000000UL

static volatile int32_t  encCount = 0;
static volatile uint32_t lastEdgeTime = 0;
static volatile uint8_t  encState = 0;

// index = (oldState << 2) | newState, state = (A << 1) | B
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
  uint8_t a = (idr >> ENC_A_PIN) & 1;
  uint8_t b = (idr >> ENC_B_PIN) & 1;
  return (a << 1) | b;
}

static void encoderUpdate(void) {
  uint8_t newState = readEncoderState();
  int8_t step = QUAD_TABLE[(encState << 2) | newState];

  if (step != 0) {
    encCount += step;
    lastEdgeTime = TIM2->CNT;
  }
  encState = newState;
}

void EXTI9_5_IRQHandler(void) {
  if (EXTI->PR1 & (1 << ENC_A_PIN)) {
    EXTI->PR1 = (1 << ENC_A_PIN);
    encoderUpdate();
  }
}

void EXTI15_10_IRQHandler(void) {
  if (EXTI->PR1 & (1 << ENC_B_PIN)) {
    EXTI->PR1 = (1 << ENC_B_PIN);
    encoderUpdate();
  }
}

static void initGPIO(void) {
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
  GPIOA->MODER &= ~(GPIO_MODER_MODE9 | GPIO_MODER_MODE10);
  GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD9 | GPIO_PUPDR_PUPD10);
}

static void initTimer(void) {
  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
  TIM2->PSC = (SYSCLK_HZ / TIMER_FREQ_HZ) - 1;
  TIM2->ARR = 0xFFFFFFFF;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->CR1 |= TIM_CR1_CEN;
}

static void initEXTI(void) {
  uint32_t pins = (1 << ENC_A_PIN) | (1 << ENC_B_PIN);

  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
  SYSCFG->EXTICR[2] &= ~(SYSCFG_EXTICR3_EXTI9 | SYSCFG_EXTICR3_EXTI10);

  encState = readEncoderState();

  EXTI->RTSR1 |= pins;
  EXTI->FTSR1 |= pins;
  EXTI->PR1 = pins;
  EXTI->IMR1 |= pins;

  NVIC_EnableIRQ(EXTI9_5_IRQn);
  NVIC_EnableIRQ(EXTI15_10_IRQn);
}

static void printVelocity(float velocity) {
  int32_t milliRev = (int32_t)(velocity * 1000.0f + (velocity >= 0.0f ? 0.5f : -0.5f));
  const char *dir = "STOPPED";

  if (milliRev > 0) {
    dir = "CW";
  } else if (milliRev < 0) {
    dir = "CCW";
    milliRev = -milliRev;
  }
  printf("Speed: %ld.%03ld rev/s  Direction: %s\n", milliRev / 1000, milliRev % 1000, dir);
}

int main(void) {
  int32_t prevCount = 0;
  uint32_t prevEdgeTime = 0;
  uint32_t lastUpdate = 0;
  uint8_t stopped = 1;
  float velocity = 0.0f;

  initGPIO();
  initTimer();
  initEXTI();

  lastUpdate = TIM2->CNT;
  prevEdgeTime = lastUpdate;

  while (1) {
    if ((uint32_t)(TIM2->CNT - lastUpdate) >= UPDATE_US) {
      lastUpdate += UPDATE_US;

      __disable_irq();
      int32_t count = encCount;
      uint32_t edgeTime = lastEdgeTime;
      __enable_irq();

      uint32_t now = TIM2->CNT;
      int32_t deltaCount = count - prevCount;

      if (deltaCount != 0) {
        uint32_t dt = edgeTime - prevEdgeTime;
        if (!stopped && dt > 0) {
          velocity = ((float)deltaCount * TIMER_FREQ_HZ) / ((float)COUNTS_PER_REV * dt);
        }
        stopped = 0;
        prevCount = count;
        prevEdgeTime = edgeTime;
      } else if ((uint32_t)(now - prevEdgeTime) > STOP_TIMEOUT_US) {
        velocity = 0.0f;
        stopped = 1;
      }

      printVelocity(velocity);
    }
  }
}
