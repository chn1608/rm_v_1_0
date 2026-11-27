#ifndef RM_FREERTOS_CONFIG_H
#define RM_FREERTOS_CONFIG_H
#include <stdint.h>
extern uint32_t SystemCoreClock;
void board_fatal(void);

#define configUSE_PREEMPTION 1
#define configCPU_CLOCK_HZ SystemCoreClock
#define configTICK_RATE_HZ ((TickType_t)1000)
#define configMAX_PRIORITIES 8
#define configMINIMAL_STACK_SIZE 128
#define configMAX_TASK_NAME_LEN 16
#define configUSE_16_BIT_TICKS 0
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configSUPPORT_STATIC_ALLOCATION 1
#define configSUPPORT_DYNAMIC_ALLOCATION 0
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configKERNEL_INTERRUPT_PRIORITY (15 << 4)
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5 << 4)
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define configASSERT(condition) do { if (!(condition)) board_fatal(); } while (0)
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler
#endif
