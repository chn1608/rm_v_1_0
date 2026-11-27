#include "board.h"
#include "lcd_runtime.h"
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

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

int main(void)
{
    board_hold_fixed_outputs_off();
    if (HAL_Init() != HAL_OK) board_fatal();
    board_clock_init();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    /* The bench shares the product UI. It has no external telemetry provider. */
    if (!lcd_runtime_start(NULL)) board_fatal();

    vTaskStartScheduler();
    board_fatal();
    return 1;
}
