#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board_config.h"
#include "chassis.h"
#include "debug_service.h"
#include "keys.h"
#include "params.h"
#include "pid.h"
#include "profile.h"
#include "protocol.h"
#include "safety.h"
#include "servo_ctrl.h"
#include "snapshot.h"

/* ------------------------------------------------------------------ frames */

static Frame captured;
static unsigned frames;
static void on_frame(void *context, const Frame *frame)
{
    (void)context;
    captured = *frame;
    ++frames;
}

static void test_protocol(void)
{
    Frame f = {.type = MSG_STATUS, .sequence = 0x1234u, .length = 3u,
               .payload = {0x11u, 0x22u, 0x33u}};
    uint8_t wire[PROTOCOL_FRAME_MAX];
    const size_t size = protocol_encode(&f, wire, sizeof(wire));
    assert(size == 12u);
    assert(wire[0] == 0xA5u && wire[1] == 0x5Au && wire[2] == PROTOCOL_VERSION);
    assert(protocol_crc(wire + 2, size - 4u) == protocol_u16(wire + size - 2u));

    /* Byte-at-a-time feed must reassemble exactly one frame. */
    Parser parser = {0};
    frames = 0;
    for (size_t i = 0; i < size; ++i) protocol_feed(&parser, wire[i], 10u + (uint32_t)i, on_frame, NULL);
    assert(frames == 1u);
    assert(captured.type == MSG_STATUS && captured.length == 3u && captured.sequence == 0x1234u);
    assert(memcmp(captured.payload, f.payload, 3u) == 0);
    assert(parser.good == 1u);

    /* Leading noise is skipped without being counted as a damaged frame. */
    parser = (Parser){0};
    frames = 0;
    const uint8_t noise[] = {0x00u, 0xA5u, 0x11u, 0xFFu};
    for (size_t i = 0; i < sizeof(noise); ++i) protocol_feed(&parser, noise[i], 100u, on_frame, NULL);
    for (size_t i = 0; i < size; ++i) protocol_feed(&parser, wire[i], 200u + (uint32_t)i, on_frame, NULL);
    assert(frames == 1u && parser.good == 1u && parser.bad == 0u);

    /* Corrupted CRC must be rejected, not delivered. */
    parser = (Parser){0};
    frames = 0;
    uint8_t broken[sizeof(wire)];
    memcpy(broken, wire, size);
    broken[size - 1u] ^= 0xFFu;
    for (size_t i = 0; i < size; ++i) protocol_feed(&parser, broken[i], 300u + (uint32_t)i, on_frame, NULL);
    assert(frames == 0u && parser.bad > 0u);

    /* Two frames back to back, plus a truncated one that must expire. */
    parser = (Parser){0};
    frames = 0;
    for (size_t i = 0; i < size; ++i) protocol_feed(&parser, wire[i], 400u, on_frame, NULL);
    for (size_t i = 0; i < size; ++i) protocol_feed(&parser, wire[i], 400u, on_frame, NULL);
    assert(frames == 2u);
    protocol_feed(&parser, wire[0], 500u, on_frame, NULL);
    protocol_feed(&parser, wire[1], 500u + CHEN_FRAME_TIMEOUT_MS + 1u, on_frame, NULL);
    assert(parser.expired == 1u);
}

/* ------------------------------------------------------------------ params */

static void test_params(void)
{
    Params p;
    params_defaults(&p);
    assert(params_self_check(&p));

    int32_t value = 0;
    assert(params_get(&p, PARAM_TELEMETRY_MS, &value) && value == (int32_t)CHEN_TELEMETRY_PERIOD_MS);
    assert(!params_get(&p, PARAM_COUNT, &value));

    /* Runtime values may change while moving, safety values may not. */
    assert(params_set(&p, PARAM_TELEMETRY_MS, 100, false));
    assert(!params_set(&p, PARAM_MOTOR_LIMIT, 500, false));
    assert(params_set(&p, PARAM_MOTOR_LIMIT, 500, true));
    assert(!params_set(&p, PARAM_MOTOR_LIMIT, 5000, true));

    /* Coupled servo limits are rejected when they would invert the travel. */
    assert(!params_set(&p, PARAM_SERVO_MIN_US, 1600, true));
    assert(params_set(&p, PARAM_SERVO_MIN_US, 600, true));
    assert(params_self_check(&p));

    /* Corruption in the block is detected by the CRC. */
    Params bad = p;
    bad.value[PARAM_TELEMETRY_MS] = 999;
    assert(!params_self_check(&bad));

    params_reset(&p);
    assert(p.revision == 0u);
    assert(p.value[PARAM_MOTOR_LIMIT] == BOARD_MOTOR_LIMIT_PERMILLE);
}

/* ------------------------------------------------------------------ safety */

static SafetyInputs healthy(void)
{
    SafetyInputs in = {0};
    in.interlocks_valid = true;
    in.power_ok = true;
    in.ble_link = true;
    return in;
}

static void test_safety(void)
{
    Safety s;
    const SafetyInputs in = healthy();
    safety_begin(&s);
    assert(s.state == SELF_TEST);
    safety_self_test(&s, true);
    assert(s.state == STANDBY);

    /* Arming needs a local acknowledgement and returns a new session. */
    assert(!safety_arm(&s, in, OWNER_BLE, false, 100u));
    assert(safety_arm(&s, in, OWNER_BLE, true, 100u));
    assert(s.state == REMOTE_ACTIVE && s.owner == OWNER_BLE);
    const uint32_t session = s.session;

    assert(safety_motion(&s, in, OWNER_BLE, session, 1u, 300, -300, 110u));
    assert(s.left == 300 && s.right == -300);
    /* Duplicate and stale sequence numbers are dropped. */
    assert(!safety_motion(&s, in, OWNER_BLE, session, 1u, 400, 400, 120u));
    assert(s.left == 300);
    /* Out of range commands are refused. */
    assert(!safety_motion(&s, in, OWNER_BLE, session, 2u, 2000, 0, 130u));
    /* A wrong session cannot steer the robot. */
    assert(!safety_motion(&s, in, OWNER_BLE, session + 1u, 3u, 100, 100, 140u));

    /* Link loss trips a latched fault and drops authority. */
    SafetyInputs lost = healthy();
    lost.ble_link = false;
    safety_step(&s, lost, 150u);
    assert(s.state == FAULT && (s.faults & FAULT_LINK) != 0u);
    assert(s.owner == OWNER_NONE && !safety_active(&s));

    /* Clear needs a local ack, then a fresh arm gives a new session. */
    assert(safety_clear(&s, lost, false) == false);
    assert(safety_clear(&s, healthy(), true));
    assert(s.state == SELF_TEST && s.faults == 0u);
    safety_self_test(&s, true);
    assert(safety_arm(&s, in, OWNER_LOCAL, true, 200u));
    assert(s.session == session + 1u);

    /* Motion timeout while armed. */
    safety_motion(&s, in, OWNER_LOCAL, s.session, 7u, 100, 100, 210u);
    safety_step(&s, in, 210u + CHEN_MOTION_TIMEOUT_MS);
    assert((s.faults & FAULT_TIMEOUT) != 0u);
    assert(s.state == FAULT);
}

/* ----------------------------------------------------------------- chassis */

static void test_chassis(void)
{
    Chassis c;
    chassis_init(&c, 1000, 100, 5u);
    assert(chassis_update(&c, 0, 0, true));
    assert(chassis_wheel(&c, 0) == 0 && chassis_wheel(&c, 1) == 0);

    /* Straight line ramps towards the command. */
    for (unsigned i = 0; i < 20u; ++i) assert(chassis_update(&c, 500, 0, true));
    assert(chassis_wheel(&c, 0) == 500 && chassis_wheel(&c, 1) == 500);

    /* Right turn: left wheel above right wheel, never above the limit. */
    for (unsigned i = 0; i < 20u; ++i) assert(chassis_update(&c, 800, 400, true));
    assert(chassis_wheel(&c, 0) <= 1000 && chassis_wheel(&c, 1) >= -1000);
    assert(chassis_wheel(&c, 0) > chassis_wheel(&c, 1));

    /* Reversal must pass through zero and wait before changing direction. */
    chassis_init(&c, 1000, 200, 3u);
    for (unsigned i = 0; i < 10u; ++i) chassis_update(&c, 600, 0, true);
    assert(chassis_wheel(&c, 0) > 0);
    chassis_update(&c, -600, 0, true);          /* commanded reversal */
    assert(chassis_wheel(&c, 0) >= 0);          /* still stopping */
    for (unsigned i = 0; i < 4u; ++i) chassis_update(&c, -600, 0, true);
    assert(chassis_wheel(&c, 0) == 0);          /* holding at zero */
    for (unsigned i = 0; i < 10u; ++i) chassis_update(&c, -600, 0, true);
    assert(chassis_wheel(&c, 0) < 0);

    /* Without permission the wheels are forced to zero. */
    assert(chassis_update(&c, 500, 0, false));
    assert(chassis_wheel(&c, 0) == 0 && chassis_wheel(&c, 1) == 0);
}

/* -------------------------------------------------------------------- servo */

static void test_servo(void)
{
    ServoCal cal = {.min_mdeg = -90000, .center_mdeg = 0, .max_mdeg = 90000,
                    .min_us = 500, .center_us = 1500, .max_us = 2500};
    assert(servo_cal_valid(&cal));
    ServoCtrl s;
    assert(servo_init(&s, &cal, 8u));
    assert(servo_output_us(&s) == 1500u);

    assert(servo_target(&s, 45000));
    assert(servo_command_mdeg(&s) == 45000);
    /* Slew limited: one step moves at most 8 us. */
    servo_step(&s);
    assert(servo_output_us(&s) == 1508u);
    for (unsigned i = 0; i < 200u; ++i) servo_step(&s);
    assert(servo_output_us(&s) == 2000u);

    /* Angle commands are clamped to the mechanical limits. */
    assert(servo_target(&s, 999999));
    assert(servo_command_mdeg(&s) == 90000);
    assert(servo_target(&s, -999999));
    assert(servo_command_mdeg(&s) == -90000);

    ServoCal inverted = cal;
    inverted.center_us = 400u;
    assert(!servo_cal_valid(&inverted));
    assert(!servo_init(&s, &inverted, 8u));
    assert(servo_output_us(&s) == 0u);
}

/* ---------------------------------------------------------------------- pid */

static void test_pid(void)
{
    Pid pid;
    assert(pid_configure(&pid, 1000, 0, 0, -1000, 1000));
    assert(pid_update(&pid, 100, 0, 10u) == 100);
    assert(pid_update(&pid, 0, 0, 10u) == 0);

    /* Output saturation clamps and the integral does not wind up beyond it. */
    assert(pid_configure(&pid, 5000, 5000, 0, -100, 100));
    for (unsigned i = 0; i < 100u; ++i) assert(pid_update(&pid, 1000, 0, 10u) == 100);
    assert(pid_update(&pid, 0, 0, 10u) >= -100);

    /* Bad configuration is refused. */
    assert(!pid_configure(&pid, 1, 1, 1, 10, 10));
    assert(!pid_configure(&pid, -1, 0, 0, -10, 10));
}

/* --------------------------------------------------------------------- keys */

static void test_keys(void)
{
    /* Official DM-MC02 windows (raw 12-bit counts). */
    assert(key_classify_raw(0) == KEY_CENTER);
    assert(key_classify_raw(100) == KEY_CENTER);
    assert(key_classify_raw(816) == KEY_RIGHT);
    assert(key_classify_raw(1635) == KEY_LEFT);
    assert(key_classify_raw(2457) == KEY_UP);
    assert(key_classify_raw(3279) == KEY_DOWN);
    assert(key_classify_raw(3300) == KEY_DOWN);   /* inside the down window */
    assert(key_classify_raw(4000) == KEY_NONE);    /* released is full scale */
    /* Values in the gaps between windows are released, never a stray key. */
    assert(key_classify_raw(400) == KEY_NONE);
    assert(key_classify_raw(1200) == KEY_NONE);
    assert(key_classify_raw(2000) == KEY_NONE);
    assert(key_classify_raw(2650) == KEY_NONE);

    /* Time based debounce: a level must hold CHEN_KEY_STABLE_MS before it is
     * accepted, and the RC ramp after release must not create extra keys. */
    KeyFilter f = {0};
    Key event = KEY_NONE;
    assert(!key_update(&f, KEY_UP, 0u, &event));
    assert(!key_update(&f, KEY_UP, 10u, &event));
    assert(!key_update(&f, KEY_UP, 20u, &event));
    assert(key_update(&f, KEY_UP, 50u, &event) && event == KEY_UP);
    assert(!key_update(&f, KEY_UP, 200u, &event));          /* held: no repeat */

    /* Release ramp through the down rung must not be reported. */
    assert(!key_update(&f, KEY_DOWN, 205u, &event));
    assert(!key_update(&f, KEY_DOWN, 210u, &event));
    assert(!key_update(&f, KEY_DOWN, 215u, &event));
    assert(!key_update(&f, KEY_NONE, 220u, &event));
    assert(!key_update(&f, KEY_NONE, 230u, &event));
    assert(key_update(&f, KEY_NONE, 300u, &event) && event == KEY_NONE);
}

/* ---------------------------------------------------------------- snapshot */

static void test_snapshot(void)
{
    Snapshot s;
    snapshot_clear(&s);
    s.state = REMOTE_ACTIVE;
    s.owner = OWNER_BLE;
    s.left_cmd = -250;
    s.right_cmd = 250;
    s.servo_us = 1750u;
    s.servo_mdeg = -45000;
    s.battery_mv = (Reading){15100, 0, DATA_MEASURED, true};
    s.faults = FAULT_LINK | FAULT_TIMEOUT;
    s.uptime_s = 42u;
    s.good_frames = 7u;
    s.bad_frames = 3u;
    s.ble_link = true;

    uint8_t out[SNAPSHOT_STATUS_BYTES];
    snapshot_encode_status(&s, out);
    assert(out[0] == 1u);
    assert(out[1] == (uint8_t)REMOTE_ACTIVE);
    assert(out[2] == (uint8_t)OWNER_BLE);
    assert((out[3] & 0x10u) != 0u);                  /* ble link flag */
    assert(protocol_u16(out + 12) == 1750u);
    assert(protocol_u16(out + 14) == (uint16_t)(int16_t)(-4500));  /* decidegrees */
    assert(protocol_u16(out + 16) == 15100u);
    assert(protocol_u32(out + 20) == (FAULT_LINK | FAULT_TIMEOUT));
    assert(protocol_u32(out + 24) == 42u);
    assert(protocol_u16(out + 28) == 7u);
    assert(protocol_u16(out + 30) == 3u);

    uint8_t health[SNAPSHOT_HEALTH_BYTES];
    snapshot_encode_health(&s, health);
    assert(protocol_u32(health) == 7u);
    assert(protocol_u32(health + 4) == 3u);
}

/* ------------------------------------------------------------ debug service */

static void test_debug(void)
{
    Params params;
    params_defaults(&params);
    Safety safety;
    safety_begin(&safety);
    safety_self_test(&safety, true);
    Snapshot snap;
    snapshot_clear(&snap);
    snap.now_ms = 1000u;

    DebugService svc;
    debug_init(&svc, &params, &safety, &snap);

    Frame request = {0}, reply = {0};

    /* Status is available to every source. */
    request.type = MSG_STATUS;
    request.sequence = 1u;
    debug_handle(&svc, OWNER_PC, &request, &reply);
    assert(reply.type == MSG_STATUS_REPLY && reply.length == SNAPSHOT_STATUS_BYTES);

    /* Version carries the profile and the protocol revision. */
    request.type = MSG_VERSION;
    debug_handle(&svc, OWNER_PC, &request, &reply);
    assert(reply.type == MSG_VERSION_REPLY && reply.payload[4] == PROTOCOL_VERSION);

    /* Parameter read works for the PC, writing requires maintenance authority. */
    request.type = MSG_PARAM_GET;
    request.length = 1u;
    request.payload[0] = (uint8_t)PARAM_TELEMETRY_MS;
    debug_handle(&svc, OWNER_PC, &request, &reply);
    assert(reply.type == MSG_PARAM_REPLY);

    request.type = MSG_PARAM_SET;
    request.length = 5u;
    request.payload[0] = (uint8_t)PARAM_TELEMETRY_MS;
    protocol_put_u32(request.payload + 1, 100u);
    debug_handle(&svc, OWNER_PC, &request, &reply);
    assert(reply.type == MSG_NAK);
    svc.maintenance = true;
    debug_handle(&svc, OWNER_PC, &request, &reply);
    assert(reply.type == MSG_ACK);

    /* The PC may not drive the motors even when armed. */
    request.type = MSG_MOTION;
    request.length = 4u;
    debug_handle(&svc, OWNER_PC, &request, &reply);
    assert(reply.type == MSG_NAK);

    /* BLE cannot arm without local confirmation (PARAM_ARM_REQUIRE_LOCAL=1). */
    request.type = MSG_ARM;
    request.length = 0u;
    debug_handle(&svc, OWNER_BLE, &request, &reply);
    assert(reply.type == MSG_NAK);

    /* Once local authority arms the robot, BLE motion is refused (wrong owner). */
    SafetyInputs in = healthy();
    assert(safety_arm(&safety, in, OWNER_LOCAL, true, snap.now_ms));
    request.type = MSG_MOTION;
    request.length = 4u;
    protocol_put_u16(request.payload, 100u);
    protocol_put_u16(request.payload + 2, 100u);
    debug_handle(&svc, OWNER_BLE, &request, &reply);
    assert(reply.type == MSG_NAK);

    /* Stop is accepted from the owner. */
    request.type = MSG_STOP;
    request.length = 0u;
    debug_handle(&svc, OWNER_LOCAL, &request, &reply);
    assert(reply.type == MSG_ACK);
    assert(safety.owner == OWNER_NONE && !safety_active(&safety));
}

int main(void)
{
    test_protocol();
    test_params();
    test_safety();
    test_chassis();
    test_servo();
    test_pid();
    test_keys();
    test_snapshot();
    test_debug();
    puts("chen_bot_v_1_0: protocol, params, safety, chassis, servo, pid, keys, snapshot, debug passed");
    return 0;
}
