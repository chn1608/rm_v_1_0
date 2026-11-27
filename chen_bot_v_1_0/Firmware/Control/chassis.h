#ifndef CHEN_CONTROL_CHASSIS_H
#define CHEN_CONTROL_CHASSIS_H
#include <stdbool.h>
#include <stdint.h>

/* Differential chassis shaping: throttle/steer mixing, per-wheel ramp and a
 * direction reversal guard. Pure arithmetic, no HAL, host tested.
 *
 * Units: all commands are permille of full scale (-1000..1000).
 */
typedef struct {
    int16_t value;
    int8_t last_direction;
    uint16_t zero_ticks;
} WheelRamp;

typedef struct {
    WheelRamp wheel[2];       /* 0 = left, 1 = right */
    int16_t limit;            /* symmetric magnitude limit, 100..1000 */
    uint16_t ramp_step;       /* permille per control tick */
    uint16_t reversal_ticks;  /* ticks held at zero before a direction change */
} Chassis;

void chassis_init(Chassis *c, int16_t limit, uint16_t ramp_step, uint16_t reversal_ticks);
/* Returns false when the configuration is invalid; outputs are forced to zero. */
bool chassis_update(Chassis *c, int16_t throttle, int16_t steer, bool permit);
/* Direct per-wheel targets, used when the command already carries left/right
 * differential values (BLE/PC). Same ramp and reversal guard as chassis_update. */
bool chassis_set_wheels(Chassis *c, int16_t left, int16_t right, bool permit);
int16_t chassis_wheel(const Chassis *c, unsigned index);

#endif
