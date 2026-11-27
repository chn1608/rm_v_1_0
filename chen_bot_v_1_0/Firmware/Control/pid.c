#include "pid.h"
#include <stddef.h>

void pid_reset(Pid *pid)
{
    if (pid == NULL) return;
    pid->integral = 0;
    pid->previous_measurement = 0;
    pid->primed = false;
}

bool pid_configure(Pid *pid, int32_t kp, int32_t ki, int32_t kd,
                   int32_t out_min, int32_t out_max)
{
    if (pid == NULL || out_min >= out_max) return false;
    if (kp < 0 || ki < 0 || kd < 0) return false;
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->out_min = out_min;
    pid->out_max = out_max;
    pid_reset(pid);
    return true;
}

static int32_t clamp(int32_t value, int32_t low, int32_t high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

int32_t pid_update(Pid *pid, int32_t setpoint, int32_t measurement, uint32_t dt_ms)
{
    if (pid == NULL || dt_ms == 0u || dt_ms > 1000u) return 0;

    const int32_t error = setpoint - measurement;
    int32_t derivative = 0;
    if (pid->primed) {
        derivative = (measurement - pid->previous_measurement) * 1000 / (int32_t)dt_ms;
    }
    pid->previous_measurement = measurement;
    pid->primed = true;

    const int32_t proportional = pid->kp * error;
    const int32_t derivative_term = pid->kd * derivative;

    /* Integrate first, then clamp the whole output and back-calculate the
     * integral so it cannot wind up beyond the actuator range. */
    pid->integral += pid->ki * error * (int32_t)dt_ms / 1000;
    const int32_t unclamped = (proportional + pid->integral + derivative_term) / 1000;
    const int32_t output = clamp(unclamped, pid->out_min, pid->out_max);
    if (output != unclamped && pid->ki != 0) {
        pid->integral -= (unclamped - output) * 1000;
    }
    return output;
}
