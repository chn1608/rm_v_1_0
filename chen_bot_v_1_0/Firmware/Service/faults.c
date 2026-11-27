#include "faults.h"
#include <stdio.h>
#include <string.h>

static const struct {
    uint32_t bit;
    const char *name;
} names[] = {
    {FAULT_SELF_TEST, "self-test"},
    {FAULT_INTERLOCK, "interlock"},
    {FAULT_LINK, "link"},
    {FAULT_TIMEOUT, "timeout"},
    {FAULT_DEVICE, "device"},
    {FAULT_ESTOP, "estop"},
    {FAULT_POWER, "power"},
    {FAULT_PARAM, "param"},
    {FAULT_COMM, "comm"},
    {FAULT_WATCHDOG, "watchdog"},
    {FAULT_OVERRUN, "overrun"},
};

const char *fault_name(uint32_t bit)
{
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (names[i].bit == bit) return names[i].name;
    }
    return "unknown";
}

void fault_describe(uint32_t faults, char *out, unsigned capacity)
{
    if (out == NULL || capacity == 0u) return;
    out[0] = '\0';
    unsigned used = 0u;
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if ((faults & names[i].bit) == 0u) continue;
        const int written = snprintf(out + used, capacity - used, "%s%s",
                                     used != 0u ? " " : "", names[i].name);
        if (written < 0) break;
        const unsigned added = (unsigned)written;
        if (added >= capacity - used) {   /* truncated: keep it terminated */
            out[capacity - 1u] = '\0';
            break;
        }
        used += added;
    }
    if (used == 0u) (void)snprintf(out, capacity, "none");
}
