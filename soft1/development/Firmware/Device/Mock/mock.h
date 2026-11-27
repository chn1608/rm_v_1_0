#ifndef RM_MOCK_H
#define RM_MOCK_H
#include "devices.h"

typedef struct {
    int16_t left, right;
    uint16_t pulse_us;
    uint32_t now_ms;
    bool powered;
} Mock;
Devices mock_devices(Mock *model);
#endif
