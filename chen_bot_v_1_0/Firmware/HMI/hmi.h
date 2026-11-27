#ifndef RM_HMI_H
#define RM_HMI_H

#include "devices.h"
#include <stdbool.h>
#include <stdint.h>

#define HMI_WIDTH 240u
#define HMI_HEIGHT 280u
#define HMI_STRIP_HEIGHT 12u
#define HMI_MAX_ROWS 9u

typedef enum {
    HMI_PAGE_HOME,
    HMI_PAGE_MOTOR,
    HMI_PAGE_SERVO,
    HMI_PAGE_COMM,
    HMI_PAGE_FAULTS,
    HMI_PAGE_SETTINGS,
    HMI_PAGE_COUNT
} HmiPage;

typedef enum {
    HMI_BUS_MV,
    HMI_BLE_CONNECTED,
    HMI_SYSTEM_STATE,
    HMI_CONTROL_MODE,
    HMI_POWER_ENABLED,
    HMI_ESTOP,
    HMI_MOTOR_CMD_L,
    HMI_MOTOR_CMD_R,
    HMI_MOTOR_PWM_L,
    HMI_MOTOR_PWM_R,
    HMI_MOTOR_DIRECTION_L,
    HMI_MOTOR_DIRECTION_R,
    HMI_MOTOR_ENABLED,
    HMI_MOTOR_RPM_L,
    HMI_MOTOR_RPM_R,
    HMI_SERVO_TARGET_MDEG,
    HMI_SERVO_PULSE_US,
    HMI_SERVO_CENTER_US,
    HMI_SERVO_ANGLE_MIN_MDEG,
    HMI_SERVO_ANGLE_MAX_MDEG,
    HMI_SERVO_PULSE_MIN_US,
    HMI_SERVO_PULSE_MAX_US,
    HMI_SERVO_POSITION_MDEG,
    HMI_PC_CONNECTED,
    HMI_VALID_FRAMES,
    HMI_CRC_ERRORS,
    HMI_COMM_TIMEOUTS,
    HMI_LAST_COMMAND_MS,
    HMI_POWER_COMMAND,
    HMI_CURRENT_MA,
    HMI_RELAY_FEEDBACK,
    HMI_FAULT_CODE,
    HMI_SYSTEM_ERROR,
    HMI_PARAM_VIEW,
    HMI_PARAM_EDIT,
    HMI_PARAM_RESET,
    HMI_PARAM_SAVE,
    HMI_VERSION,
    HMI_UPTIME_S,
    HMI_TASK_HEALTH,
    HMI_FIELD_COUNT
} HmiField;

/* A missing, stale or unavailable reading is valid=false.
 * Integer units follow the field suffix. Command/PWM fields use permille,
 * directions use -1/0/+1, and firmware version encodes major*100+minor.
 * Booleans are 0/1; system state and authority follow Control/safety.h.
 */
typedef struct {
    int32_t value;
    bool valid;
} HmiValue;

typedef struct {
    HmiValue fields[HMI_FIELD_COUNT];
} HmiSnapshot;

typedef struct {
    void *context;
    /* Render a rectangular span of row-major RGB565 with high byte first. */
    bool (*blit)(void *context, uint16_t x, uint16_t y, uint16_t width,
                 uint16_t height, const uint8_t *pixels);
} HmiDisplay;

typedef struct {
    HmiPage page;
    uint8_t selected;
    bool detail;
    HmiPage drawn_page;
    uint8_t drawn_selection;
    bool drawn_detail;
    uint8_t next_band;
    uint16_t pending;
    HmiValue drawn_values[HMI_MAX_ROWS];
    uint8_t strip[HMI_WIDTH * HMI_STRIP_HEIGHT * 2u];
} Hmi;

void hmi_init(Hmi *hmi);
void hmi_handle_key(Hmi *hmi, Key key);
/* Paints at most one dirty screen band. Call periodically from one task. */
bool hmi_render(Hmi *hmi, const HmiDisplay *display, const HmiSnapshot *snapshot);
bool hmi_has_pending(const Hmi *hmi);

#endif
