/* chen_bot_v_1_0 application core.
 *
 * Task layout (see docs/FREERTOS_TASKS.md):
 *   control    5 ms   safety step, motion shaping, actuator output, snapshot
 *   comm       5 ms   PC and BLE receive, protocol dispatch, replies
 *   hmi       20 ms   five-way key scan, one display band per tick
 *   telemetry 50 ms   periodic status frame to the PC link
 *   diag       1 s    uptime, loop timing, health counters
 *
 * The control task is the only writer of the shared snapshot and the only task
 * that touches the actuators. Everything else observes or requests.
 */
#include "app.h"
#include "board.h"
#include "board_config.h"
#include "chassis.h"
#include "debug_service.h"
#include "features.h"
#include "faults.h"
#include "hmi.h"
#include "keys.h"
#include "lcd.h"
#include "profile.h"
#include "protocol.h"
#include "servo_ctrl.h"

#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

#include <string.h>


static struct {
    Devices devices;
    Params params;
    Safety safety;
    Snapshot snapshot;
    DebugService debug;

    Chassis chassis;
    ServoCtrl servo;
    Hmi hmi;
    KeyFilter keys;
    bool panel_ready;
    bool backlight;

    Transport pc;
    Transport ble;
    Parser pc_parser;
    Parser ble_parser;
    uint32_t last_pc_rx_ms;
    uint32_t last_ble_rx_ms;
    bool pc_ready;
    bool ble_ready;

    StaticTask_t control_tcb, comm_tcb, hmi_tcb, telemetry_tcb, diag_tcb;
    StackType_t control_stack[CHEN_STACK_CONTROL];
    StackType_t comm_stack[CHEN_STACK_COMM];
    StackType_t hmi_stack[CHEN_STACK_HMI];
    StackType_t telemetry_stack[CHEN_STACK_TELEMETRY];
    StackType_t diag_stack[CHEN_STACK_DIAG];
    TaskHandle_t control_handle;
    volatile uint32_t loop_start_us;
    volatile uint32_t loop_max_us;
} app;

/* ------------------------------------------------------------- snapshot IO */

void app_snapshot(Snapshot *out)
{
    taskENTER_CRITICAL();
    *out = app.snapshot;
    taskEXIT_CRITICAL();
}

Safety *app_safety(void) { return &app.safety; }
Params *app_params(void) { return &app.params; }

static void publish(const Snapshot *s)
{
    taskENTER_CRITICAL();
    app.snapshot = *s;
    taskEXIT_CRITICAL();
}

/* A link is considered healthy when a frame arrived inside the configured
 * window; unplugged or silent peers must not hold authority forever. */
static bool link_recent(uint32_t last_ms, uint32_t now, uint32_t window_ms)
{
    return last_ms != 0u && now - last_ms < window_ms;
}

/* ---------------------------------------------------------------- dispatch */

static void reply_on(Transport *link, const Frame *reply)
{
    if (link->write == NULL) return;
    uint8_t wire[PROTOCOL_FRAME_MAX];
    const size_t size = protocol_encode(reply, wire, sizeof(wire));
    if (size == 0u) return;
    if (link->write(link->context, wire, (unsigned)size) != DEV_OK) ++app.snapshot.dropped_tx;
}

/* The protocol parser needs a callback; route it through a per-link context so
 * the same function can build the reply for either transport. */
typedef struct {
    Owner source;
    Transport *link;
    uint32_t now;
} DispatchContext;

static void on_frame(void *context, const Frame *request)
{
    DispatchContext *ctx = context;

    Frame reply;
    debug_handle(&app.debug, ctx->source, request, &reply);
    reply_on(ctx->link, &reply);

    if (request->type == MSG_STOP || request->type == MSG_DISARM) {
        /* Any stop request immediately drops authority, whatever the reply. */
        safety_stop(&app.safety);
    }
}

static void pump(Transport *link, Parser *parser, Owner source, uint32_t now)
{
    if (link->read == NULL) return;
    uint8_t buffer[64];
    unsigned used = 0u;
    const DeviceResult result = link->read(link->context, buffer, sizeof(buffer), &used);
    if (result == DEV_IO) {
        protocol_parser_reset(parser);
        ++app.snapshot.crc_errors;
        return;
    }
    if (result != DEV_OK || used == 0u) return;

    DispatchContext ctx = {.source = source, .link = link, .now = now};
    for (unsigned i = 0; i < used; ++i) protocol_feed(parser, buffer[i], now, on_frame, &ctx);
}

/* ----------------------------------------------------------------- control */

static SafetyInputs gather_inputs(uint32_t now)
{
    SafetyInputs in = {0};
    in.interlocks_valid = true;          /* no interlock chain fitted yet */
    in.ble_link = app.ble_ready && link_recent(app.last_ble_rx_ms, now,
                                               (uint32_t)app.params.value[PARAM_LINK_TIMEOUT_MS]);
    in.power_ok = true;
    if (app.snapshot.battery_mv.valid) {
        const int32_t mv = app.snapshot.battery_mv.value;
        in.power_ok = mv >= app.params.value[PARAM_VBAT_MIN_MV] &&
                      mv <= app.params.value[PARAM_VBAT_MAX_MV];
    }
#if CHEN_FEATURE_ESTOP_INPUT
    Reading estop;
    if ((app.devices.voltage != NULL) &&
        app.devices.voltage(app.devices.context, 1u, &estop) == DEV_OK && estop.valid) {
        in.estop = estop.value < 500;    /* contact open -> asserted */
    }
#endif
    return in;
}

static void control_step(uint32_t now)
{
    Snapshot snap = app.snapshot;
    snap.now_ms = now;
    snap.uptime_s = now / 1000u;
    snap.battery_mv = (Reading){0, now, DATA_ABSENT, false};
    if (app.devices.voltage != NULL) {
        (void)app.devices.voltage(app.devices.context, 0u, &snap.battery_mv);
    }

    const SafetyInputs inputs = gather_inputs(now);
    safety_step(&app.safety, inputs, now);

    const bool armed = safety_active(&app.safety) && CHEN_REAL_OUTPUTS;
    const int16_t target_left = armed ? app.safety.left : 0;
    const int16_t target_right = armed ? app.safety.right : 0;
    chassis_set_wheels(&app.chassis, target_left, target_right, true);

    const int16_t left = chassis_wheel(&app.chassis, 0u);
    const int16_t right = chassis_wheel(&app.chassis, 1u);
    if (app.devices.motor != NULL) {
        (void)app.devices.motor(app.devices.context, left, right);
    }

    uint16_t pulse = 0u;
    if (armed && app.safety.servo_us != 0u) {
        pulse = app.safety.servo_us;
        (void)servo_target(&app.servo, (int32_t)(pulse - BOARD_SERVO_CENTER_US) * 180000 /
                                          (int32_t)(BOARD_SERVO_MAX_US - BOARD_SERVO_MIN_US));
    } else {
        (void)servo_target(&app.servo, BOARD_SERVO_CENTER_MDEG);
    }
    servo_step(&app.servo);
    pulse = servo_output_us(&app.servo);
    if (app.devices.servo != NULL && CHEN_REAL_OUTPUTS) {
        (void)app.devices.servo(app.devices.context, pulse);
    }

    const bool want_power = armed;
    if (app.devices.power != NULL) {
        if (app.devices.power(app.devices.context, want_power) != DEV_OK && want_power) {
            safety_trip(&app.safety, FAULT_DEVICE);
        }
    }

    snap.state = app.safety.state;
    snap.owner = app.safety.owner;
    snap.faults = app.safety.faults;
    snap.session = app.safety.session;
    snap.arm_count = app.safety.arm_count;
    snap.motion_permitted = app.safety.motion_permitted && armed;
    snap.left_cmd = app.safety.left;
    snap.right_cmd = app.safety.right;
    snap.left_out = left;
    snap.right_out = right;
    snap.servo_us = pulse;
    snap.servo_mdeg = servo_command_mdeg(&app.servo);
    snap.power_enabled = want_power;
    snap.ble_link = inputs.ble_link;
    snap.pc_link = app.pc_ready && link_recent(app.last_pc_rx_ms, now,
                                               (uint32_t)app.params.value[PARAM_LINK_TIMEOUT_MS]);
    snap.good_frames = app.pc_parser.good + app.ble_parser.good;
    snap.bad_frames = app.pc_parser.bad + app.ble_parser.bad;
    snap.timeouts = app.pc_parser.expired + app.ble_parser.expired;
    snap.loop_max_us = app.loop_max_us;
    publish(&snap);
}

/* -------------------------------------------------------------------- tasks */

static void control_task(void *argument)
{
    (void)argument;
    TickType_t tick = xTaskGetTickCount();
    for (;;) {
        const uint32_t start = HAL_GetTick();
        control_step(start);
        const uint32_t elapsed = (HAL_GetTick() - start) * 1000u;
        if (elapsed > app.loop_max_us) app.loop_max_us = elapsed;
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(CHEN_CONTROL_PERIOD_MS));
    }
}

static void comm_task(void *argument)
{
    (void)argument;
    TickType_t tick = xTaskGetTickCount();
    for (;;) {
        const uint32_t now = HAL_GetTick();
        pump(&app.pc, &app.pc_parser, OWNER_PC, now);
        pump(&app.ble, &app.ble_parser, OWNER_BLE, now);
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(CHEN_COMM_PERIOD_MS));
    }
}

static HmiValue value_of(int32_t value, bool valid)
{
    const HmiValue v = {value, valid};
    return v;
}

static void fill_hmi(HmiSnapshot *out)
{
    Snapshot snap;
    app_snapshot(&snap);
    memset(out, 0, sizeof(*out));
    out->fields[HMI_BUS_MV] = value_of(snap.battery_mv.value, snap.battery_mv.valid);
    out->fields[HMI_BLE_CONNECTED] = value_of(snap.ble_link ? 1 : 0, true);
    out->fields[HMI_SYSTEM_STATE] = value_of((int32_t)snap.state, true);
    out->fields[HMI_CONTROL_MODE] = value_of((int32_t)snap.owner, true);
    out->fields[HMI_POWER_ENABLED] = value_of(snap.power_enabled ? 1 : 0, true);
    out->fields[HMI_ESTOP] = value_of((snap.faults & FAULT_ESTOP) != 0u ? 1 : 0, true);
    out->fields[HMI_FAULT_CODE] = value_of((int32_t)snap.faults, true);
    out->fields[HMI_MOTOR_CMD_L] = value_of(snap.left_cmd, true);
    out->fields[HMI_MOTOR_CMD_R] = value_of(snap.right_cmd, true);
    out->fields[HMI_MOTOR_PWM_L] = value_of(snap.left_out, true);
    out->fields[HMI_MOTOR_PWM_R] = value_of(snap.right_out, true);
    out->fields[HMI_SERVO_TARGET_MDEG] = value_of(snap.servo_mdeg, true);
    out->fields[HMI_SERVO_PULSE_US] = value_of(snap.servo_us, true);
    out->fields[HMI_PC_CONNECTED] = value_of(snap.pc_link ? 1 : 0, true);
    out->fields[HMI_VALID_FRAMES] = value_of((int32_t)snap.good_frames, true);
    out->fields[HMI_CRC_ERRORS] = value_of((int32_t)snap.bad_frames, true);
    out->fields[HMI_COMM_TIMEOUTS] = value_of((int32_t)snap.timeouts, true);
    out->fields[HMI_VERSION] = value_of(100, true);
    out->fields[HMI_UPTIME_S] = value_of((int32_t)snap.uptime_s, true);
    out->fields[HMI_TASK_HEALTH] = value_of(snap.loop_max_us < CHEN_TASK_DEADLINE_MS * 1000u, true);
}

static bool panel_blit(void *context, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       const uint8_t *pixels)
{
    (void)context;
    return smoke_panel_blit565(x, y, w, h, pixels);
}

static void on_key(Key key)
{
    if (key < KEY_CENTER || key > KEY_DOWN) return;
    hmi_handle_key(&app.hmi, key);
}

static void hmi_task(void *argument)
{
    (void)argument;
    TickType_t tick = xTaskGetTickCount();
    static const HmiDisplay display = {NULL, panel_blit};

    for (;;) {
        if (!app.panel_ready) {
            app.panel_ready = smoke_panel_init();
            if (app.panel_ready) hmi_init(&app.hmi);
        }
        if (app.panel_ready) {
            Key sample = KEY_UNKNOWN;
            if (app.devices.keys != NULL) {
                (void)app.devices.keys(app.devices.context, &sample);
            }
            Key event;
            if (key_update(&app.keys, sample, &event)) on_key(event);

            HmiSnapshot hmi_snapshot;
            fill_hmi(&hmi_snapshot);
            if (!hmi_render(&app.hmi, &display, &hmi_snapshot)) {
                app.panel_ready = false;
                smoke_panel_set_backlight(false);
            } else if (!app.backlight && !hmi_has_pending(&app.hmi)) {
                smoke_panel_set_backlight(true);
                app.backlight = true;
            }
        }
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(CHEN_HMI_PERIOD_MS));
    }
}

static void telemetry_task(void *argument)
{
    (void)argument;
    TickType_t tick = xTaskGetTickCount();
    for (;;) {
        if (app.pc_ready) {
            Snapshot snap;
            app_snapshot(&snap);
            Frame frame = {0};
            frame.type = MSG_STATUS_REPLY;
            frame.length = (uint8_t)SNAPSHOT_STATUS_BYTES;
            snapshot_encode_status(&snap, frame.payload);
            reply_on(&app.pc, &frame);
        }
        vTaskDelayUntil(&tick, pdMS_TO_TICKS((TickType_t)app.params.value[PARAM_TELEMETRY_MS]));
    }
}

static void diag_task(void *argument)
{
    (void)argument;
    TickType_t tick = xTaskGetTickCount();
    for (;;) {
        /* Health bookkeeping only. The IWDG is not started until the reset
         * behaviour has been reviewed on hardware (docs/HARDWARE_TODO.md). */
        taskENTER_CRITICAL();
        app.snapshot.loop_max_us = app.loop_max_us;
        taskEXIT_CRITICAL();
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(CHEN_DIAG_PERIOD_MS));
    }
}

/* --------------------------------------------------------------------- init */

bool app_init(const Devices *devices, bool can_arm)
{
    memset(&app, 0, sizeof(app));
    app.devices = *devices;
    params_defaults(&app.params);
    safety_begin(&app.safety);

    snapshot_clear(&app.snapshot);
    app.snapshot.state = app.safety.state;
    app.snapshot.battery_mv = (Reading){0, 0, DATA_ABSENT, false};

    chassis_init(&app.chassis, (int16_t)app.params.value[PARAM_MOTOR_LIMIT],
                 (uint16_t)app.params.value[PARAM_MOTOR_RAMP],
                 (uint16_t)app.params.value[PARAM_REVERSAL_TICKS]);
    const ServoCal cal = {.min_mdeg = BOARD_SERVO_MIN_MDEG, .center_mdeg = BOARD_SERVO_CENTER_MDEG,
                          .max_mdeg = BOARD_SERVO_MAX_MDEG, .min_us = BOARD_SERVO_MIN_US,
                          .center_us = BOARD_SERVO_CENTER_US, .max_us = BOARD_SERVO_MAX_US};
    (void)servo_init(&app.servo, &cal, (uint16_t)app.params.value[PARAM_SERVO_SLEW_US]);

    app.pc = board_pc_transport();
    app.ble = board_ble_transport();
    app.pc_ready = board_pc_ready();
    app.ble_ready = board_ble_ready();
    debug_init(&app.debug, &app.params, &app.safety, &app.snapshot);

    const bool outputs_off = app.devices.power == NULL ||
                             app.devices.power(app.devices.context, false) == DEV_OK;
    /* Self test passes only when the safe output state and the link that the
     * profile needs are both available. */
    safety_self_test(&app.safety, outputs_off && can_arm);
    app.snapshot.state = app.safety.state;
    return outputs_off;
}

void app_start(void)
{
    app.control_handle = xTaskCreateStatic(control_task, "control", CHEN_STACK_CONTROL, NULL,
                                           CHEN_PRIO_CONTROL, app.control_stack,
                                           &app.control_tcb);
    (void)xTaskCreateStatic(comm_task, "comm", CHEN_STACK_COMM, NULL, CHEN_PRIO_COMM,
                            app.comm_stack, &app.comm_tcb);
    (void)xTaskCreateStatic(hmi_task, "hmi", CHEN_STACK_HMI, NULL, CHEN_PRIO_HMI,
                            app.hmi_stack, &app.hmi_tcb);
    (void)xTaskCreateStatic(telemetry_task, "telemetry", CHEN_STACK_TELEMETRY, NULL,
                            CHEN_PRIO_TELEMETRY, app.telemetry_stack, &app.telemetry_tcb);
    (void)xTaskCreateStatic(diag_task, "diag", CHEN_STACK_DIAG, NULL, CHEN_PRIO_DIAG,
                            app.diag_stack, &app.diag_tcb);
    if (app.control_handle == NULL) board_fatal();
}
