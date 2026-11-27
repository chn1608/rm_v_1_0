#include "chassis.h"
#include <stddef.h>

static int sign_of(int32_t value)
{
    return (value > 0) - (value < 0);
}

static int16_t approach(int16_t value, int16_t target, uint16_t step)
{
    const int32_t delta = (int32_t)target - value;
    if (delta > (int32_t)step) return (int16_t)(value + (int16_t)step);
    if (delta < -(int32_t)step) return (int16_t)(value - (int16_t)step);
    return target;
}

static bool ramp_valid(int16_t limit, uint16_t step, uint16_t reversal)
{
    return limit >= 100 && limit <= 1000 && step > 0u && step <= 1000u && reversal <= 1000u;
}

void chassis_init(Chassis *c, int16_t limit, uint16_t ramp_step, uint16_t reversal_ticks)
{
    if (c == NULL) return;
    c->wheel[0] = (WheelRamp){0};
    c->wheel[1] = (WheelRamp){0};
    c->limit = limit;
    c->ramp_step = ramp_step;
    c->reversal_ticks = reversal_ticks;
}

static void wheel_stop(WheelRamp *w)
{
    *w = (WheelRamp){0};
}

/* Ramp one wheel towards `target`. Direction changes always pass through zero
 * and rest there for `reversal_ticks`, which protects the H bridge and the
 * gearbox from a plugging reversal. */
static void wheel_update(WheelRamp *w, int16_t target, uint16_t step, uint16_t reversal_ticks)
{
    const int direction = sign_of(target);
    const bool reversing = direction != 0 && w->last_direction != 0 &&
                           direction != w->last_direction;
    if (reversing && (w->value != 0 || w->zero_ticks < reversal_ticks)) target = 0;

    const bool was_zero = w->value == 0;
    w->value = approach(w->value, target, step);
    if (w->value != 0) {
        w->last_direction = (int8_t)sign_of(w->value);
        w->zero_ticks = 0u;
    } else if (was_zero && w->zero_ticks < UINT16_MAX) {
        ++w->zero_ticks;
    }
}

bool chassis_update(Chassis *c, int16_t throttle, int16_t steer, bool permit)
{
    if (c == NULL) return false;
    const bool valid = ramp_valid(c->limit, c->ramp_step, c->reversal_ticks);
    if (!permit || !valid) {
        wheel_stop(&c->wheel[0]);
        wheel_stop(&c->wheel[1]);
        return valid;
    }

    if (throttle > 1000) throttle = 1000;
    if (throttle < -1000) throttle = -1000;
    if (steer > 1000) steer = 1000;
    if (steer < -1000) steer = -1000;

    /* steer > 0 turns right: left wheel keeps more throttle. */
    int32_t left = (int32_t)throttle + steer;
    int32_t right = (int32_t)throttle - steer;

    /* Scale the pair instead of clipping each wheel so a full-throttle turn
     * keeps its curvature rather than turning into a straight line. */
    int32_t peak = left < 0 ? -left : left;
    const int32_t right_peak = right < 0 ? -right : right;
    if (right_peak > peak) peak = right_peak;
    if (peak > (int32_t)c->limit && peak > 0) {
        left = left * (int32_t)c->limit / peak;
        right = right * (int32_t)c->limit / peak;
    }

    wheel_update(&c->wheel[0], (int16_t)left, c->ramp_step, c->reversal_ticks);
    wheel_update(&c->wheel[1], (int16_t)right, c->ramp_step, c->reversal_ticks);
    return true;
}

int16_t chassis_wheel(const Chassis *c, unsigned index)
{
    if (c == NULL || index > 1u) return 0;
    return c->wheel[index].value;
}

bool chassis_set_wheels(Chassis *c, int16_t left, int16_t right, bool permit)
{
    if (c == NULL) return false;
    const bool valid = ramp_valid(c->limit, c->ramp_step, c->reversal_ticks);
    if (!permit || !valid) {
        wheel_stop(&c->wheel[0]);
        wheel_stop(&c->wheel[1]);
        return valid;
    }
    const int32_t limit = c->limit;
    int32_t l = left;
    int32_t r = right;
    if (l > limit) l = limit;
    if (l < -limit) l = -limit;
    if (r > limit) r = limit;
    if (r < -limit) r = -limit;
    wheel_update(&c->wheel[0], (int16_t)l, c->ramp_step, c->reversal_ticks);
    wheel_update(&c->wheel[1], (int16_t)r, c->ramp_step, c->reversal_ticks);
    return true;
}
