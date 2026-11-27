#ifndef CHEN_APP_APP_H
#define CHEN_APP_APP_H
#include <stdbool.h>
#include "devices.h"
#include "params.h"
#include "safety.h"
#include "snapshot.h"

/* Application wiring: one owner for the device table, the safety state, the
 * parameter block and the tasks that consume them. */
bool app_init(const Devices *devices, bool can_arm, bool panel_ready);
void app_start(void);

/* The single status snapshot, copied out atomically. Readers on any task get a
 * consistent view; writers are only the control task. */
void app_snapshot(Snapshot *out);
Safety *app_safety(void);
Params *app_params(void);

#endif
