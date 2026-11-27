#ifndef CHEN_CONTROL_PID_H
#define CHEN_CONTROL_PID_H
#include <stdbool.h>
#include <stdint.h>

/* Integer PID reserved for the future encoder speed loop.
 *
 * Gains are scaled by 1000 so they can be stored in the parameter block:
 *   output = (kp * error + ki * integral + kd * derivative) / 1000
 * The integral term is clamped to the output range (anti wind-up) and the
 * derivative is taken on the measurement to avoid a setpoint kick.
 */
typedef struct {
    int32_t kp, ki, kd;
    int32_t integral;
    int32_t previous_measurement;
    int32_t out_min, out_max;
    bool primed;
} Pid;

void pid_reset(Pid *pid);
bool pid_configure(Pid *pid, int32_t kp, int32_t ki, int32_t kd,
                   int32_t out_min, int32_t out_max);
int32_t pid_update(Pid *pid, int32_t setpoint, int32_t measurement, uint32_t dt_ms);

#endif
