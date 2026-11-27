#include "protocol.h"
#include "profile.h"
#include <string.h>

uint16_t protocol_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

void protocol_put_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

void protocol_put_u32(uint8_t *p, uint32_t value)
{
    protocol_put_u16(p, (uint16_t)value);
    protocol_put_u16(p + 2, (uint16_t)(value >> 16));
}

uint16_t protocol_crc(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xffffu;
    for (size_t i = 0; i < length; ++i) {
        crc = (uint16_t)(crc ^ ((uint16_t)data[i] << 8));
        for (unsigned b = 0; b < 8u; ++b) {
            const uint16_t poly = (crc & 0x8000u) != 0u ? 0x1021u : 0u;
            crc = (uint16_t)((crc << 1) ^ poly);
        }
    }
    return crc;
}

size_t protocol_encode(const Frame *f, uint8_t *out, size_t cap)
{
    const size_t size = (size_t)f->length + 9u;
    if (f->length > RM_PAYLOAD_MAX || cap < size) return 0u;
    out[0] = 0xa5u;
    out[1] = 0x5au;
    out[2] = RM_VERSION;
    out[3] = f->type;
    out[4] = f->length;
    protocol_put_u16(out + 5, f->sequence);
    memcpy(out + 7, f->payload, f->length);
    protocol_put_u16(out + size - 2u, protocol_crc(out + 2, size - 4u));
    return size;
}

static void drop(Parser *p, unsigned n)
{
    p->used -= n;
    memmove(p->data, p->data + n, p->used);
}

void protocol_feed(Parser *p, uint8_t byte, uint32_t now, FrameHandler handler, void *context)
{
    if (p->used != 0u && now - p->last_byte_ms >= RM_FRAME_TIMEOUT_MS) {
        ++p->expired;
        p->used = 0u;
    }
    p->last_byte_ms = now;
    /* The loop always consumes a full buffer before returning. */
    p->data[p->used++] = byte;
    while (p->used != 0u) {
        uint8_t *d = p->data;
        if (d[0] != 0xa5u || (p->used >= 2u && d[1] != 0x5au)) {
            drop(p, 1u);
            continue;
        }
        if (p->used < 5u) return;
        const unsigned total = (unsigned)d[4] + 9u;
        if (d[2] != RM_VERSION || total > RM_FRAME_MAX ||
            (p->used >= total && protocol_crc(d + 2, total - 4u) !=
                                      protocol_u16(d + total - 2u))) {
            ++p->bad;
            drop(p, 1u);
            continue;
        }
        if (p->used < total) return;
        Frame frame = {0};
        frame.type = d[3];
        frame.length = d[4];
        frame.sequence = protocol_u16(d + 5);
        memcpy(frame.payload, d + 7, frame.length);
        ++p->good;
        drop(p, total);
        handler(context, &frame);
    }
}
