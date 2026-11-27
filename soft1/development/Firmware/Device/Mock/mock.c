#include "mock.h"
#include "profile.h"
#if RM_PROFILE != RM_SIM
#error Mock is exclusive to SIM
#endif

static DeviceResult power(void *ctx, bool on)
{
    Mock *m = ctx;
    m->powered = on;
    if (!on) {
        m->left = 0;
        m->right = 0;
        m->pulse_us = 0;
    }
    return DEV_OK;
}
static DeviceResult motors(void *ctx, int16_t left, int16_t right)
{
    Mock *m = ctx;
    if (!m->powered) return DEV_UNAVAILABLE;
    if (left < -1000 || left > 1000 || right < -1000 || right > 1000) return DEV_INVALID;
    m->left = left;
    m->right = right;
    return DEV_OK;
}
static DeviceResult servo(void *ctx, uint16_t us)
{
    Mock *m = ctx;
    if (!m->powered) return DEV_UNAVAILABLE;
    if (us < 1000u || us > 2000u) return DEV_INVALID;
    m->pulse_us = us;
    return DEV_OK;
}
static DeviceResult voltage(void *ctx, unsigned channel, Reading *r)
{
    const Mock *m = ctx;
    if (channel != 0u) {
        *r = (Reading){0, m->now_ms, DATA_ABSENT, false};
        return DEV_UNAVAILABLE;
    }
    *r = (Reading){15000, m->now_ms, DATA_SIMULATED, true};
    return DEV_OK;
}
Devices mock_devices(Mock *model)
{
    const Devices d = { .context = model, .motors = motors, .servo = servo,
                        .power = power, .voltage = voltage };
    return d;
}
