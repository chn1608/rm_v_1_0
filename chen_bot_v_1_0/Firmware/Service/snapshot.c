#include "snapshot.h"
#include "protocol.h"
#include <string.h>

void snapshot_clear(Snapshot *s)
{
    memset(s, 0, sizeof(*s));
}

void snapshot_encode_status(const Snapshot *s, uint8_t out[SNAPSHOT_STATUS_BYTES])
{
    uint8_t flags = 0u;
    if (s->motion_permitted) flags |= 0x01u;
    if (s->battery_mv.valid) flags |= 0x02u;
    if (s->current_ma.valid) flags |= 0x04u;
    if (s->power_enabled) flags |= 0x08u;
    if (s->ble_link) flags |= 0x10u;
    if (s->pc_link) flags |= 0x20u;
    if (s->relay_closed[0]) flags |= 0x40u;
    if (s->relay_closed[1]) flags |= 0x80u;

    memset(out, 0, SNAPSHOT_STATUS_BYTES);
    out[0] = 1u;                              /* payload layout version */
    out[1] = (uint8_t)s->state;
    out[2] = (uint8_t)s->owner;
    out[3] = flags;
    protocol_put_u16(out + 4, (uint16_t)s->left_cmd);
    protocol_put_u16(out + 6, (uint16_t)s->right_cmd);
    protocol_put_u16(out + 8, (uint16_t)s->left_out);
    protocol_put_u16(out + 10, (uint16_t)s->right_out);
    protocol_put_u16(out + 12, s->servo_us);
    protocol_put_u16(out + 14, (uint16_t)(s->servo_mdeg / 10)); /* decidegrees */
    protocol_put_u16(out + 16, s->battery_mv.valid ? (uint16_t)s->battery_mv.value : 0u);
    protocol_put_u16(out + 18, s->current_ma.valid ? (uint16_t)s->current_ma.value : 0u);
    protocol_put_u32(out + 20, s->faults);
    protocol_put_u32(out + 24, s->uptime_s);
    protocol_put_u16(out + 28, (uint16_t)(s->good_frames > 0xFFFFu ? 0xFFFFu : s->good_frames));
    protocol_put_u16(out + 30, (uint16_t)(s->bad_frames > 0xFFFFu ? 0xFFFFu : s->bad_frames));
    protocol_put_u16(out + 32, (uint16_t)(s->loop_max_us > 0xFFFFu ? 0xFFFFu : s->loop_max_us));
    out[34] = (uint8_t)s->battery_mv.source;
    out[35] = (uint8_t)s->current_ma.source;
}

void snapshot_encode_health(const Snapshot *s, uint8_t out[SNAPSHOT_HEALTH_BYTES])
{
    memset(out, 0, SNAPSHOT_HEALTH_BYTES);
    protocol_put_u32(out + 0, s->good_frames);
    protocol_put_u32(out + 4, s->bad_frames + s->crc_errors);
    protocol_put_u32(out + 8, s->timeouts);
    protocol_put_u16(out + 12, (uint16_t)(s->loop_max_us > 0xFFFFu ? 0xFFFFu : s->loop_max_us));
    out[14] = (uint8_t)s->state;
    out[15] = 0u;
}
