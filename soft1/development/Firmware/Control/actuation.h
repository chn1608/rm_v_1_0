#ifndef RM_ACTUATION_H
#define RM_ACTUATION_H
#include <stdbool.h>
#include <stdint.h>

/* Pure control math. Hardware-adapter enable and shutdown remain independent. */
typedef struct {
    int16_t value;
    int8_t last_direction;
    uint16_t zero_ticks;
} WheelRamp;

typedef struct {
    int32_t min_mdeg, center_mdeg, max_mdeg;
    uint16_t min_us, center_us, max_us;
} ServoCalibration;

bool wheel_update(WheelRamp *wheel, int16_t target, uint16_t step,
                  uint16_t reversal_ticks, bool permit);
bool servo_pulse(const ServoCalibration *cfg, int32_t target_mdeg, uint16_t *out);
#endif
