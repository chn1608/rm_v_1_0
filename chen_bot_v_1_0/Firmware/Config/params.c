#include "params.h"
#include "board_config.h"
#include "profile.h"
#include <stddef.h>
#include <string.h>

/* Order must match ParamId. Ranges are engineering limits, not measurements:
 * they bound what the software will accept, the final value still has to come
 * from the bench calibration described in docs/DEBUG_GUIDE.md. */
static const ParamInfo table[PARAM_COUNT] = {
    [PARAM_TELEMETRY_MS] = {"telemetry_ms", 20, 1000, (int32_t)CHEN_TELEMETRY_PERIOD_MS, PARAM_RUNTIME, "ms"},
    [PARAM_MOTOR_LIMIT] = {"motor_limit", 100, BOARD_MOTOR_LIMIT_PERMILLE, BOARD_MOTOR_LIMIT_PERMILLE, PARAM_SAFETY, "permille"},
    [PARAM_MOTOR_RAMP] = {"motor_ramp", 1, 200, BOARD_MOTOR_RAMP_STEP, PARAM_RUNTIME, "permille/tick"},
    [PARAM_REVERSAL_TICKS] = {"reversal_ticks", 0, 200, (int32_t)BOARD_MOTOR_REVERSAL_TICKS, PARAM_SAFETY, "ticks"},
    [PARAM_SERVO_CENTER_US] = {"servo_center_us", 1000, 2000, (int32_t)BOARD_SERVO_CENTER_US, PARAM_CAL, "us"},
    [PARAM_SERVO_MIN_US] = {"servo_min_us", 500, 1500, (int32_t)BOARD_SERVO_MIN_US, PARAM_CAL, "us"},
    [PARAM_SERVO_MAX_US] = {"servo_max_us", 1500, 2500, (int32_t)BOARD_SERVO_MAX_US, PARAM_CAL, "us"},
    [PARAM_SERVO_SLEW_US] = {"servo_slew_us", 1, 50, (int32_t)BOARD_SERVO_SLEW_US_PER_TICK, PARAM_RUNTIME, "us/tick"},
    [PARAM_VBAT_MIN_MV] = {"vbat_min_mv", 10000, 16000, BOARD_VBAT_MIN_MV, PARAM_SAFETY, "mV"},
    [PARAM_VBAT_MAX_MV] = {"vbat_max_mv", 16000, 18000, BOARD_VBAT_MAX_MV, PARAM_SAFETY, "mV"},
    [PARAM_MOTION_TIMEOUT_MS] = {"motion_timeout_ms", 50, 1000, (int32_t)CHEN_MOTION_TIMEOUT_MS, PARAM_SAFETY, "ms"},
    [PARAM_LINK_TIMEOUT_MS] = {"link_timeout_ms", 200, 5000, (int32_t)CHEN_LINK_TIMEOUT_MS, PARAM_SAFETY, "ms"},
    [PARAM_ARM_REQUIRE_LOCAL] = {"arm_require_local", 0, 1, 1, PARAM_SAFETY, ""},
};

const ParamInfo *params_info(ParamId id)
{
    if ((unsigned)id >= (unsigned)PARAM_COUNT) return NULL;
    return &table[id];
}

void params_defaults(Params *params)
{
    memset(params, 0, sizeof(*params));
    for (unsigned i = 0; i < (unsigned)PARAM_COUNT; ++i) params->value[i] = table[i].fallback;
    params->crc = params_block_crc(params);
}

bool params_range_ok(ParamId id, int32_t value)
{
    if ((unsigned)id >= (unsigned)PARAM_COUNT) return false;
    return value >= table[id].minimum && value <= table[id].maximum;
}

bool params_get(const Params *params, ParamId id, int32_t *value)
{
    if (value == NULL || (unsigned)id >= (unsigned)PARAM_COUNT) return false;
    *value = params->value[id];
    return true;
}

bool params_set(Params *params, ParamId id, int32_t value, bool stopped)
{
    if (!params_range_ok(id, value)) return false;
    /* Safety and calibration values must never change while the machine can
     * move: the caller asserts that motion is stopped. */
    if (table[id].klass != PARAM_RUNTIME && !stopped) return false;
    /* Coupled limits are validated as a group. */
    if (id == PARAM_SERVO_CENTER_US) {
        if (value <= params->value[PARAM_SERVO_MIN_US] ||
            value >= params->value[PARAM_SERVO_MAX_US]) return false;
    }
    if (id == PARAM_SERVO_MIN_US) {
        if (value >= params->value[PARAM_SERVO_CENTER_US]) return false;
    }
    if (id == PARAM_SERVO_MAX_US) {
        if (value <= params->value[PARAM_SERVO_CENTER_US]) return false;
    }
    if (id == PARAM_VBAT_MIN_MV && value >= params->value[PARAM_VBAT_MAX_MV]) return false;
    if (id == PARAM_VBAT_MAX_MV && value <= params->value[PARAM_VBAT_MIN_MV]) return false;

    if (params->value[id] == value) return true;
    params->value[id] = value;
    ++params->revision;
    params->crc = params_block_crc(params);
    return true;
}

uint32_t params_block_crc(const Params *params)
{
    /* FNV-1a over the raw value array: cheap, order sensitive and good enough
     * to detect a truncated or corrupted parameter block in NVM. */
    const uint8_t *bytes = (const uint8_t *)params->value;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < sizeof(params->value); ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

bool params_self_check(const Params *params)
{
    for (unsigned i = 0; i < (unsigned)PARAM_COUNT; ++i) {
        if (!params_range_ok((ParamId)i, params->value[i])) return false;
    }
    if (params->value[PARAM_SERVO_MIN_US] >= params->value[PARAM_SERVO_CENTER_US] ||
        params->value[PARAM_SERVO_CENTER_US] >= params->value[PARAM_SERVO_MAX_US]) return false;
    if (params->value[PARAM_VBAT_MIN_MV] >= params->value[PARAM_VBAT_MAX_MV]) return false;
    return params->crc == params_block_crc(params);
}

void params_reset(Params *params)
{
    params_defaults(params);
}
