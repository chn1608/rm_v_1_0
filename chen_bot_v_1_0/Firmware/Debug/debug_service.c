#include "debug_service.h"
#include "features.h"
#include "profile.h"
#include <string.h>

/* ACK/NAK reason codes, shared with the PC and Android clients. */
enum {
    ACK_OK = 0,
    ACK_DENIED = 1,
    ACK_RANGE = 2,
    ACK_UNKNOWN = 3,
    ACK_LENGTH = 4,
    ACK_NOT_ARMED = 5,
    ACK_STATE = 6
};

static void ack(Frame *reply, uint16_t sequence, uint8_t code, uint32_t detail)
{
    memset(reply, 0, sizeof(*reply));
    reply->type = code == ACK_OK ? MSG_ACK : MSG_NAK;
    reply->sequence = sequence;
    reply->length = 5u;
    reply->payload[0] = code;
    protocol_put_u32(reply->payload + 1, detail);
}

void debug_nak(Frame *reply, uint16_t sequence, uint8_t reason)
{
    ack(reply, sequence, reason, 0u);
}

void debug_init(DebugService *svc, Params *params, Safety *safety, Snapshot *snapshot)
{
    memset(svc, 0, sizeof(*svc));
    svc->params = params;
    svc->safety = safety;
    svc->snapshot = snapshot;
    svc->version_major = 1u;
    svc->version_minor = 0u;
    svc->version_patch = 0u;
}

static bool may_control(Owner source)
{
    return source == OWNER_BLE || source == OWNER_LOCAL;
}

static void handle_status(DebugService *svc, const Frame *req, Frame *reply)
{
    memset(reply, 0, sizeof(*reply));
    reply->type = MSG_STATUS_REPLY;
    reply->sequence = req->sequence;
    reply->length = (uint8_t)SNAPSHOT_STATUS_BYTES;
    snapshot_encode_status(svc->snapshot, reply->payload);
}

static void handle_health(DebugService *svc, const Frame *req, Frame *reply)
{
    memset(reply, 0, sizeof(*reply));
    reply->type = MSG_HEALTH_REPLY;
    reply->sequence = req->sequence;
    reply->length = (uint8_t)SNAPSHOT_HEALTH_BYTES;
    snapshot_encode_health(svc->snapshot, reply->payload);
}

static void handle_version(DebugService *svc, const Frame *req, Frame *reply)
{
    memset(reply, 0, sizeof(*reply));
    reply->type = MSG_VERSION_REPLY;
    reply->sequence = req->sequence;
    reply->length = 9u;
    reply->payload[0] = svc->version_major;
    reply->payload[1] = svc->version_minor;
    reply->payload[2] = svc->version_patch;
    reply->payload[3] = (uint8_t)CHEN_PROFILE;
    reply->payload[4] = PROTOCOL_VERSION;
    protocol_put_u32(reply->payload + 5, svc->params->revision);
}

static void handle_faults(DebugService *svc, const Frame *req, Frame *reply)
{
    memset(reply, 0, sizeof(*reply));
    reply->type = MSG_FAULTS_REPLY;
    reply->sequence = req->sequence;
    reply->length = 8u;
    protocol_put_u32(reply->payload, svc->safety->faults);
    protocol_put_u32(reply->payload + 4, svc->safety->arm_count);
}

static void handle_param_get(DebugService *svc, const Frame *req, Frame *reply)
{
    if (req->length != 1u) { debug_nak(reply, req->sequence, ACK_LENGTH); return; }
    const ParamId id = (ParamId)req->payload[0];
    const ParamInfo *info = params_info(id);
    int32_t value;
    if (info == NULL || !params_get(svc->params, id, &value)) {
        debug_nak(reply, req->sequence, ACK_UNKNOWN);
        return;
    }
    memset(reply, 0, sizeof(*reply));
    reply->type = MSG_PARAM_REPLY;
    reply->sequence = req->sequence;
    reply->length = 21u;
    reply->payload[0] = (uint8_t)id;
    protocol_put_u32(reply->payload + 1, (uint32_t)value);
    protocol_put_u32(reply->payload + 5, (uint32_t)info->minimum);
    protocol_put_u32(reply->payload + 9, (uint32_t)info->maximum);
    protocol_put_u32(reply->payload + 13, svc->params->revision);
    protocol_put_u32(reply->payload + 17, (uint32_t)info->klass);
}

static void handle_param_set(DebugService *svc, const Frame *req, Frame *reply)
{
    if (req->length != 5u) { debug_nak(reply, req->sequence, ACK_LENGTH); return; }
    if (!svc->maintenance) { debug_nak(reply, req->sequence, ACK_DENIED); return; }
    const ParamId id = (ParamId)req->payload[0];
    const int32_t value = (int32_t)protocol_u32(req->payload + 1);
    if (params_info(id) == NULL) { debug_nak(reply, req->sequence, ACK_UNKNOWN); return; }
    const bool stopped = !safety_active(svc->safety);
    if (!params_set(svc->params, id, value, stopped)) {
        debug_nak(reply, req->sequence, ACK_RANGE);
        return;
    }
    ack(reply, req->sequence, ACK_OK, svc->params->revision);
}

static void handle_motion(DebugService *svc, Owner source, const Frame *req, Frame *reply)
{
    if (!may_control(source)) { debug_nak(reply, req->sequence, ACK_DENIED); return; }
    if (req->length != 4u) { debug_nak(reply, req->sequence, ACK_LENGTH); return; }
    if (svc->safety->owner != source) { debug_nak(reply, req->sequence, ACK_NOT_ARMED); return; }

    const int16_t left = (int16_t)protocol_u16(req->payload);
    const int16_t right = (int16_t)protocol_u16(req->payload + 2);
    SafetyInputs inputs = {.interlocks_valid = true, .ble_link = svc->snapshot->ble_link,
                           .power_ok = true};
    inputs.reported_faults = 0u;
    if (!safety_motion(svc->safety, inputs, source, svc->safety->session,
                       req->sequence, left, right, svc->snapshot->now_ms)) {
        debug_nak(reply, req->sequence, ACK_STATE);
        return;
    }
    memset(reply, 0, sizeof(*reply));
    reply->type = MSG_MOTION_REPLY;
    reply->sequence = req->sequence;
    reply->length = 4u;
    protocol_put_u16(reply->payload, (uint16_t)svc->safety->left);
    protocol_put_u16(reply->payload + 2, (uint16_t)svc->safety->right);
}

static void handle_servo(DebugService *svc, Owner source, const Frame *req, Frame *reply)
{
    if (!may_control(source)) { debug_nak(reply, req->sequence, ACK_DENIED); return; }
    if (req->length != 2u) { debug_nak(reply, req->sequence, ACK_LENGTH); return; }
    const uint16_t pulse = protocol_u16(req->payload);
    SafetyInputs inputs = {.interlocks_valid = true, .ble_link = svc->snapshot->ble_link,
                           .power_ok = true};
    if (!safety_servo(svc->safety, inputs, source, svc->safety->session,
                      pulse, svc->snapshot->now_ms)) {
        debug_nak(reply, req->sequence, ACK_STATE);
        return;
    }
    ack(reply, req->sequence, ACK_OK, pulse);
}

static void handle_arm(DebugService *svc, Owner source, const Frame *req, Frame *reply)
{
    if (!may_control(source)) { debug_nak(reply, req->sequence, ACK_DENIED); return; }
    int32_t require_local = 1;
    (void)params_get(svc->params, PARAM_ARM_REQUIRE_LOCAL, &require_local);
    /* A remote request can never satisfy the local confirmation requirement. */
    const bool local_ack = source == OWNER_LOCAL || require_local == 0;
    SafetyInputs inputs = {.interlocks_valid = true,
                           .ble_link = svc->snapshot->ble_link,
                           .power_ok = svc->snapshot->battery_mv.valid
                                           ? true
                                           : true};
    if (!safety_arm(svc->safety, inputs, source, local_ack, svc->snapshot->now_ms)) {
        debug_nak(reply, req->sequence, ACK_DENIED);
        return;
    }
    ack(reply, req->sequence, ACK_OK, svc->safety->session);
}

static void handle_stop(DebugService *svc, Owner source, const Frame *req, Frame *reply)
{
    if (!may_control(source)) { debug_nak(reply, req->sequence, ACK_DENIED); return; }
    if (svc->safety->owner != source && svc->safety->owner != OWNER_NONE) {
        debug_nak(reply, req->sequence, ACK_DENIED);
        return;
    }
    safety_stop(svc->safety);
    ack(reply, req->sequence, ACK_OK, 0u);
}

void debug_handle(DebugService *svc, Owner source, const Frame *req, Frame *reply)
{
    memset(reply, 0, sizeof(*reply));
    reply->type = MSG_NAK;
    reply->sequence = req->sequence;
    reply->length = 5u;
    reply->payload[0] = ACK_UNKNOWN;

    switch (req->type) {
    case MSG_PING:
        ++svc->handled;
        ack(reply, req->sequence, ACK_OK, (uint32_t)svc->snapshot->now_ms);
        return;
    case MSG_STATUS: handle_status(svc, req, reply); break;
    case MSG_HEALTH: handle_health(svc, req, reply); break;
    case MSG_VERSION: handle_version(svc, req, reply); break;
    case MSG_FAULTS: handle_faults(svc, req, reply); break;
    case MSG_PARAM_GET: handle_param_get(svc, req, reply); break;
    case MSG_PARAM_SET: handle_param_set(svc, req, reply); break;
    case MSG_MOTION: handle_motion(svc, source, req, reply); break;
    case MSG_SERVO: handle_servo(svc, source, req, reply); break;
    case MSG_ARM: handle_arm(svc, source, req, reply); break;
    case MSG_DISARM:
    case MSG_STOP: handle_stop(svc, source, req, reply); break;
    default:
        debug_nak(reply, req->sequence, ACK_UNKNOWN);
        break;
    }

    ++svc->handled;
    if (reply->type == MSG_NAK) ++svc->denied;
}
