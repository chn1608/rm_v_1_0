#ifndef CHEN_SERVICE_FAULTS_H
#define CHEN_SERVICE_FAULTS_H
#include <stdint.h>

/* Latched fault bits. A bit stays set until an explicit, authorised clear so a
 * transient event cannot be hidden by the next good sample. */
enum {
    FAULT_SELF_TEST = 1u << 0,
    FAULT_INTERLOCK = 1u << 1,
    FAULT_LINK = 1u << 2,
    FAULT_TIMEOUT = 1u << 3,
    FAULT_DEVICE = 1u << 4,
    FAULT_ESTOP = 1u << 5,
    FAULT_POWER = 1u << 6,
    FAULT_PARAM = 1u << 7,
    FAULT_COMM = 1u << 8,
    FAULT_WATCHDOG = 1u << 9,
    FAULT_OVERRUN = 1u << 10
};

/* Stable short name for telemetry and the LCD fault page. */
const char *fault_name(uint32_t bit);
/* Human readable list of the set bits, space separated, for a debug banner. */
void fault_describe(uint32_t faults, char *out, unsigned capacity);

#endif
