#ifndef CHEN_DEVICE_DEVICES_H
#define CHEN_DEVICE_DEVICES_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Uniform result for every device call. DEV_UNAVAILABLE means the feature is
 * compiled out or not fitted; it is never treated as success. */
typedef enum { DEV_OK, DEV_UNAVAILABLE, DEV_INVALID, DEV_IO } DeviceResult;

/* Where a number came from. Upper layers display this instead of pretending
 * every value is a measurement. */
typedef enum {
    DATA_INVALID, DATA_MEASURED, DATA_COMMAND, DATA_ESTIMATED,
    DATA_SIMULATED, DATA_ABSENT, DATA_UNCALIBRATED, DATA_STALE
} DataSource;

typedef struct {
    int32_t value;
    uint32_t time_ms;
    DataSource source;
    bool valid;
} Reading;

typedef enum { KEY_NONE, KEY_CENTER, KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_UNKNOWN } Key;

/* Device table. A NULL entry means "not implemented by this board", which is
 * reported as DEV_UNAVAILABLE rather than silently succeeding. */
typedef struct {
    void *context;

    /* Signed permille command for both wheels, -1000..1000. */
    DeviceResult (*motor)(void *, int16_t left_permille, int16_t right_permille);
    /* Absolute pulse width in microseconds, already clamped by the caller. */
    DeviceResult (*servo)(void *, uint16_t pulse_us);
    /* Main power latch (relays K1/K2). false must always be safe. */
    DeviceResult (*power)(void *, bool enable);
    /* Plain text on the local display, used for boot and fault banners. */
    DeviceResult (*display)(void *, const char *text);
    /* Blocking-free key sample: returns the key held at call time. */
    DeviceResult (*keys)(void *, Key *key);
    /* Channel 0 = battery pack, 1 = K1 contact, 2 = K2 contact. */
    DeviceResult (*voltage)(void *, unsigned channel, Reading *millivolts);
    DeviceResult (*current)(void *, Reading *milliamps);
    DeviceResult (*speed)(void *, unsigned wheel, Reading *rpm);
    DeviceResult (*position)(void *, Reading *millidegrees);
    DeviceResult (*relay_feedback)(void *, unsigned relay, bool *closed);
} Devices;

/* Byte transport used by both the PC link and the BLE link. Reads never block. */
typedef struct {
    void *context;
    DeviceResult (*read)(void *, uint8_t *data, unsigned capacity, unsigned *used);
    DeviceResult (*write)(void *, const uint8_t *data, unsigned length);
} Transport;

/* Helpers so callers do not repeat the NULL check pattern. */
static inline DeviceResult device_call(DeviceResult (*fn)(void *, int16_t, int16_t),
                                       void *ctx, int16_t a, int16_t b)
{
    return fn != NULL ? fn(ctx, a, b) : DEV_UNAVAILABLE;
}

#endif
