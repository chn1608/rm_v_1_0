#ifndef CHEN_SERVICE_SNAPSHOT_H
#define CHEN_SERVICE_SNAPSHOT_H
#include <stdbool.h>
#include <stdint.h>
#include "devices.h"
#include "safety.h"

/* One authoritative status structure. The LCD, the PC telemetry and the BLE
 * status reply all read this same shape so the three terminals can never
 * disagree about what the robot is doing.
 *
 * Producers update it under a lock (see snapshot_begin/end on the MCU); pure
 * consumers copy it and then read fields.
 */
typedef struct {
    uint32_t now_ms;
    uint32_t uptime_s;
    SafetyState state;
    Owner owner;
    uint32_t faults;
    uint32_t session;
    uint32_t arm_count;

    /* Commands actually handed to the actuators. */
    bool motion_permitted;
    int16_t left_cmd;        /* permille */
    int16_t right_cmd;       /* permille */
    int16_t left_out;        /* permille after ramp */
    int16_t right_out;
    uint16_t servo_us;       /* commanded pulse */
    int32_t servo_mdeg;      /* commanded angle */

    /* Measurements; check `.source` and `.valid` before displaying. */
    Reading battery_mv;
    Reading current_ma;
    Reading wheel_rpm[2];
    Reading servo_feedback_mdeg;
    /* Sampled once per control tick by the only task that owns the ADC, so the
     * HMI never touches the converter concurrently. */
    uint16_t key_raw;
    bool key_valid;
    Reading estop_mv;
    bool power_enabled;      /* commanded relay state */
    bool relay_closed[2];    /* real contact feedback, currently not fitted */

    /* Link and health counters. */
    uint32_t good_frames;
    uint32_t bad_frames;
    uint32_t crc_errors;
    uint32_t timeouts;
    uint32_t dropped_tx;
    uint32_t loop_max_us;
    bool ble_link;
    bool pc_link;
} Snapshot;

/* Fixed-size wire encodings, little endian. Layouts are documented in
 * docs/PROTOCOL.md and must stay in sync with the PC and Android decoders. */
#define SNAPSHOT_STATUS_BYTES 36u
#define SNAPSHOT_HEALTH_BYTES 16u

void snapshot_clear(Snapshot *s);
void snapshot_encode_status(const Snapshot *s, uint8_t out[SNAPSHOT_STATUS_BYTES]);
void snapshot_encode_health(const Snapshot *s, uint8_t out[SNAPSHOT_HEALTH_BYTES]);

#endif
