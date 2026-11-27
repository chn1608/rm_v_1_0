#ifndef CHEN_DEVICE_MOCK_H
#define CHEN_DEVICE_MOCK_H
#include "devices.h"

/* Simulated devices for the SIM profile and for host tests. Every reading is
 * tagged DATA_SIMULATED so it can never be mistaken for a measurement. */
typedef struct {
    int16_t left_permille, right_permille;
    uint16_t pulse_us;
    uint32_t now_ms;
    bool powered;
    bool ble_link;
    Key key;
    int32_t battery_mv;
    uint32_t writes;
    uint32_t rejected;
} Mock;

Devices mock_devices(Mock *model);

#endif
