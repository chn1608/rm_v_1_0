#ifndef RM_PROTOCOL_H
#define RM_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RM_PAYLOAD_MAX 64u
#define RM_FRAME_MAX (RM_PAYLOAD_MAX + 9u)
#define RM_VERSION 0x10u
enum { MSG_PING = 1, MSG_STATUS = 2, MSG_PARAM_GET = 3, MSG_PARAM_SET = 4,
       MSG_STOP = 5, MSG_STATUS_REPLY = 0x82, MSG_PARAM_REPLY = 0x83, MSG_ACK = 0x90 };
typedef struct {
    uint8_t type, length;
    uint16_t sequence;
    uint8_t payload[RM_PAYLOAD_MAX];
} Frame;
typedef struct {
    uint8_t data[RM_FRAME_MAX];
    unsigned used;
    uint32_t last_byte_ms, good, bad, expired;
} Parser;
typedef void (*FrameHandler)(void *context, const Frame *frame);

uint16_t protocol_crc(const uint8_t *data, size_t length);
size_t protocol_encode(const Frame *f, uint8_t *out, size_t cap);
void protocol_feed(Parser *p, uint8_t byte, uint32_t now, FrameHandler handler, void *context);
uint16_t protocol_u16(const uint8_t *data);
void protocol_put_u16(uint8_t *data, uint16_t value);
void protocol_put_u32(uint8_t *data, uint32_t value);
#endif
