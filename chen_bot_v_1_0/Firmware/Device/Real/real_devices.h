#ifndef CHEN_DEVICE_REAL_DEVICES_H
#define CHEN_DEVICE_REAL_DEVICES_H
#include "devices.h"

/* Real hardware adapter: maps the abstract Devices table onto the BSP calls.
 * No policy, no limits and no state machine live here. */
Devices real_devices(void);

#endif
