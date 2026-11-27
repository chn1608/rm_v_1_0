#ifndef CHEN_CONTROL_SERVO_CTRL_H
#define CHEN_CONTROL_SERVO_CTRL_H
#include <stdbool.h>
#include <stdint.h>

/* Steering servo shaping: angle to pulse mapping plus a slew limit that keeps
 * the mechanical load inside what the LD-220MG and its supply can survive.
 *
 * The output is always a *commanded* pulse. Without position feedback the code
 * must never present it as a measured angle.
 */
typedef struct {
    int32_t min_mdeg;      /* mechanical lower limit, millidegrees */
    int32_t center_mdeg;
    int32_t max_mdeg;
    uint16_t min_us;
    uint16_t center_us;
    uint16_t max_us;
} ServoCal;

typedef struct {
    ServoCal cal;
    uint16_t slew_us;      /* maximum change per step */
    uint16_t target_us;    /* clamped target */
    uint16_t output_us;    /* slew limited output */
    int32_t command_mdeg;  /* last accepted target, clamped */
    bool valid;
} ServoCtrl;

bool servo_init(ServoCtrl *s, const ServoCal *cal, uint16_t slew_us);
/* Accepts a new angle command; returns false (and keeps the old target) when
 * the calibration or the angle is invalid. */
bool servo_target(ServoCtrl *s, int32_t target_mdeg);
/* Advance the slew limiter by one control tick. */
void servo_step(ServoCtrl *s);
uint16_t servo_output_us(const ServoCtrl *s);
int32_t servo_command_mdeg(const ServoCtrl *s);
bool servo_cal_valid(const ServoCal *cal);

#endif
