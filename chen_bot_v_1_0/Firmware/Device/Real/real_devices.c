#include "real_devices.h"
#include "board.h"
#include "features.h"
#include "keys.h"
#include "stm32h7xx_hal.h"

static DeviceResult real_motor(void *context, int16_t left, int16_t right)
{
    (void)context;
#if CHEN_FEATURE_ACTUATION
    if (left < -1000 || left > 1000 || right < -1000 || right > 1000) return DEV_INVALID;
    board_motor_set(left, right);
    return DEV_OK;
#else
    (void)left;
    (void)right;
    return DEV_UNAVAILABLE;
#endif
}

static DeviceResult real_servo(void *context, uint16_t pulse_us)
{
    (void)context;
#if CHEN_FEATURE_ACTUATION
    if (pulse_us < 400u || pulse_us > 2600u) return DEV_INVALID;
    board_servo_set(pulse_us);
    return DEV_OK;
#else
    (void)pulse_us;
    return DEV_UNAVAILABLE;
#endif
}

static DeviceResult real_power(void *context, bool enable)
{
    (void)context;
    return board_power_set(enable) ? DEV_OK : DEV_UNAVAILABLE;
}

static DeviceResult real_display(void *context, const char *text)
{
    (void)context;
    (void)text;
    /* The HMI owns the panel; plain text banners are not used in this build. */
    return DEV_UNAVAILABLE;
}

static DeviceResult real_keys(void *context, Key *key)
{
    (void)context;
    if (key == NULL) return DEV_INVALID;
    uint16_t mv = 0u;
    if (!board_analog_key(&mv)) return DEV_IO;
    *key = key_classify_mv(mv);
    return DEV_OK;
}

static DeviceResult real_voltage(void *context, unsigned channel, Reading *out)
{
    (void)context;
    if (out == NULL) return DEV_INVALID;
    switch (channel) {
    case 0u:
        return board_analog_battery(out) ? DEV_OK : DEV_UNAVAILABLE;
    case 1u:
    case 2u:
        if (channel == 2u) {
            /* K2 contact feedback is not wired. */
            *out = (Reading){0, HAL_GetTick(), DATA_ABSENT, false};
            return DEV_UNAVAILABLE;
        }
        return board_analog_estop(out) ? DEV_OK : DEV_UNAVAILABLE;
    default:
        *out = (Reading){0, HAL_GetTick(), DATA_INVALID, false};
        return DEV_INVALID;
    }
}

static DeviceResult real_current(void *context, Reading *out)
{
    (void)context;
    if (out == NULL) return DEV_INVALID;
    *out = (Reading){0, HAL_GetTick(), DATA_ABSENT, false};
    return DEV_UNAVAILABLE;
}

static DeviceResult real_speed(void *context, unsigned wheel, Reading *out)
{
    (void)context;
    (void)wheel;
    if (out == NULL) return DEV_INVALID;
    *out = (Reading){0, HAL_GetTick(), DATA_ABSENT, false};
    return DEV_UNAVAILABLE;
}

static DeviceResult real_position(void *context, Reading *out)
{
    (void)context;
    if (out == NULL) return DEV_INVALID;
    *out = (Reading){0, HAL_GetTick(), DATA_ABSENT, false};
    return DEV_UNAVAILABLE;
}

static DeviceResult real_relay(void *context, unsigned relay, bool *closed)
{
    (void)context;
    if (closed == NULL) return DEV_INVALID;
    return board_relay_state(relay, closed) ? DEV_OK : DEV_UNAVAILABLE;
}

Devices real_devices(void)
{
    Devices d = {0};
    d.context = NULL;
    d.motor = real_motor;
    d.servo = real_servo;
    d.power = real_power;
    d.display = real_display;
    d.keys = real_keys;
    d.voltage = real_voltage;
    d.current = real_current;
    d.speed = real_speed;
    d.position = real_position;
    d.relay_feedback = real_relay;
    return d;
}
