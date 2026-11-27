#ifndef CHEN_DEBUG_DEBUG_SERVICE_H
#define CHEN_DEBUG_DEBUG_SERVICE_H
#include <stdbool.h>
#include <stdint.h>
#include "params.h"
#include "protocol.h"
#include "safety.h"
#include "snapshot.h"

/* Single entry point for every request coming from the PC link, the BLE link
 * or the local panel. Keeping the permission matrix here is what stops three
 * terminals from growing three conflicting parameter paths.
 *
 * Permission matrix (see docs/PROTOCOL.md):
 *
 *   source        status/health/version/faults   param get   param set   arm/motion   servo
 *   OWNER_PC      yes                            yes         maint only  no           no
 *   OWNER_BLE     yes                            yes         no          yes          yes
 *   OWNER_LOCAL   yes                            yes         maint only  yes          yes
 */
typedef struct {
    Params *params;
    Safety *safety;
    Snapshot *snapshot;
    bool maintenance;      /* local maintenance authority is present */
    uint8_t version_major, version_minor, version_patch;
    uint32_t handled, denied, bad_length;
} DebugService;

void debug_init(DebugService *svc, Params *params, Safety *safety, Snapshot *snapshot);

/* Dispatch one request. Always produces a reply of the matching reply type
 * (or NAK), with the request sequence copied back. */
void debug_handle(DebugService *svc, Owner source, const Frame *request, Frame *reply);

/* Reusable NAK builder for callers that need to reject a frame early. */
void debug_nak(Frame *reply, uint16_t sequence, uint8_t reason);

#endif
