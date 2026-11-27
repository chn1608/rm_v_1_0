#include "safety.h"
#include "profile.h"

static void disarm(Safety *s)
{
    s->owner = OWNER_NONE;
    s->left = 0;
    s->right = 0;
    s->have_seq = false;
    s->motion_permitted = false;
}

void safety_begin(Safety *s)
{
    const Safety initial = {.state = BOOT};
    *s = initial;
    s->state = SELF_TEST;
}

void safety_trip(Safety *s, uint32_t faults)
{
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
        if (s->owner == OWNER_BLE && !in.ble_link) faults |= FAULT_LINK;
        if (now - s->last_motion_ms >= RM_MOTION_TIMEOUT_MS) faults |= FAULT_TIMEOUT;
    }
    if (faults != 0u) safety_trip(s, faults);
}

bool safety_clear(Safety *s, SafetyInputs in, bool local_ack)
{
    if (!local_ack || in.estop || !in.interlocks_valid || in.reported_faults != 0u ||
        (s->state != ESTOP && s->state != FAULT)) return false;
    s->faults = 0u;
    s->state = SELF_TEST;
    return true;
}

bool safety_arm(Safety *s, SafetyInputs in, Owner owner, bool local_ack, uint32_t now)
{
    safety_step(s, in, now);
    if ((RM_PROFILE != RM_SIM && !RM_REAL_OUTPUTS) || s->state != STANDBY ||
        !local_ack || !in.interlocks_valid || s->session == UINT32_MAX ||
        (owner != OWNER_LOCAL && owner != OWNER_BLE) ||
        (owner == OWNER_BLE && !in.ble_link)) return false;
    s->session++;
    s->last_motion_ms = now;
    s->owner = owner;
    s->state = owner == OWNER_LOCAL ? LOCAL_ACTIVE : REMOTE_ACTIVE;
    return true;
}

bool safety_motion(Safety *s, SafetyInputs in, Owner owner, uint32_t session,
                   uint16_t seq, int16_t left, int16_t right, uint32_t now)
{
    const uint16_t advance = (uint16_t)(seq - s->seq);
    safety_step(s, in, now);
    if (s->owner == OWNER_NONE || owner != s->owner || session != s->session ||
        left < -1000 || left > 1000 || right < -1000 || right > 1000 ||
        (s->have_seq && (advance == 0u || advance >= 0x8000u))) return false;
    s->left = left;
    s->right = right;
    s->seq = seq;
    s->have_seq = true;
    s->motion_permitted = true;
    s->last_motion_ms = now;
    return true;
}
