#ifndef CHEN_CONTROL_SAFETY_H
#define CHEN_CONTROL_SAFETY_H
#include <stdbool.h>
#include <stdint.h>
#include "faults.h"

/* Control authority and safety state machine.
 *
 *   BOOT -> SELF_TEST -> STANDBY <-> {LOCAL_ACTIVE, REMOTE_ACTIVE}
 *                            \-> FAULT / ESTOP (latched)
 *
 * Only one owner may drive motion at a time. Ownership is granted explicitly by
 * safety_arm() and dropped on every stop, fault, timeout and profile change; a
 * new session id is issued each time so stale packets cannot be replayed.
 */
typedef enum {
    BOOT, SELF_TEST, STANDBY, LOCAL_ACTIVE, REMOTE_ACTIVE, FAULT, ESTOP
} SafetyState;

typedef enum { OWNER_NONE, OWNER_LOCAL, OWNER_BLE, OWNER_PC } Owner;

typedef struct {
    bool estop;              /* true = emergency stop asserted */
    bool interlocks_valid;   /* wiring/interlock chain proven closed */
    bool ble_link;           /* BLE transport considered healthy */
    bool power_ok;           /* battery inside the configured window */
    uint32_t reported_faults;
} SafetyInputs;

typedef struct {
    SafetyState state;
    Owner owner;
    uint32_t faults;
    uint32_t session;
    uint32_t last_motion_ms;
    uint32_t last_arm_ms;
    uint16_t seq;
    int16_t left, right;
    uint16_t servo_us;
    bool have_seq;
    bool motion_permitted;
    uint32_t arm_count, trip_count;
} Safety;

void safety_begin(Safety *s);
void safety_self_test(Safety *s, bool passed);
void safety_step(Safety *s, SafetyInputs in, uint32_t now);
void safety_stop(Safety *s);
void safety_trip(Safety *s, uint32_t faults);
bool safety_clear(Safety *s, SafetyInputs in, bool local_ack);

/* Request control. `local_ack` is the physical confirmation (centre key or a
 * maintenance command) required by the design, never implied by a link. */
bool safety_arm(Safety *s, SafetyInputs in, Owner owner, bool local_ack, uint32_t now);
void safety_disarm(Safety *s);

/* Motor command. Rejected unless this owner holds the current session and the
 * sequence number advances. Also refreshes the motion heartbeat. */
bool safety_motion(Safety *s, SafetyInputs in, Owner owner, uint32_t session,
                   uint16_t seq, int16_t left, int16_t right, uint32_t now);

/* Steering command, gated by the same authority as motion. */
bool safety_servo(Safety *s, SafetyInputs in, Owner owner, uint32_t session,
                  uint16_t pulse_us, uint32_t now);

bool safety_active(const Safety *s);
const char *safety_state_name(SafetyState state);
const char *owner_name(Owner owner);

#endif
