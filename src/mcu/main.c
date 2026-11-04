// Arthur Cary
// main.c
// Lab 5: Quadrature encoder velocity measurement

#include "main.h"
#include "interrupt.h"

typedef struct {
    int32_t  position;
    uint32_t time;
} SpeedRef;

int _write(int file, char *ptr, int len) {
    (void)file;
    for (int i = 0; i < len; i++) {
        ITM_SendChar(ptr[i]);
    }
    return len;
}

static float rpsFromWindow(int32_t counts, uint32_t ticks) {
    return (float)counts * TICK_HZ / ((float)COUNTS_PER_REV * (float)ticks);
}

static float rpsFromPeriod(int8_t dir, uint32_t ticks) {
    return (float)dir * TICK_HZ / ((float)COUNTS_PER_REV * (float)ticks);
}

// 0 when idle
static float estimateSpeed(SpeedRef *ref, const EncoderSample *s, uint32_t now) {
    int32_t  counts = s->position - ref->position;
    uint32_t window = now - ref->time;
    uint32_t idle   = now - s->edgeTime;

    ref->position = s->position;
    ref->time     = now;

    int32_t magnitude = (counts < 0) ? -counts : counts;
    if (magnitude >= MIN_WINDOW_COUNTS) {
        return rpsFromWindow(counts, window);
    }
    if (s->edgePeriod == 0 || idle > STOP_TIMEOUT_TICKS) {
        return 0.0f;
    }
    return rpsFromPeriod(s->dir, (idle > s->edgePeriod) ? idle : s->edgePeriod);
}

static void printSpeed(float rps) {
    if (rps == 0.0f) {
        printf("STOPPED  0.000 rev/s\n");
    } else if (rps > 0.0f) {
        printf("%s  %.3f rev/s\n", LABEL_POS, rps);
    } else {
        printf("%s  %.3f rev/s\n", LABEL_NEG, -rps);
    }
}

int main(void) {
    printf("Before: SystemCoreClock = %lu Hz\n", (unsigned long)SystemCoreClock);

    configureFlash();
    configureClock();
    SystemCoreClockUpdate();

    printf("After:  SystemCoreClock = %lu Hz\n", (unsigned long)SystemCoreClock);

    encoderInit();

    EncoderSample sample;
    encoderRead(&sample);

    SpeedRef ref = { sample.position, ENC_TIMER->CNT };
    uint32_t lastUpdate = ref.time;

    while (1) {
        if ((ENC_TIMER->CNT - lastUpdate) < UPDATE_TICKS) {
            continue;
        }

        encoderRead(&sample);
        uint32_t now = ENC_TIMER->CNT;  // read after the sample
        lastUpdate = now;

        printSpeed(estimateSpeed(&ref, &sample, now));
    }
}
