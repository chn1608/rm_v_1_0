#include "board.h"
#include "profile.h"
#include "service.h"
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#if RM_PROFILE != RM_SIM
#include "lcd_runtime.h"
#endif
#if RM_PROFILE == RM_SIM
#include "mock.h"
static Mock model;
#endif

static StaticTask_t app_task, idle_task;
static StackType_t app_stack[384], idle_stack[configMINIMAL_STACK_SIZE];
static Safety safety;
static Params params;
static Parser pc_parser;
static Transport pc;
static Devices devices;
static uint32_t dropped_tx, link_errors;
#if RM_PROFILE != RM_SIM
static HmiSnapshot lcd_snapshot;
static bool debug_uart_ready, pc_frame_seen;
static uint32_t last_pc_frame_ms;

static void read_lcd_snapshot(HmiSnapshot *out)
{
    taskENTER_CRITICAL();
    *out = lcd_snapshot;
    taskEXIT_CRITICAL();
}

static void publish_lcd_snapshot(const Snapshot *snapshot)
{
    const bool has_voltage = snapshot->bus_mv.valid && snapshot->bus_mv.source == DATA_MEASURED;
    const bool recent_pc = pc_frame_seen && snapshot->now_ms - last_pc_frame_ms < 1000u;
    taskENTER_CRITICAL();
    lcd_snapshot.fields[HMI_BUS_MV] = (HmiValue){snapshot->bus_mv.value, has_voltage};
    lcd_snapshot.fields[HMI_SYSTEM_STATE] = (HmiValue){(int32_t)safety.state, true};
    lcd_snapshot.fields[HMI_CONTROL_MODE] = (HmiValue){(int32_t)safety.owner, true};
    lcd_snapshot.fields[HMI_POWER_COMMAND] = (HmiValue){safety.motion_permitted, true};
    lcd_snapshot.fields[HMI_FAULT_CODE] = (HmiValue){(int32_t)safety.faults, true};
    lcd_snapshot.fields[HMI_PC_CONNECTED] = (HmiValue){1, recent_pc};
    lcd_snapshot.fields[HMI_VALID_FRAMES] = (HmiValue){(int32_t)(pc_parser.good & 0x7fffffffu), debug_uart_ready};
    taskEXIT_CRITICAL();
}
#endif

static void reply_to_pc(void *context, const Frame *req)
{
    Snapshot *snap = context;
#if RM_PROFILE != RM_SIM
    pc_frame_seen = true;
    last_pc_frame_ms = snap->now_ms;
#endif
    Frame response;
    uint8_t wire[RM_FRAME_MAX];
    debug_handle(&params, &safety, snap, OWNER_PC, false, req, &response);
    const size_t count = protocol_encode(&response, wire, sizeof(wire));
    if (count == 0u || pc.write(pc.context, wire, (unsigned)count) != DEV_OK) ++dropped_tx;
}

static void cycle(void *unused)
{
    (void)unused;
    TickType_t tick = xTaskGetTickCount();
    for (;;) {
        const uint32_t now = HAL_GetTick();
        SafetyInputs input = {0};
#if RM_PROFILE == RM_SIM
        model.now_ms = now;
        input.interlocks_valid = true;
#endif
        safety_step(&safety, input, now);
        if (!safety.motion_permitted || RM_PROFILE != RM_SIM) {
            if (devices.power(devices.context, false) != DEV_OK) safety_trip(&safety, FAULT_DEVICE);
        }
        Snapshot snap = {.safety = &safety, .now_ms = now,
                         .good_frames = pc_parser.good,
                         .bad_frames = pc_parser.bad + pc_parser.expired + link_errors + dropped_tx};
        devices.voltage(devices.context, 0u, &snap.bus_mv);
        uint8_t received[64];
        unsigned count;
        const DeviceResult rx = pc.read(pc.context, received, sizeof(received), &count);
        if (rx == DEV_IO) { pc_parser.used = 0u; ++link_errors; }
        if (rx == DEV_OK) {
            for (unsigned i = 0; i < count; ++i) {
                protocol_feed(&pc_parser, received[i], now, reply_to_pc, &snap);
            }
        }
#if RM_PROFILE != RM_SIM
        publish_lcd_snapshot(&snap);
#endif
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(RM_CONTROL_PERIOD_MS));
    }
}

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

int main(void)
{
    board_hold_fixed_outputs_off();
    if (HAL_Init() != HAL_OK) board_fatal();
    board_clock_init();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    const bool uart_ready = board_debug_init(&pc);
#if RM_PROFILE != RM_SIM
    debug_uart_ready = uart_ready;
#endif
#if RM_PROFILE == RM_SIM
    devices = mock_devices(&model);
#else
    devices = board_devices();
#endif
    params_reset(&params);
    safety_begin(&safety);
    const bool outputs_off = devices.power(devices.context, false) == DEV_OK;
    safety_self_test(&safety, uart_ready && outputs_off && RM_PROFILE == RM_SIM);
    if (xTaskCreateStatic(cycle, "safety-diag", 384u, NULL, 3u, app_stack, &app_task) == NULL) {
        board_fatal();
    }
#if RM_PROFILE != RM_SIM
    (void)lcd_runtime_start(read_lcd_snapshot);
#endif
    vTaskStartScheduler();
    board_fatal();
    return 1;
}
