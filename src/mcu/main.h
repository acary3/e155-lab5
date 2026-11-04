// Arthur Cary
// main.h
// Lab 5: Quadrature encoder velocity measurement

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stdio.h>
#include "STM32L432KC.h"

#define TICK_HZ             1000000u
#define UPDATE_TICKS        (1u * TICK_HZ)
#define STOP_TIMEOUT_TICKS  (2u * TICK_HZ)

#define ENCODER_PPR         408
#define EDGES_PER_PULSE     4
#define COUNTS_PER_REV      (ENCODER_PPR * EDGES_PER_PULSE)
#define MIN_WINDOW_COUNTS   50

#define LABEL_POS           "CW"
#define LABEL_NEG           "CCW"

#endif
