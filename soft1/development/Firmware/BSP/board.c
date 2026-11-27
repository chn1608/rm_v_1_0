/* Clock configuration adapted from mc02_control Core/Src/main.c.
 * Copyright (c) 2025 STMicroelectronics. See the reference component licenses.
 */
#include "board.h"
#include "profile.h"
#include "stm32h7xx_hal.h"
#include <string.h>

#define BOARD_POWER_PINS (GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15)
#define UART_RING_SIZE 256u

static UART_HandleTypeDef debug_uart;
static uint8_t rx_byte;
static uint8_t rx_data[UART_RING_SIZE], tx_data[80];
static volatile unsigned rx_head, rx_tail;
static volatile bool rx_lost;
static volatile bool debug_ok;

void board_hold_fixed_outputs_off(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOC, BOARD_POWER_PINS, GPIO_PIN_RESET);
    gpio.Pin = BOARD_POWER_PINS;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);
}

void board_fatal(void)
{
    __disable_irq();
    board_hold_fixed_outputs_off();
    for (;;) { __WFI(); }
}

void board_clock_init(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clock = {0};
    if (HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY) != HAL_OK) board_fatal();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    const uint32_t start = HAL_GetTick();
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
        if (HAL_GetTick() - start >= 100u) board_fatal();
    }
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 2;
    osc.PLL.PLLN = 40;
    osc.PLL.PLLP = 1;
    osc.PLL.PLLQ = 6;
    osc.PLL.PLLR = 2;
    osc.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
    osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) board_fatal();
    clock.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                      RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                      RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    clock.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clock.SYSCLKDivider = RCC_SYSCLK_DIV1;
    clock.AHBCLKDivider = RCC_HCLK_DIV2;
    clock.APB3CLKDivider = RCC_APB3_DIV2;
    clock.APB1CLKDivider = RCC_APB1_DIV2;
    clock.APB2CLKDivider = RCC_APB2_DIV2;
    clock.APB4CLKDivider = RCC_APB4_DIV2;
    if (HAL_RCC_ClockConfig(&clock, FLASH_LATENCY_3) != HAL_OK) board_fatal();
}

static DeviceResult debug_read(void *ctx, uint8_t *out, unsigned cap, unsigned *used)
{
    (void)ctx;
    *used = 0u;
    if (!debug_ok) return DEV_UNAVAILABLE;
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    const bool lost = rx_lost;
    if (lost) {
        rx_tail = rx_head;
        rx_lost = false;
    }
    while (*used < cap && rx_tail != rx_head) {
        out[(*used)++] = rx_data[rx_tail];
        rx_tail = (rx_tail + 1u) % UART_RING_SIZE;
    }
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    return lost ? DEV_IO : DEV_OK;
}

static DeviceResult debug_write(void *ctx, const uint8_t *data, unsigned n)
{
    (void)ctx;
    if (!debug_ok || debug_uart.gState != HAL_UART_STATE_READY) return DEV_UNAVAILABLE;
    if (n == 0u || n > sizeof(tx_data)) return DEV_INVALID;
    memcpy(tx_data, data, n);
    return HAL_UART_Transmit_IT(&debug_uart, tx_data, (uint16_t)n) == HAL_OK ? DEV_OK : DEV_IO;
}

bool board_debug_init(Transport *pc)
{
    const Transport transport = {.read = debug_read, .write = debug_write};
    *pc = transport;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);
    debug_uart.Instance = USART1;
    debug_uart.Init.BaudRate = RM_DEBUG_BAUD;
    debug_uart.Init.WordLength = UART_WORDLENGTH_8B;
    debug_uart.Init.StopBits = UART_STOPBITS_1;
    debug_uart.Init.Parity = UART_PARITY_NONE;
    debug_uart.Init.Mode = UART_MODE_TX_RX;
    debug_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    debug_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    debug_uart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    if (HAL_UART_Init(&debug_uart) != HAL_OK) return false;
    debug_ok = HAL_UART_Receive_IT(&debug_uart, &rx_byte, 1u) == HAL_OK;
    if (debug_ok) {
        HAL_NVIC_SetPriority(USART1_IRQn, 7u, 0u);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    }
    return debug_ok;
}
void USART1_IRQHandler(void) { HAL_UART_IRQHandler(&debug_uart); }

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart != &debug_uart) return;
    if (uart->ErrorCode != HAL_UART_ERROR_NONE) {
        rx_lost = true;
        return; /* The HAL error callback rearms after handling its flags. */
    }
    const unsigned next = (rx_head + 1u) % UART_RING_SIZE;
    if (next == rx_tail) rx_lost = true;
    else { rx_data[rx_head] = rx_byte; rx_head = next; }
    if (HAL_UART_Receive_IT(uart, &rx_byte, 1u) != HAL_OK) rx_lost = true;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart != &debug_uart) return;
    rx_lost = true;
    if (uart->RxState == HAL_UART_STATE_READY &&
        HAL_UART_Receive_IT(uart, &rx_byte, 1u) != HAL_OK) debug_ok = false;
}

static DeviceResult power(void *ctx, bool enabled)
{
    (void)ctx;
    if (enabled) return DEV_UNAVAILABLE;
    HAL_GPIO_WritePin(GPIOC, BOARD_POWER_PINS, GPIO_PIN_RESET);
    return DEV_OK;
}
static DeviceResult voltage(void *ctx, unsigned channel, Reading *out)
{
    (void)ctx;
    *out = (Reading){0, HAL_GetTick(), channel == 0u ? DATA_UNCALIBRATED : DATA_ABSENT, false};
    return DEV_UNAVAILABLE;
}
Devices board_devices(void)
{
    const Devices d = {.power = power, .voltage = voltage};
    return d;
}
