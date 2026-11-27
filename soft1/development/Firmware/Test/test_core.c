#include "keys.h"
#include "actuation.h"
#include "profile.h"
#include "protocol.h"
#include "service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#if RM_PROFILE == RM_SIM
#include "mock.h"
#endif

typedef struct { unsigned count; Frame last; } Capture;
static void capture(void *self, const Frame *frame)
{
    Capture *c = self;
    ++c->count;
    c->last = *frame;
}

static void feed(Parser *p, Capture *c, const uint8_t *data, size_t n, uint32_t time)
{
    for (size_t i = 0; i < n; ++i) protocol_feed(p, data[i], time, capture, c);
}

static void protocol_test(void)
{
    const Frame request = {.type = MSG_STATUS, .sequence = 0x1234};
    uint8_t data[RM_FRAME_MAX], broken[RM_FRAME_MAX];
    assert(protocol_crc((const uint8_t *)"123456789", 9u) == 0x29b1);
    assert(protocol_encode(&request, data, 0u) == 0u);
    const size_t n = protocol_encode(&request, data, sizeof(data));
    assert(n == 9u);
    Parser p = {0};
    Capture c = {0};
    assert(n == 9u && data[7] == 0x1cu && data[8] == 0x03u);
    feed(&p, &c, data, 3u, 1u);
    assert(c.count == 0u);
    feed(&p, &c, data + 3u, n - 3u, 2u);
    feed(&p, &c, data, n, 2u);
    assert(c.count == 2u && c.last.sequence == 0x1234);
    memcpy(broken, data, n);
    broken[7] ^= 1u;
    feed(&p, &c, broken, n, 9u);
    broken[4] = 65u;
    feed(&p, &c, broken, n, 9u);
    assert(p.bad == 2u && c.count == 2u);
    feed(&p, &c, data, 4u, 10u);
    feed(&p, &c, data, n, 110u);
    assert(p.expired == 1u && c.count == 3u);
    uint32_t x = 17u;
    for (unsigned i = 0; i < 300000u; ++i) {
        x = x * 1664525u + 1013904223u;
        protocol_feed(&p, (uint8_t)(x >> 24), 111u, capture, &c);
        assert(p.used < RM_FRAME_MAX);
    }
    const unsigned before = c.count;
    feed(&p, &c, data, n, 300u);
    assert(c.count == before + 1u);
}

static void safety_test(void)
{
    Safety s;
    SafetyInputs in = {.interlocks_valid = true, .ble_link = true};
    safety_begin(&s);
    assert(!s.motion_permitted && s.state == SELF_TEST);
    safety_self_test(&s, false);
    assert(s.state == FAULT && s.faults == FAULT_SELF_TEST);
    assert(!safety_clear(&s, in, false));
    assert(safety_clear(&s, in, true));
    safety_self_test(&s, true);
    assert(!safety_arm(&s, in, OWNER_PC, true, 0u));
    const SafetyInputs invalid = {.ble_link = true};
    assert(!safety_arm(&s, invalid, OWNER_BLE, true, 0u));
#if RM_PROFILE == RM_SIM
    const uint32_t start = UINT32_MAX - 10u;
    assert(!safety_arm(&s, in, OWNER_BLE, false, start));
    assert(safety_arm(&s, in, OWNER_BLE, true, start));
    const uint32_t old = s.session;
    assert(!s.motion_permitted);
    assert(!safety_arm(&s, in, OWNER_LOCAL, true, start));
    assert(!safety_motion(&s, in, OWNER_BLE, old, 65534u, 1001, 0, start));
    assert(!safety_motion(&s, in, OWNER_LOCAL, old, 65535u, 0, 0, start));
    assert(safety_motion(&s, in, OWNER_BLE, old, 65535u, 500, -500, start));
    assert(!safety_motion(&s, in, OWNER_BLE, old, 65535u, 0, 0, start + 199u));
    assert(safety_motion(&s, in, OWNER_BLE, old, 0u, 500, -500, start + 199u));
    safety_step(&s, in, start + 399u);
    assert(s.state == FAULT && s.faults == FAULT_TIMEOUT && s.left == 0);
    assert(safety_clear(&s, in, true));
    safety_self_test(&s, true);
    assert(safety_arm(&s, in, OWNER_BLE, true, 500u));
    assert(!safety_motion(&s, in, OWNER_BLE, old, 1u, 500, 0, 500u));
    in.ble_link = false;
    safety_step(&s, in, 510u);
    assert(s.state == FAULT && s.owner == OWNER_NONE && !s.motion_permitted);
    in.ble_link = true;
    safety_step(&s, in, 511u);
    assert(s.state == FAULT);
#else
    assert(!safety_arm(&s, in, OWNER_LOCAL, true, 0u));
    assert(!safety_arm(&s, in, OWNER_BLE, true, 0u));
#endif
    in.estop = true;
    safety_step(&s, in, 512u);
    assert(s.state == ESTOP && !s.motion_permitted);
    assert(!safety_clear(&s, in, true));
    in.estop = false;
    safety_step(&s, in, 513u);
    assert(s.state == ESTOP);
    assert(safety_clear(&s, in, true) && s.state == SELF_TEST);
}

static void debug_test(void)
{
    Params p;
    Safety s;
    safety_begin(&s);
    safety_self_test(&s, true);
    params_reset(&p);
    const Snapshot snap = {.now_ms = 100u, .bus_mv = {0, 100u, DATA_ABSENT, false}};
    Frame req = {.type = MSG_PARAM_SET, .length = 3u, .payload = {1u, 60u, 0u}};
    Frame reply;
    debug_handle(&p, &s, &snap, OWNER_PC, true, &req, &reply);
    assert(reply.payload[1] == DEBUG_DENIED && p.telemetry_ms == 50u);
    debug_handle(&p, &s, &snap, OWNER_LOCAL, true, &req, &reply);
    assert(reply.payload[1] == DEBUG_OK && p.telemetry_ms == 60u);
    req.payload[1] = 3u;
    debug_handle(&p, &s, &snap, OWNER_LOCAL, true, &req, &reply);
    assert(reply.payload[1] == DEBUG_RANGE && p.telemetry_ms == 60u);
    req.length = 0u;
    req.type = MSG_STATUS;
    debug_handle(&p, &s, &snap, OWNER_PC, false, &req, &reply);
    assert(reply.type == MSG_STATUS_REPLY && reply.length == 35u &&
           reply.payload[12] == DATA_ABSENT && reply.payload[13] == 0u);
}

static void key_test(void)
{
    const uint16_t samples[] = {3300u, 0u, 660u, 1320u, 1980u, 2640u};
    for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        assert(key_classify_mv(samples[i]) == (Key)i);
    }
    assert(key_classify_mv(330u) == KEY_UNKNOWN && key_classify_mv(4000u) == KEY_UNKNOWN);
    KeyFilter f = {0};
    Key event = KEY_NONE;
    assert(!key_update(&f, KEY_LEFT, &event));
    assert(!key_update(&f, KEY_UNKNOWN, &event));
    assert(!key_update(&f, KEY_LEFT, &event));
    assert(!key_update(&f, KEY_LEFT, &event));
    assert(key_update(&f, KEY_LEFT, &event) && event == KEY_LEFT);
    assert(!key_update(&f, KEY_LEFT, &event));
    assert(!key_update(&f, KEY_NONE, &event));
    assert(!key_update(&f, KEY_NONE, &event));
    assert(key_update(&f, KEY_NONE, &event) && event == KEY_NONE);
}

static size_t hex(const char *input, uint8_t *out, size_t cap)
{
    if (strcmp(input, "-") == 0) return 0u;
    const size_t length = strlen(input);
    assert(length % 2u == 0u && length / 2u <= cap);
    for (size_t i = 0; i < length / 2u; ++i) {
        unsigned value;
        assert(sscanf(input + 2u * i, "%2x", &value) == 1);
        out[i] = (uint8_t)value;
    }
    return length / 2u;
}

static void shared_vectors(const char *path)
{
    FILE *file = fopen(path, "r");
    assert(file != NULL);
    char line[320];
    unsigned cases = 0u;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] == '#' || line[0] == '\n') continue;
        unsigned kind, seq;
        char payload[129], wire[147];
        assert(sscanf(line, "%x %x %128s %146s", &kind, &seq, payload, wire) == 4);
        Frame req = {.type = (uint8_t)kind, .sequence = (uint16_t)seq};
        req.length = (uint8_t)hex(payload, req.payload, sizeof(req.payload));
        uint8_t expected[RM_FRAME_MAX], actual[RM_FRAME_MAX];
        const size_t n = hex(wire, expected, sizeof(expected));
        assert(protocol_encode(&req, actual, sizeof(actual)) == n && memcmp(expected, actual, n) == 0);
        Capture c = {0};
        Parser p = {0};
        feed(&p, &c, actual, n, 0u);
        assert(c.count == 1u && c.last.type == req.type && c.last.sequence == req.sequence &&
               c.last.length == req.length && memcmp(c.last.payload, req.payload, req.length) == 0);
        ++cases;
    }
    assert(!ferror(file) && cases != 0u);
    fclose(file);
    printf("%u shared wire vectors passed\n", cases);
}

static void actuation_test(void)
{
    WheelRamp wheel = {0};
    assert(wheel_update(&wheel, 30000, 300u, 2u, true) && wheel.value == 300);
    assert(wheel_update(&wheel, 30000, 300u, 2u, true) && wheel.value == 600);
    assert(wheel_update(&wheel, -1000, 300u, 2u, true) && wheel.value == 300);
    assert(wheel_update(&wheel, -1000, 300u, 2u, true) && wheel.value == 0);
    assert(wheel_update(&wheel, -1000, 300u, 2u, true) && wheel.value == 0);
    assert(wheel_update(&wheel, -1000, 300u, 2u, true) && wheel.value == 0);
    assert(wheel_update(&wheel, -1000, 300u, 2u, true) && wheel.value == -300);
    assert(wheel_update(&wheel, -1000, 300u, 2u, false) && wheel.value == 0);
    assert(!wheel_update(&wheel, -1000, 0u, 2u, true) && wheel.value == 0);
    ServoCalibration cfg = {-90000, 0, 60000, 1000, 1500, 1900};
    uint16_t pulse;
    assert(servo_pulse(&cfg, 0, &pulse) && pulse == 1500u);
    assert(servo_pulse(&cfg, -45000, &pulse) && pulse == 1250u);
    assert(servo_pulse(&cfg, 30000, &pulse) && pulse == 1700u);
    assert(servo_pulse(&cfg, INT32_MAX, &pulse) && pulse == 1900u);
    cfg.center_us = 2600u;
    assert(!servo_pulse(&cfg, 0, &pulse));
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    shared_vectors(argv[1]);
    protocol_test();
    safety_test();
    debug_test();
    key_test();
    actuation_test();
#if RM_PROFILE == RM_SIM
    Mock model = {0};
    const Devices d = mock_devices(&model);
    Reading r;
    assert(d.voltage(d.context, 0u, &r) == DEV_OK && r.valid && r.source == DATA_SIMULATED);
    assert(d.motors(d.context, 500, 0) == DEV_UNAVAILABLE);
    assert(d.power(d.context, true) == DEV_OK);
    assert(d.motors(d.context, 500, 0) == DEV_OK);
    assert(d.servo(d.context, 999) == DEV_INVALID);
    assert(d.servo(d.context, 1500) == DEV_OK);
    assert(d.power(d.context, false) == DEV_OK && model.left == 0 && model.pulse_us == 0);
#endif
    printf("profile %d: protocol, safety, parameters, keys, actuation and device isolation passed\n", RM_PROFILE);
    return 0;
}
