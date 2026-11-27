#include "safety.h"
#include "features.h"
#include "profile.h"
#include <stddef.h>

static void disarm(Safety *s)
{
    s->owner = OWNER_NONE;
    s->left = 0;
    s->right = 0;
    s->have_seq = false;
    s->motion_permitted = false;
    s->servo_us = 0u;
}

void safety_begin(Safety *s)
{
    const Safety initial = {.state = BOOT};
    *s = initial;
    s->state = SELF_TEST;
    s->servo_us = 0u;
}

void safety_trip(Safety *s, uint32_t faults)
{
    if (faults != 0u) ++s->trip_count;
    disarm(s);
    s->faults |= faults;
    s->state = (s->faults & FAULT_ESTOP) != 0u ? ESTOP : FAULT;
}

void safety_self_test(Safety *s, bool passed)
{
    if (s->state != SELF_TEST) return;
    if (passed) s->state = STANDBY;
    else safety_trip(s, FAULT_SELF_TEST);
}

void safety_stop(Safety *s)
{
    disarm(s);
    if (s->state == LOCAL_ACTIVE || s->state == REMOTE_ACTIVE) s->state = STANDBY;
}

void safety_step(Safety *s, SafetyInputs in, uint32_t now)
{
    uint32_t faults = in.reported_faults;
    if (in.estop) faults |= FAULT_ESTOP;
    if (s->owner != OWNER_NONE) {
        if (!in.interlocks_valid) faults |= FAULT_INTERLOCK;
        if (!in.power_ok) faults |= FAULT_POWER;
        if (s->owner == OWNER_BLE && !in.ble_link) faults |= FAULT_LINK;
        if (now - s->last_motion_ms >= CHEN_MOTION_TIMEOUT_MS) faults |= FAULT_TIMEOUT;
    }
    if (faults != 0u) safety_trip(s, faults);
}

bool safety_clear(Safety *s, SafetyInputs in, bool local_ack)
{
    if (!local_ack || in.estop || !in.interlocks_valid || in.reported_faults != 0u)
        return false;
    if (s->state != ESTOP && s->state != FAULT) return false;
    /* A clear never re-arms: the machine returns to STANDBY and needs a fresh
     * arm with a new session id. */
    s->faults = 0u;
    s->state = SELF_TEST;
    return true;
}

bool safety_arm(Safety *s, SafetyInputs in, Owner owner, bool local_ack, uint32_t now)
{
    safety_step(s, in, now);
#if !CHEN_REAL_OUTPUTS
    /* Actuation is compiled out for this profile: arming is impossible by
     * construction, not merely refused at run time. */
    (void)in;
    (void)owner;
    (void)local_ack;
    (void)now;
    return false;
#else
    if (s->state != STANDBY || !local_ack || !in.interlocks_valid || !in.power_ok) return false;
    if (owner != OWNER_LOCAL && owner != OWNER_BLE) return false;
    if (owner == OWNER_BLE && !in.ble_link) return false;
    if (s->session == UINT32_MAX) return false;

    ++s->session;
    s->owner = owner;
    s->state = owner == OWNER_LOCAL ? LOCAL_ACTIVE : REMOTE_ACTIVE;
    s->last_motion_ms = now;
    s->last_arm_ms = now;
    s->have_seq = false;
    s->motion_permitted = true;
    ++s->arm_count;
    return true;
#endif
}

void safety_disarm(Safety *s)
{
    safety_stop(s);
    s->servo_us = 0u;
}

static bool authority_ok(const Safety *s, Owner owner, uint32_t session)
{
    return s->owner != OWNER_NONE && s->owner == owner && s->session == session &&
           (s->state == LOCAL_ACTIVE || s->state == REMOTE_ACTIVE);
}

bool safety_motion(Safety *s, SafetyInputs in, Owner owner, uint32_t session,
                   uint16_t seq, int16_t left, int16_t right, uint32_t now)
{
    const uint16_t advance = (uint16_t)(seq - s->seq);
    safety_step(s, in, now);
    if (!authority_ok(s, owner, session)) return false;
    if (left < -1000 || left > 1000 || right < -1000 || right > 1000) return false;
    /* Sequence must strictly advance: replays and duplicates are dropped, and
     * wraparound is handled by comparing half-range. */
    if (s->have_seq && (advance == 0u || advance >= 0x8000u)) return false;

    s->left = left;
    s->right = right;
    s->seq = seq;
    s->have_seq = true;
    s->motion_permitted = true;
    s->last_motion_ms = now;
    return true;
}

bool safety_servo(Safety *s, SafetyInputs in, Owner owner, uint32_t session,
                  uint16_t pulse_us, uint32_t now)
{
    safety_step(s, in, now);
    if (!authority_ok(s, owner, session)) return false;
    if (pulse_us < 400u || pulse_us > 2600u) return false;
    s->servo_us = pulse_us;
    s->last_motion_ms = now;
    return true;
}

bool safety_active(const Safety *s)
{
    return s->state == LOCAL_ACTIVE || s->state == REMOTE_ACTIVE;
}

const char *safety_state_name(SafetyState state)
{
    switch (state) {
    case BOOT: return "boot";
    case SELF_TEST: return "self-test";
    case STANDBY: return "standby";
    case LOCAL_ACTIVE: return "local";
    case REMOTE_ACTIVE: return "remote";
    case FAULT: return "fault";
    case ESTOP: return "estop";
    }
    return "?";
}

const char *owner_name(Owner owner)
{
    switch (owner) {
    case OWNER_NONE: return "none";
    case OWNER_LOCAL: return "local";
    case OWNER_BLE: return "ble";
    case OWNER_PC: return "pc";
    }
    return "?";
}
