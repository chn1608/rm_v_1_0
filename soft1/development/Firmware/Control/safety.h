#ifndef RM_SAFETY_H
#define RM_SAFETY_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { BOOT, SELF_TEST, STANDBY, LOCAL_ACTIVE, REMOTE_ACTIVE, FAULT, ESTOP } State;
typedef enum { OWNER_NONE, OWNER_LOCAL, OWNER_BLE, OWNER_PC } Owner;
enum {
    FAULT_SELF_TEST = 1u, FAULT_INTERLOCK = 2u, FAULT_LINK = 4u,
    FAULT_TIMEOUT = 8u, FAULT_DEVICE = 16u, FAULT_ESTOP = 32u
};
typedef struct {
    bool estop, interlocks_valid, ble_link;
    uint32_t reported_faults;
} SafetyInputs;
typedef struct {
    State state;
    Owner owner;
    uint32_t faults, last_motion_ms, session;
    uint16_t seq;
    int16_t left, right;
    bool have_seq, motion_permitted;
} Safety;

void safety_begin(Safety *s);
void safety_self_test(Safety *s, bool passed);
void safety_step(Safety *s, SafetyInputs in, uint32_t now);
void safety_stop(Safety *s);
void safety_trip(Safety *s, uint32_t faults);
bool safety_clear(Safety *s, SafetyInputs in, bool local_ack);
bool safety_arm(Safety *s, SafetyInputs in, Owner owner, bool local_ack, uint32_t now);
bool safety_motion(Safety *s, SafetyInputs in, Owner owner, uint32_t session,
                   uint16_t seq, int16_t left, int16_t right, uint32_t now);
#endif
