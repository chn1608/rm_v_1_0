#include "board.h"
#include "keys.h"
#include "panel.h"
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

#define SAMPLE_MS 20u
#define ADC_FULL_SCALE 4095u
#define NOMINAL_KEY_REF_MV 3300u
#define KEY_SIZE 32u
#define SMOKE_STACK_WORDS 384u

enum {
    FAULT_HAL = 1,
    FAULT_SCHEDULER = 2,
    FAULT_PANEL = 3,
    FAULT_ADC_SETUP = 4,
    FAULT_CPU = 5
};

/* Keil Watch expression: lcd_smoke. Millivolts use nominal, uncalibrated VREF. */
volatile struct {
    uint32_t stage;
    uint32_t fault;
    uint32_t heartbeat;
    uint32_t adc_errors;
    uint32_t key_changes;
    uint32_t seen_keys;
    uint32_t uart_dropped;
    uint16_t adc_raw;
    uint16_t key_mv;
    uint8_t key_candidate;
    uint8_t key_stable;
    uint8_t key_sample_valid;
    uint8_t serial_ready;
    uint8_t panel_ready;
    uint8_t adc_ready;
} lcd_smoke;

static Transport serial;
static StaticTask_t smoke_tcb, idle_tcb;
static StackType_t smoke_stack[SMOKE_STACK_WORDS], idle_stack[configMINIMAL_STACK_SIZE];

static void fail(uint32_t fault)
{
    lcd_smoke.fault = fault;
    board_fatal();
}

static void fill(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    if (lcd_smoke.panel_ready && !smoke_panel_fill(x, y, width, height, color)) {
        lcd_smoke.panel_ready = 0u;
        smoke_panel_set_backlight(false);
        if (lcd_smoke.fault == 0u) lcd_smoke.fault = FAULT_PANEL;
    }
}

static void key_block(Key key, uint16_t color)
{
    static const uint16_t origins[][2] = {
        {104, 144},  /* Center */
        {64, 144},   /* Left */
        {144, 144},  /* Right */
        {104, 104},  /* Up */
        {104, 184},  /* Down */
    };
    if (key < KEY_CENTER || key > KEY_DOWN) return;
    const unsigned i = (unsigned)(key - KEY_CENTER);
    fill(origins[i][0], origins[i][1], KEY_SIZE, KEY_SIZE, color);
}

static void serial_write(const char *line, unsigned length)
{
    if (lcd_smoke.serial_ready &&
        serial.write(serial.context, (const uint8_t *)line, length) != DEV_OK) {
        ++lcd_smoke.uart_dropped;
    }
}

static bool serial_query_received(void)
{
    uint8_t received[8];
    unsigned used;
    if (!lcd_smoke.serial_ready ||
        serial.read(serial.context, received, sizeof(received), &used) != DEV_OK)
        return false;
    for (unsigned i = 0; i < used; ++i) {
        if (received[i] == '?') return true;
    }
    return false;
}

static void serial_status(const char *tag)
{
    static const char *const key_names[] = {"NONE", "CENTER", "LEFT", "RIGHT", "UP", "DOWN"};
    if (!lcd_smoke.serial_ready) return;
    const unsigned key = lcd_smoke.key_stable;
    const char *name = key < sizeof(key_names) / sizeof(key_names[0]) ? key_names[key] : "UNKNOWN";
    /* The shared transport retains one bounded, nonblocking TX buffer. */
    char line[80];
    const int n = snprintf(line, sizeof(line), "%s hb=%lu st=%u fault=%u key=%s raw=%u adc=%u lcd=%u\r\n",
                           tag, (unsigned long)lcd_smoke.heartbeat, (unsigned)lcd_smoke.stage,
                           (unsigned)lcd_smoke.fault, name, (unsigned)lcd_smoke.adc_raw,
                           (unsigned)lcd_smoke.key_sample_valid, (unsigned)lcd_smoke.panel_ready);
    if (n > 0 && (unsigned)n < sizeof(line)) serial_write(line, (unsigned)n);
}
static void smoke_task(void *arg)
{
    (void)arg;
    lcd_smoke.stage = 3u;
    static const char banner[] = "LCD_SMOKE USART1 115200 8N1\r\n";
    serial_write(banner, sizeof(banner) - 1u);
    lcd_smoke.panel_ready = (uint8_t)smoke_panel_init();
    if (!lcd_smoke.panel_ready) lcd_smoke.fault = FAULT_PANEL;

    fill(0u, 0u, PANEL_WIDTH, PANEL_HEIGHT, 0x0842u);
    fill(12u, 14u, 72u, 56u, 0xf800u);
    fill(84u, 14u, 72u, 56u, 0x07e0u);
    fill(156u, 14u, 72u, 56u, 0x001fu);
    for (unsigned key = KEY_CENTER; key <= KEY_DOWN; ++key) {
        key_block((Key)key, 0x630cu);
    }
    if (lcd_smoke.panel_ready) smoke_panel_set_backlight(true);
    lcd_smoke.stage = 4u;

    const bool adc_ready = smoke_key_adc_init();
    lcd_smoke.adc_ready = (uint8_t)adc_ready;
    if (!adc_ready && lcd_smoke.fault == 0u) lcd_smoke.fault = FAULT_ADC_SETUP;
    lcd_smoke.stage = 5u;

    KeyFilter filter = {.stable = KEY_NONE, .candidate = KEY_NONE, .repeats = 0u};
    Key highlighted = KEY_NONE;
    TickType_t wake = xTaskGetTickCount();
    TickType_t last_heartbeat = wake;
    for (;;) {
        uint16_t raw;
        Key candidate = KEY_UNKNOWN;
        const bool valid = adc_ready && smoke_key_adc_sample(&raw);
        lcd_smoke.key_sample_valid = (uint8_t)valid;
        if (valid) {
            const uint16_t mv = (uint16_t)(((uint32_t)raw * NOMINAL_KEY_REF_MV +
                                           ADC_FULL_SCALE / 2u) / ADC_FULL_SCALE);
            lcd_smoke.adc_raw = raw;
            lcd_smoke.key_mv = mv;
            candidate = key_classify_mv(mv);
        } else if (adc_ready) {
            ++lcd_smoke.adc_errors;
        }

        lcd_smoke.key_candidate = (uint8_t)candidate;
        Key next;
        const bool key_changed = key_update(&filter, candidate, &next);
        if (key_changed) {
            lcd_smoke.key_stable = (uint8_t)next;
            ++lcd_smoke.key_changes;
            if (next >= KEY_CENTER && next <= KEY_DOWN) {
                lcd_smoke.seen_keys |= 1u << (unsigned)(next - KEY_CENTER);
            }
            key_block(highlighted, 0x630cu);
            key_block(next, 0x07e0u);
            highlighted = next;
        }
        const TickType_t now = xTaskGetTickCount();
        const bool heartbeat_due = now - last_heartbeat >= pdMS_TO_TICKS(1000u);
        if (heartbeat_due) {
            last_heartbeat = now;
            ++lcd_smoke.heartbeat;
            const uint16_t color = (lcd_smoke.heartbeat & 1u) ? 0x07ffu : 0x630cu;
            fill(12u, 252u, 216u, 10u, color);
        }
        if (serial_query_received()) serial_status("STATUS");
        else if (heartbeat_due) serial_status("HB");
        else if (key_changed) serial_status("KEY");
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(SAMPLE_MS));
    }
}

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack, uint32_t *size)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *size = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    fail(FAULT_CPU);
}

void HardFault_Handler(void) { fail(FAULT_CPU); }
void MemManage_Handler(void) { fail(FAULT_CPU); }
void BusFault_Handler(void) { fail(FAULT_CPU); }
void UsageFault_Handler(void) { fail(FAULT_CPU); }

int main(void)
{
    board_hold_fixed_outputs_off();
    if (HAL_Init() != HAL_OK) fail(FAULT_HAL);
    lcd_smoke.stage = 1u;
    board_clock_init();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    lcd_smoke.stage = 2u;
    lcd_smoke.serial_ready = (uint8_t)board_debug_init(&serial);
    if (xTaskCreateStatic(smoke_task, "lcd-smoke", SMOKE_STACK_WORDS, NULL, 2u,
                          smoke_stack, &smoke_tcb) == NULL)
        fail(FAULT_SCHEDULER);
    vTaskStartScheduler();
    fail(FAULT_SCHEDULER);
    return 1;
}
