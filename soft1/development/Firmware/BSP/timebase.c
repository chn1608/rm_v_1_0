/* TIM6 implementation follows the HAL timebase contract in mc02_control.
 * Copyright (c) 2025 STMicroelectronics. See the reference component licenses.
 */
#include "stm32h7xx_hal.h"

static TIM_HandleTypeDef tick_timer;

HAL_StatusTypeDef HAL_InitTick(uint32_t priority)
{
    if (priority >= (1UL << __NVIC_PRIO_BITS)) return HAL_ERROR;
    RCC_ClkInitTypeDef clocks;
    uint32_t latency;
    HAL_RCC_GetClockConfig(&clocks, &latency);
    uint32_t hz = HAL_RCC_GetPCLK1Freq();
    if (clocks.APB1CLKDivider != RCC_APB1_DIV1) hz *= 2u;
    if (hz < 1000000u || hz % 1000000u != 0u) return HAL_ERROR;
    __HAL_RCC_TIM6_CLK_ENABLE();
    HAL_NVIC_DisableIRQ(TIM6_DAC_IRQn);
    tick_timer.Instance = TIM6;
    tick_timer.Init.Prescaler = hz / 1000000u - 1u;
    tick_timer.Init.Period = 1000u - 1u;
    tick_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    if (HAL_TIM_Base_Init(&tick_timer) != HAL_OK) return HAL_ERROR;
    __HAL_TIM_CLEAR_FLAG(&tick_timer, TIM_FLAG_UPDATE);
    HAL_NVIC_ClearPendingIRQ(TIM6_DAC_IRQn);
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, priority, 0u);
    uwTickPrio = priority;
    const HAL_StatusTypeDef result = HAL_TIM_Base_Start_IT(&tick_timer);
    if (result == HAL_OK) HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    return result;
}
void TIM6_DAC_IRQHandler(void) { HAL_TIM_IRQHandler(&tick_timer); }
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *timer)
{
    if (timer == &tick_timer) HAL_IncTick();
}
void HAL_SuspendTick(void) { __HAL_TIM_DISABLE_IT(&tick_timer, TIM_IT_UPDATE); }
void HAL_ResumeTick(void) { __HAL_TIM_ENABLE_IT(&tick_timer, TIM_IT_UPDATE); }
