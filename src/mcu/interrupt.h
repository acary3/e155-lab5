// Arthur Cary
// interrupt.h
// Lab 5: Encoder edge interrupts on PA9 (A) and PA8 (B)

#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "main.h"

#define ENC_A_PIN   PA9
#define ENC_B_PIN   PA8
#define ENC_TIMER   TIM2

typedef struct {
    int32_t  position;
    uint32_t edgeTime;
    uint32_t edgePeriod;
    int8_t   dir;
} EncoderSample;

void timestampInit(void);
void encoderInit(void);
void encoderRead(EncoderSample *out);

#endif
