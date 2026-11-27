#ifndef CHEN_BSP_BOARD_H
#define CHEN_BSP_BOARD_H
#include <stdbool.h>
#include <stdint.h>
#include "devices.h"

/* Board support: clocks, safe defaults, PWM, analog and the two serial links.
 * Nothing above this layer touches a register. */

void board_clock_init(void);
/* Drives every actuator pin to its safe state before anything else runs.
 * Called before HAL_Init so a reset during start-up cannot energise a motor. */
void board_safe_defaults(void);
void board_fatal(void) __attribute__((noreturn));

bool board_pwm_init(void);
bool board_analog_init(void);
bool board_links_init(void);

/* Permille of full scale, already limited by the caller. */
void board_motor_set(int16_t left, int16_t right);
void board_motor_off(void);
void board_servo_set(uint16_t pulse_us);
bool board_power_set(bool enable);

bool board_analog_battery(Reading *out);
bool board_analog_estop(Reading *out);
bool board_analog_key(uint16_t *millivolts);
bool board_relay_state(unsigned relay, bool *closed);

Transport board_pc_transport(void);
Transport board_ble_transport(void);
bool board_pc_ready(void);
bool board_ble_ready(void);

#endif
