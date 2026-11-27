#include "service.h"
#include "profile.h"

void params_reset(Params *params) { params->telemetry_ms = 50u; }

void debug_handle(Params *params, Safety *safety, const Snapshot *snapshot,
                  Owner source, bool local_maintenance, const Frame *req, Frame *reply)
{
    const Frame empty = {0};
    *reply = empty;
    reply->sequence = req->sequence;
    reply->type = MSG_ACK;
    reply->length = 2u;
    reply->payload[0] = req->type;
    DebugResult result = DEBUG_OK;

    switch (req->type) {
    case MSG_PING:
        if (req->length != 0u) result = DEBUG_BAD_LENGTH;
        break;
    case MSG_STATUS:
        if (req->length != 0u) { result = DEBUG_BAD_LENGTH; break; }
        reply->type = MSG_STATUS_REPLY;
        reply->length = 35u;
        protocol_put_u32(reply->payload, snapshot->now_ms);
        reply->payload[4] = RM_PROFILE;
        reply->payload[5] = (uint8_t)safety->state;
        reply->payload[6] = (uint8_t)safety->owner;
        reply->payload[7] = (uint8_t)safety->motion_permitted;
        protocol_put_u32(reply->payload + 8, safety->faults);
        reply->payload[12] = (uint8_t)snapshot->bus_mv.source;
        reply->payload[13] = (uint8_t)snapshot->bus_mv.valid;
        protocol_put_u32(reply->payload + 14, (uint32_t)snapshot->bus_mv.value);
        protocol_put_u32(reply->payload + 18, snapshot->bus_mv.time_ms);
        protocol_put_u16(reply->payload + 22, (uint16_t)safety->left);
        protocol_put_u16(reply->payload + 24, (uint16_t)safety->right);
        reply->payload[26] = RM_CONFIG_VERSION;
        protocol_put_u32(reply->payload + 27, snapshot->good_frames);
        protocol_put_u32(reply->payload + 31, snapshot->bad_frames);
        return;
    case MSG_PARAM_GET:
        if (req->length != 1u) { result = DEBUG_BAD_LENGTH; break; }
        if (req->payload[0] != 1u) { result = DEBUG_UNKNOWN; break; }
        reply->type = MSG_PARAM_REPLY;
        reply->length = 3u;
        reply->payload[0] = 1u;
        protocol_put_u16(reply->payload + 1, params->telemetry_ms);
        return;
    case MSG_PARAM_SET:
        if (req->length != 3u) { result = DEBUG_BAD_LENGTH; break; }
        if (source != OWNER_LOCAL || !local_maintenance || safety->state != STANDBY) {
            result = DEBUG_DENIED; break;
        }
        if (req->payload[0] != 1u) { result = DEBUG_UNKNOWN; break; }
        {
            const uint16_t ms = protocol_u16(req->payload + 1);
            if (ms < 20u || ms > 1000u) result = DEBUG_RANGE;
            else params->telemetry_ms = ms;
        }
        break;
    case MSG_STOP:
        if (req->length != 0u) { result = DEBUG_BAD_LENGTH; break; }
        if (source != OWNER_LOCAL && source != OWNER_BLE) {
            result = DEBUG_DENIED;
        } else {
            safety_stop(safety);
        }
        break;
    default:
        result = DEBUG_UNKNOWN;
        break;
    }
    reply->payload[1] = (uint8_t)result;
}
