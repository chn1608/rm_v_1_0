#include "protocol.h"
#include "profile.h"
#include <string.h>

uint16_t protocol_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

uint32_t protocol_u32(const uint8_t *p)
{
    return (uint32_t)protocol_u16(p) | ((uint32_t)protocol_u16(p + 2) << 16);
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

/* CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final xor. */
uint16_t protocol_crc(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc = (uint16_t)(crc ^ ((uint16_t)data[i] << 8));
        for (unsigned bit = 0; bit < 8u; ++bit) {
            const uint16_t poly = (crc & 0x8000u) != 0u ? 0x1021u : 0u;
            crc = (uint16_t)((crc << 1) ^ poly);
        }
    }
    return crc;
}

size_t protocol_encode(const Frame *frame, uint8_t *out, size_t capacity)
{
    const size_t size = (size_t)frame->length + 9u;
    if (frame->length > PROTOCOL_PAYLOAD_MAX || capacity < size) return 0u;
    out[0] = 0xA5u;
    out[1] = 0x5Au;
    out[2] = PROTOCOL_VERSION;
    out[3] = frame->type;
    out[4] = frame->length;
    protocol_put_u16(out + 5, frame->sequence);
    memcpy(out + 7, frame->payload, frame->length);
    protocol_put_u16(out + size - 2u, protocol_crc(out + 2, size - 4u));
    return size;
}

void protocol_parser_reset(Parser *parser)
{
    parser->used = 0u;
    parser->last_byte_ms = 0u;
}

static void drop(Parser *parser, unsigned count)
{
    parser->used -= count;
    memmove(parser->data, parser->data + count, parser->used);
}

void protocol_feed(Parser *parser, uint8_t byte, uint32_t now,
                   FrameHandler handler, void *context)
{
    if (parser->used != 0u && now - parser->last_byte_ms >= CHEN_FRAME_TIMEOUT_MS) {
        ++parser->expired;
        parser->used = 0u;
    }
    parser->last_byte_ms = now;

    /* Never write past the buffer: a stream of header look-alikes must not be
     * able to overflow the parser state. */
    if (parser->used >= sizeof(parser->data)) {
        ++parser->bad;
        drop(parser, 1u);
    }
    parser->data[parser->used++] = byte;

    while (parser->used != 0u) {
        uint8_t *data = parser->data;
        if (data[0] != 0xA5u || (parser->used >= 2u && data[1] != 0x5Au)) {
            drop(parser, 1u);
            continue;
        }
        if (parser->used < 5u) return;

        const unsigned total = (unsigned)data[4] + 9u;
        const bool header_ok = data[2] == PROTOCOL_VERSION && total <= PROTOCOL_FRAME_MAX;
        const bool complete = parser->used >= total;
        if (!header_ok || (complete &&
                           protocol_crc(data + 2, total - 4u) != protocol_u16(data + total - 2u))) {
            ++parser->bad;
            drop(parser, 1u);
            continue;
        }
        if (!complete) return;

        Frame frame = {0};
        frame.type = data[3];
        frame.length = data[4];
        frame.sequence = protocol_u16(data + 5);
        memcpy(frame.payload, data + 7, frame.length);
        ++parser->good;
        drop(parser, total);
        handler(context, &frame);
    }
}
