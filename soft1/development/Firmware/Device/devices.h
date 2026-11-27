#ifndef RM_DEVICES_H
#define RM_DEVICES_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { DEV_OK, DEV_UNAVAILABLE, DEV_INVALID, DEV_IO } DeviceResult;
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

typedef struct {
    void *context;
    DeviceResult (*motors)(void *, int16_t left_permille, int16_t right_permille);
    DeviceResult (*servo)(void *, uint16_t pulse_us);
    DeviceResult (*power)(void *, bool enable);
    DeviceResult (*display)(void *, const char *text);
    DeviceResult (*keys)(void *, Key *);
    DeviceResult (*voltage)(void *, unsigned channel, Reading *millivolts);
    DeviceResult (*current)(void *, Reading *milliamps);
    DeviceResult (*speed)(void *, unsigned wheel, Reading *rpm);
    DeviceResult (*position)(void *, Reading *millidegrees);
} Devices;

typedef struct {
    void *context;
    DeviceResult (*read)(void *, uint8_t *data, unsigned capacity, unsigned *used);
    DeviceResult (*write)(void *, const uint8_t *data, unsigned length);
} Transport; /* Instantiate independently for PC and BLE. */
#endif
