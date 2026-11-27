#ifndef RM_DEBUG_SERVICE_H
#define RM_DEBUG_SERVICE_H
#include "devices.h"
#include "protocol.h"
#include "safety.h"

#define RM_CONFIG_VERSION 1u
typedef enum { DEBUG_OK, DEBUG_BAD_LENGTH, DEBUG_UNKNOWN, DEBUG_DENIED, DEBUG_RANGE } DebugResult;
typedef struct {
    uint16_t telemetry_ms;
} Params;
typedef struct {
    const Safety *safety;
    Reading bus_mv;
    uint32_t now_ms, good_frames, bad_frames;
} Snapshot;

void params_reset(Params *params);
void debug_handle(Params *params, Safety *safety, const Snapshot *snapshot,
                  Owner source, bool local_maintenance, const Frame *request, Frame *reply);
#endif
