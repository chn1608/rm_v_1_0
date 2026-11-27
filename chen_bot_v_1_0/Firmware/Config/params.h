#ifndef CHEN_CONFIG_PARAMS_H
#define CHEN_CONFIG_PARAMS_H
#include <stdbool.h>
#include <stdint.h>

/* Central parameter block.
 *
 * Three classes (see docs/ARCHITECTURE.md):
 *   PARAM_RUNTIME  may be changed while running
 *   PARAM_SAFETY   only with motion stopped and local maintenance authority
 *   PARAM_CAL      calibration values, require a stopped, unpowered machine
 *
 * Values are integers with explicit units so no float ABI crosses the protocol.
 */
typedef enum {
    PARAM_TELEMETRY_MS = 0,
    PARAM_MOTOR_LIMIT,
    PARAM_MOTOR_RAMP,
    PARAM_REVERSAL_TICKS,
    PARAM_SERVO_CENTER_US,
    PARAM_SERVO_MIN_US,
    PARAM_SERVO_MAX_US,
    PARAM_SERVO_SLEW_US,
    PARAM_VBAT_MIN_MV,
    PARAM_VBAT_MAX_MV,
    PARAM_MOTION_TIMEOUT_MS,
    PARAM_LINK_TIMEOUT_MS,
    PARAM_ARM_REQUIRE_LOCAL,
    PARAM_COUNT
} ParamId;

typedef enum { PARAM_RUNTIME, PARAM_SAFETY, PARAM_CAL } ParamClass;

typedef struct {
    const char *name;
    int32_t minimum;
    int32_t maximum;
    int32_t fallback;
    ParamClass klass;
    const char *unit;
} ParamInfo;

typedef struct {
    int32_t value[PARAM_COUNT];
    uint32_t revision;   /* incremented on every accepted write */
    uint32_t crc;        /* over the value array, for NVM integrity */
} Params;

/* Parameter metadata, for the debug service and the PC tool. */
const ParamInfo *params_info(ParamId id);

void params_defaults(Params *params);
bool params_range_ok(ParamId id, int32_t value);
bool params_get(const Params *params, ParamId id, int32_t *value);
/* Returns false on unknown id, out-of-range value or a class that requires the
 * machine to be stopped (caller passes `stopped`). */
bool params_set(Params *params, ParamId id, int32_t value, bool stopped);
uint32_t params_block_crc(const Params *params);
bool params_self_check(const Params *params);
void params_reset(Params *params);

#endif
