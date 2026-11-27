#ifndef CHEN_CONFIG_PROFILE_H
#define CHEN_CONFIG_PROFILE_H

/* Build profiles. The profile chooses which devices are linked in, which
 * maintenance features are compiled and whether real actuation is allowed.
 *
 *   SIM   host or MCU with mock devices, never drives real outputs
 *   BENCH single-module bring-up, real buses but actuation gated
 *   RUN   vehicle build, full safety path, actuation allowed when armed
 */
#define CHEN_SIM 1
#define CHEN_BENCH 2
#define CHEN_RUN 3

#ifndef CHEN_PROFILE
#error Select CHEN_PROFILE=1 (SIM), 2 (BENCH) or 3 (RUN)
#endif
#if CHEN_PROFILE < CHEN_SIM || CHEN_PROFILE > CHEN_RUN
#error Unknown build profile
#endif

/* Scheduling periods. The control loop is the safety heartbeat; every other
 * task may slip but this one must keep its deadline. */
#define CHEN_CONTROL_PERIOD_MS 5u
#define CHEN_COMM_PERIOD_MS 5u
#define CHEN_HMI_PERIOD_MS 20u
#define CHEN_TELEMETRY_PERIOD_MS 50u
#define CHEN_DIAG_PERIOD_MS 1000u

/* Watchdog and link supervision. */
#define CHEN_MOTION_TIMEOUT_MS 200u
#define CHEN_FRAME_TIMEOUT_MS 100u
#define CHEN_LINK_TIMEOUT_MS 1000u
#define CHEN_TASK_DEADLINE_MS 20u

/* Serial links. */
#define CHEN_DEBUG_BAUD 115200u
#define CHEN_BLE_BAUD 115200u

/* Stack sizes in words (4 bytes each). */
#define CHEN_STACK_CONTROL 384u
#define CHEN_STACK_COMM 320u
#define CHEN_STACK_HMI 512u
#define CHEN_STACK_TELEMETRY 256u
#define CHEN_STACK_DIAG 256u

/* Priorities: numeric value is the FreeRTOS priority, higher runs first.
 * Safety/control outranks communication; HMI is deliberately low. */
#define CHEN_PRIO_CONTROL 5u
#define CHEN_PRIO_COMM 4u
#define CHEN_PRIO_TELEMETRY 2u
#define CHEN_PRIO_DIAG 1u
#define CHEN_PRIO_HMI 1u

#endif
