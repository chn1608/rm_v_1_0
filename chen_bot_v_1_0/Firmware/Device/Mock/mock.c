#include "mock.h"
#include "board_config.h"
#include <stddef.h>
#include <string.h>

static DeviceResult mock_motor(void *context, int16_t left, int16_t right)
{
    Mock *m = context;
    if (m == NULL) return DEV_INVALID;
    if (left < -1000 || left > 1000 || right < -1000 || right > 1000) {
        ++m->rejected;
        return DEV_INVALID;
    }
    m->left_permille = left;
    m->right_permille = right;
    ++m->writes;
    return DEV_OK;
}

static DeviceResult mock_servo(void *context, uint16_t pulse_us)
{
    Mock *m = context;
    if (m == NULL) return DEV_INVALID;
    if (pulse_us < 400u || pulse_us > 2600u) {
        ++m->rejected;
        return DEV_INVALID;
    }
    m->pulse_us = pulse_us;
    ++m->writes;
    return DEV_OK;
}

static DeviceResult mock_power(void *context, bool enable)
{
    Mock *m = context;
    if (m == NULL) return DEV_INVALID;
    /* The mock refuses to energise while it claims a fault, mirroring the real
     * interlock path. */
    if (enable && m->battery_mv < BOARD_VBAT_MIN_MV) {
        ++m->rejected;
        return DEV_IO;
    }
    m->powered = enable;
    return DEV_OK;
}

static DeviceResult mock_display(void *context, const char *text)
{
    Mock *m = context;
    if (m == NULL || text == NULL) return DEV_INVALID;
    ++m->writes;
    return DEV_OK;
}

static DeviceResult mock_keys(void *context, Key *key)
{
    Mock *m = context;
    if (m == NULL || key == NULL) return DEV_INVALID;
    *key = m->key;
    return DEV_OK;
}

static DeviceResult mock_voltage(void *context, unsigned channel, Reading *out)
{
    Mock *m = context;
    if (m == NULL || out == NULL) return DEV_INVALID;
    if (channel != 0u) {
        *out = (Reading){0, m->now_ms, DATA_ABSENT, false};
        return DEV_UNAVAILABLE;
    }
    *out = (Reading){m->battery_mv, m->now_ms, DATA_SIMULATED, m->battery_mv > 0};
    return DEV_OK;
}

static DeviceResult mock_current(void *context, Reading *out)
{
    Mock *m = context;
    if (m == NULL || out == NULL) return DEV_INVALID;
    *out = (Reading){0, m->now_ms, DATA_ABSENT, false};
    return DEV_UNAVAILABLE;
}

static DeviceResult mock_speed(void *context, unsigned wheel, Reading *out)
{
    Mock *m = context;
    (void)wheel;
    if (m == NULL || out == NULL) return DEV_INVALID;
    *out = (Reading){0, m->now_ms, DATA_ABSENT, false};
    return DEV_UNAVAILABLE;
}

static DeviceResult mock_position(void *context, Reading *out)
{
    Mock *m = context;
    if (m == NULL || out == NULL) return DEV_INVALID;
    *out = (Reading){0, m->now_ms, DATA_ABSENT, false};
    return DEV_UNAVAILABLE;
}

static DeviceResult mock_relay(void *context, unsigned relay, bool *closed)
{
    Mock *m = context;
    if (m == NULL || closed == NULL || relay > 1u) return DEV_INVALID;
    *closed = m->powered;
    return DEV_OK;
}

Devices mock_devices(Mock *model)
{
    Devices d = {0};
    d.context = model;
    d.motor = mock_motor;
    d.servo = mock_servo;
    d.power = mock_power;
    d.display = mock_display;
    d.keys = mock_keys;
    d.voltage = mock_voltage;
    d.current = mock_current;
    d.speed = mock_speed;
    d.position = mock_position;
    d.relay_feedback = mock_relay;
    return d;
}
