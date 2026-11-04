// Arthur Cary
// interrupt.c
// Lab 5: Encoder edge interrupts on PA9 (A) and PA8 (B)

#include "interrupt.h"

#define ENC_LINES  ((1u << gpioPinOffset(ENC_A_PIN)) | (1u << gpioPinOffset(ENC_B_PIN)))

// index = (old state << 2) | new state
static const int8_t STEP_LUT[16] = {
     0,  1, -1,  0,
    -1,  0,  0,  1,
     1,  0,  0, -1,
     0, -1,  1,  0
};

static volatile EncoderSample enc;
static uint8_t lastState;

static uint8_t readPins(void) {
    uint32_t idr = GPIOA->IDR;
    return (uint8_t)((((idr >> gpioPinOffset(ENC_A_PIN)) & 1u) << 1) |
                      ((idr >> gpioPinOffset(ENC_B_PIN)) & 1u));
}

void timestampInit(void) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
    ENC_TIMER->PSC = (SystemCoreClock / TICK_HZ) - 1;
    ENC_TIMER->ARR = 0xFFFFFFFFu;
    ENC_TIMER->EGR |= TIM_EGR_UG;
    ENC_TIMER->CR1 |= TIM_CR1_CEN;
}

void encoderInit(void) {
    timestampInit();

    gpioEnable(GPIO_PORT_A);
    pinMode(ENC_A_PIN, GPIO_INPUT);
    pinMode(ENC_B_PIN, GPIO_INPUT);

    lastState    = readPins();
    enc.edgeTime = ENC_TIMER->CNT;

    // route EXTI8/EXTI9 to port A
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    SYSCFG->EXTICR[2] &= ~(SYSCFG_EXTICR3_EXTI8 | SYSCFG_EXTICR3_EXTI9);

    EXTI->RTSR1 |= ENC_LINES;
    EXTI->FTSR1 |= ENC_LINES;
    EXTI->IMR1  |= ENC_LINES;

    NVIC_EnableIRQ(EXTI9_5_IRQn);
    __enable_irq();
}

void EXTI9_5_IRQHandler(void) {
    uint32_t flags = EXTI->PR1 & ENC_LINES;
    if (flags == 0) {
        return;
    }
    EXTI->PR1 = flags;

    uint32_t t     = ENC_TIMER->CNT;
    uint8_t  state = readPins();
    int8_t   step  = STEP_LUT[(lastState << 2) | state];
    lastState = state;

    if (step != 0) {
        enc.position  += step;
        enc.dir        = step;
        enc.edgePeriod = t - enc.edgeTime;
        enc.edgeTime   = t;
    }
}

void encoderRead(EncoderSample *out) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    out->position   = enc.position;
    out->edgeTime   = enc.edgeTime;
    out->edgePeriod = enc.edgePeriod;
    out->dir        = enc.dir;
    __set_PRIMASK(primask);
}
