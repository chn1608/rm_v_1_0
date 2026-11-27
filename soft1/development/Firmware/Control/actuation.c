#include "actuation.h"

static int direction(int value) { return (value > 0) - (value < 0); }

static int16_t approach(int16_t value, int16_t target, uint16_t step)
{
    const int32_t delta = (int32_t)target - value;
    if (delta > step) return (int16_t)(value + step);
    if (delta < -(int32_t)step) return (int16_t)(value - step);
    return target;
}

bool wheel_update(WheelRamp *wheel, int16_t target, uint16_t step,
                  uint16_t reversal_ticks, bool permit)
{
    const bool valid = step > 0u && step <= 1000u && reversal_ticks > 0u;
    if (!permit || !valid) {
        *wheel = (WheelRamp){0};
        return valid;
    }
    if (target > 1000) target = 1000;
    if (target < -1000) target = -1000;
    const int sign = direction(target);
    const bool reverse = sign != 0 && wheel->last_direction != 0 && sign != wheel->last_direction;
    if (reverse && (wheel->value != 0 || wheel->zero_ticks < reversal_ticks)) target = 0;
    const bool was_zero = wheel->value == 0;
    wheel->value = approach(wheel->value, target, step);
    if (wheel->value != 0) {
        wheel->last_direction = (int8_t)direction(wheel->value);
        wheel->zero_ticks = 0u;
    } else if (was_zero && wheel->zero_ticks < UINT16_MAX) {
        ++wheel->zero_ticks;
    }
    return true;
}

bool servo_pulse(const ServoCalibration *cfg, int32_t target, uint16_t *out)
{
    if (cfg->min_mdeg < -360000 || cfg->max_mdeg > 360000 ||
        cfg->min_mdeg >= cfg->center_mdeg || cfg->center_mdeg >= cfg->max_mdeg ||
        cfg->min_us < 500u || cfg->max_us > 2500u ||
        cfg->min_us >= cfg->center_us || cfg->center_us >= cfg->max_us) return false;
    if (target < cfg->min_mdeg) target = cfg->min_mdeg;
    if (target > cfg->max_mdeg) target = cfg->max_mdeg;
    const bool lower = target < cfg->center_mdeg;
    const int32_t from_angle = lower ? cfg->min_mdeg : cfg->center_mdeg;
    const int32_t to_angle = lower ? cfg->center_mdeg : cfg->max_mdeg;
    const int32_t from_pulse = lower ? cfg->min_us : cfg->center_us;
    const int32_t to_pulse = lower ? cfg->center_us : cfg->max_us;
    const int32_t span = to_angle - from_angle;
    const int32_t delta = (target - from_angle) * (to_pulse - from_pulse);
    *out = (uint16_t)(from_pulse + (delta + span / 2) / span);
    return true;
}
