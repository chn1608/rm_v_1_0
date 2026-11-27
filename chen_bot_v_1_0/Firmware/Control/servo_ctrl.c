#include "servo_ctrl.h"
#include <stddef.h>

bool servo_cal_valid(const ServoCal *cal)
{
    if (cal == NULL) return false;
    if (cal->min_mdeg >= cal->center_mdeg || cal->center_mdeg >= cal->max_mdeg) return false;
    if (cal->min_mdeg < -180000 || cal->max_mdeg > 180000) return false;
    if (cal->min_us < 400u || cal->max_us > 2600u) return false;
    if (cal->min_us >= cal->center_us || cal->center_us >= cal->max_us) return false;
    return true;
}

/* Piecewise linear angle -> pulse, exact through the three calibration points. */
static uint16_t angle_to_pulse(const ServoCal *cal, int32_t angle)
{
    const bool lower = angle < cal->center_mdeg;
    const int32_t from_angle = lower ? cal->min_mdeg : cal->center_mdeg;
    const int32_t to_angle = lower ? cal->center_mdeg : cal->max_mdeg;
    const int32_t from_us = lower ? cal->min_us : cal->center_us;
    const int32_t to_us = lower ? cal->center_us : cal->max_us;
    const int32_t span = to_angle - from_angle;
    const int32_t delta = (angle - from_angle) * (to_us - from_us);
    return (uint16_t)(from_us + (delta + span / 2) / span);
}

bool servo_init(ServoCtrl *s, const ServoCal *cal, uint16_t slew_us)
{
    if (s == NULL || !servo_cal_valid(cal) || slew_us == 0u) {
        if (s != NULL) {
            s->valid = false;
            s->output_us = 0u;
            s->target_us = 0u;
            s->command_mdeg = 0;
            s->slew_us = 0u;
        }
        return false;
    }
    s->cal = *cal;
    s->slew_us = slew_us;
    s->command_mdeg = cal->center_mdeg;
    s->target_us = cal->center_us;
    s->output_us = cal->center_us;
    s->valid = true;
    return true;
}

bool servo_target(ServoCtrl *s, int32_t target_mdeg)
{
    if (s == NULL || !s->valid) return false;
    if (target_mdeg < s->cal.min_mdeg) target_mdeg = s->cal.min_mdeg;
    if (target_mdeg > s->cal.max_mdeg) target_mdeg = s->cal.max_mdeg;
    s->command_mdeg = target_mdeg;
    s->target_us = angle_to_pulse(&s->cal, target_mdeg);
    return true;
}

void servo_step(ServoCtrl *s)
{
    if (s == NULL || !s->valid) return;
    if (s->output_us == s->target_us) return;
    const int32_t delta = (int32_t)s->target_us - (int32_t)s->output_us;
    if (delta > (int32_t)s->slew_us) s->output_us = (uint16_t)(s->output_us + s->slew_us);
    else if (delta < -(int32_t)s->slew_us) s->output_us = (uint16_t)(s->output_us - s->slew_us);
    else s->output_us = s->target_us;
}

uint16_t servo_output_us(const ServoCtrl *s)
{
    return (s != NULL && s->valid) ? s->output_us : 0u;
}

int32_t servo_command_mdeg(const ServoCtrl *s)
{
    return (s != NULL && s->valid) ? s->command_mdeg : 0;
}
