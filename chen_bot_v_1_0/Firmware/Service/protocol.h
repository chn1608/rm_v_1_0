#ifndef CHEN_SERVICE_PROTOCOL_H
#define CHEN_SERVICE_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Application layer frame shared by the STM32, the PC tool and the Android app.
 *
 *  offset  size  field
 *   0      2     header 0xA5 0x5A
 *   2      1     protocol version
 *   3      1     command id
 *   4      1     payload length (0..64)
 *   5      2     sequence number, little endian
 *   7      n     payload
 *   7+n    2     CRC-16/CCITT-FALSE over bytes 2..6+n
 *
 * The layout is unchanged from the vectors in Protocol/vectors.txt so the
 * existing Python and Kotlin implementations keep interoperating.
 */
#define PROTOCOL_VERSION 0x10u
#define PROTOCOL_PAYLOAD_MAX 64u
#define PROTOCOL_FRAME_MAX (PROTOCOL_PAYLOAD_MAX + 9u)

/* Command ids. Requests from a controller are 0x01..0x7F, slave replies set
 * bit 7 so a request can never be mistaken for a reply. */
enum {
    MSG_PING = 0x01,
    MSG_STATUS = 0x02,
    MSG_PARAM_GET = 0x03,
    MSG_PARAM_SET = 0x04,
    MSG_STOP = 0x05,
    MSG_ARM = 0x06,
    MSG_DISARM = 0x07,
    MSG_MOTION = 0x08,
    MSG_SERVO = 0x09,
    MSG_HEALTH = 0x0A,
    MSG_VERSION = 0x0B,
    MSG_FAULTS = 0x0C,

    MSG_STATUS_REPLY = 0x82,
    MSG_PARAM_REPLY = 0x83,
    MSG_MOTION_REPLY = 0x88,
    MSG_HEALTH_REPLY = 0x8A,
    MSG_VERSION_REPLY = 0x8B,
    MSG_FAULTS_REPLY = 0x8C,
    MSG_ACK = 0x90,
    MSG_NAK = 0x91
};

typedef struct {
    uint8_t type;
    uint8_t length;
    uint16_t sequence;
    uint8_t payload[PROTOCOL_PAYLOAD_MAX];
} Frame;

typedef struct {
    uint8_t data[PROTOCOL_FRAME_MAX];
    unsigned used;
    uint32_t last_byte_ms;
    uint32_t good, bad, expired;
} Parser;

typedef void (*FrameHandler)(void *context, const Frame *frame);

uint16_t protocol_crc(const uint8_t *data, size_t length);
size_t protocol_encode(const Frame *frame, uint8_t *out, size_t capacity);
void protocol_feed(Parser *parser, uint8_t byte, uint32_t now,
                   FrameHandler handler, void *context);
void protocol_parser_reset(Parser *parser);

uint16_t protocol_u16(const uint8_t *data);
void protocol_put_u16(uint8_t *data, uint16_t value);
void protocol_put_u32(uint8_t *data, uint32_t value);
uint32_t protocol_u32(const uint8_t *data);

#endif
