/* chen_bot_v_1_0 firmware entry point.
 *
 * Start-up order is deliberate: safe pin levels first (before HAL or clocks),
 * then clocks, then the buses, then the device table, then the application.
 * Any failure along the way ends in board_fatal(), which forces the power
 * stage off and parks the core.
 */
#include "app.h"
#include "board.h"
#include "features.h"
#include "lcd.h"
#include "mock.h"
#include "profile.h"
#include "real_devices.h"
#include "safety.h"

#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

#if CHEN_PROFILE == CHEN_SIM
#include "mock.h"
static Mock sim_model;
#endif

static StaticTask_t idle_task;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack, uint32_t *size)
{
    *tcb = &idle_task;
    *stack = idle_stack;
    *size = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    board_fatal();
}

void HardFault_Handler(void) { board_fatal(); }
void MemManage_Handler(void) { board_fatal(); }
void BusFault_Handler(void) { board_fatal(); }
void UsageFault_Handler(void) { board_fatal(); }
void NMI_Handler(void) { board_fatal(); }

int main(void)
{
    /* 1. Safe electrical state before anything can run code. */
    board_safe_defaults();

    /* 2. Core, clocks, interrupt grouping. */
    if (HAL_Init() != HAL_OK) board_fatal();
    board_clock_init();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    /* 3. Buses and analog front end. A missing LCD or link is degraded, not
     *    fatal: the robot can still run and report over whichever link works. */
    const bool links = board_links_init();
    (void)board_pwm_init();
    const bool analog_ok = board_analog_init();
    (void)analog_ok;
#if CHEN_FEATURE_PC_CAN
    const bool can_ok = board_can_init();
#else
    const bool can_ok = false;
#endif
    (void)can_ok;

        const bool lcd_ok = smoke_panel_init();
    if (lcd_ok) smoke_panel_set_backlight(true);

    /* 4. Device table for this profile. */
    Devices devices;
#if CHEN_PROFILE == CHEN_SIM
    sim_model.battery_mv = 14800;
    sim_model.now_ms = 0u;
    devices = mock_devices(&sim_model);
#else
    devices = real_devices();
#endif

    /* 5. Application: parameters, safety, tasks. */
    if (!app_init(&devices, links || CHEN_PROFILE == CHEN_SIM, lcd_ok)) board_fatal();
    app_start();

    /* 6. Hand control to the scheduler; reaching here again is a failure. */
    vTaskStartScheduler();
    board_fatal();
    return 1;
}
