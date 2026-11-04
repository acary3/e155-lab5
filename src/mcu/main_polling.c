// Arthur Cary
// pollingtest.c
// Lab 5: Polling version of the encoder decoder, for comparison with interrupts.

#include "main.h"
#include "interrupt.h"

#define SHOW_DIAGNOSTICS  1
#define EXTRA_WORK_US     0
#define MISSED_STEP       2

// same as interrupt.c, but double jumps are marked so they can be counted
static const int8_t STEP_LUT[16] = {
     0,  1, -1,  2,
    -1,  0,  2,  1,
     1,  2,  0, -1,
     2, -1,  1,  0
};

typedef struct {
    int32_t  position;
    uint32_t time;
} SpeedRef;

static uint8_t readPins(void) {
    uint32_t idr = GPIOA->IDR;
    return (uint8_t)((((idr >> gpioPinOffset(ENC_A_PIN)) & 1u) << 1) |
                      ((idr >> gpioPinOffset(ENC_B_PIN)) & 1u));
}

static void cycleCounterInit(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void busyWait(uint32_t cycles) {
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles) {
    }
}

// identical to estimateSpeed() in main.c
static float estimateSpeed(SpeedRef *ref, const EncoderSample *s, uint32_t now) {
    int32_t  counts = s->position - ref->position;
    uint32_t window = now - ref->time;
    uint32_t idle   = now - s->edgeTime;

    ref->position = s->position;
    ref->time     = now;

    int32_t magnitude = (counts < 0) ? -counts : counts;
    if (magnitude >= MIN_WINDOW_COUNTS) {
        return (float)counts * TICK_HZ / ((float)COUNTS_PER_REV * (float)window);
    }
    if (s->edgePeriod == 0 || idle > STOP_TIMEOUT_TICKS) {
        return 0.0f;
    }
    uint32_t period = (idle > s->edgePeriod) ? idle : s->edgePeriod;
    return (float)s->dir * TICK_HZ / ((float)COUNTS_PER_REV * (float)period);
}

int main(void) {
    configureFlash();
    configureClock();
    SystemCoreClockUpdate();

    timestampInit();
    cycleCounterInit();

    gpioEnable(GPIO_PORT_A);
    pinMode(ENC_A_PIN, GPIO_INPUT);
    pinMode(ENC_B_PIN, GPIO_INPUT);

    const uint32_t cyclesPerUs = SystemCoreClock / 1000000u;
    const uint32_t extraCycles = EXTRA_WORK_US * cyclesPerUs;

    printf("POLLING TEST: %lu Hz clock, %d us extra work per loop\n",
           (unsigned long)SystemCoreClock, EXTRA_WORK_US);

    EncoderSample enc = {0};
    enc.edgeTime = ENC_TIMER->CNT;
    uint8_t lastState = readPins();

    uint32_t missed     = 0;
    uint32_t maxGap     = 0;
    uint32_t lastSample = DWT->CYCCNT;

    SpeedRef ref = { 0, ENC_TIMER->CNT };
    uint32_t lastUpdate = ref.time;

    while (1) {
        uint32_t cyc = DWT->CYCCNT;
        if (cyc - lastSample > maxGap) {
            maxGap = cyc - lastSample;
        }
        lastSample = cyc;

        uint8_t state = readPins();
        int8_t  step  = STEP_LUT[(lastState << 2) | state];
        lastState = state;

        if (step == MISSED_STEP) {
            missed++;
        } else if (step != 0) {
            uint32_t t = ENC_TIMER->CNT;
            enc.position  += step;
            enc.dir        = step;
            enc.edgePeriod = t - enc.edgeTime;
            enc.edgeTime   = t;
        }

        if (extraCycles > 0) {
            busyWait(extraCycles);
        }

        if ((ENC_TIMER->CNT - lastUpdate) < UPDATE_TICKS) {
            continue;
        }

        uint32_t now = ENC_TIMER->CNT;
        lastUpdate = now;
        float rps = estimateSpeed(&ref, &enc, now);

        if (rps == 0.0f) {
            printf("STOPPED  0.000 rev/s");
        } else {
            printf("%s  %.3f rev/s", (rps > 0.0f) ? LABEL_POS : LABEL_NEG,
                   (rps > 0.0f) ? rps : -rps);
        }
#if SHOW_DIAGNOSTICS
        float maxRps = (maxGap > 0)
            ? (float)SystemCoreClock / ((float)COUNTS_PER_REV * (float)maxGap)
            : 0.0f;
        printf("   [missed %lu, gap %lu us, max %.2f rev/s]",
               (unsigned long)missed, (unsigned long)(maxGap / cyclesPerUs), maxRps);
#endif
        printf("\n");

        missed = 0;
        maxGap = 0;
    }
}
