/* Interrupt driven UART ring buffer for the ATK-BLE02 link (UART7).
 *
 * The PC link is CAN-only (see BSP/can_link.c); there is no USART debug link in
 * this firmware. One byte per interrupt keeps the ISR short, and the ring
 * absorbs a burst while reporting overflow to the caller instead of dropping
 * bytes silently.
 */
#include "board.h"
#include "board_config.h"
#include "features.h"
#include "profile.h"
#include "stm32h7xx_hal.h"
#include <string.h>

#define LINK_RING 256u
#define LINK_TX 96u

typedef struct {
    UART_HandleTypeDef handle;
    IRQn_Type irq;
    uint8_t rx[LINK_RING];
    uint8_t tx[LINK_TX];
    volatile unsigned head, tail;
    volatile bool lost, ready;
    uint8_t byte;
} Link;

static Link ble_link;

static bool link_start(Link *link, USART_TypeDef *instance, IRQn_Type irq, uint32_t baud)
{
    link->irq = irq;
    link->handle.Instance = instance;
    link->handle.Init.BaudRate = baud;
    link->handle.Init.WordLength = UART_WORDLENGTH_8B;
    link->handle.Init.StopBits = UART_STOPBITS_1;
    link->handle.Init.Parity = UART_PARITY_NONE;
    link->handle.Init.Mode = UART_MODE_TX_RX;
    link->handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    link->handle.Init.OverSampling = UART_OVERSAMPLING_16;
    link->handle.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    if (HAL_UART_Init(&link->handle) != HAL_OK) return false;
    link->ready = HAL_UART_Receive_IT(&link->handle, &link->byte, 1u) == HAL_OK;
    if (link->ready) {
        HAL_NVIC_SetPriority(irq, 7u, 0u);
        HAL_NVIC_EnableIRQ(irq);
    }
    return link->ready;
}

static void link_irq(Link *link)
{
    HAL_UART_IRQHandler(&link->handle);
}

static void link_rx_complete(Link *link)
{
    if (link->handle.ErrorCode != HAL_UART_ERROR_NONE) {
        link->lost = true;
        return;
    }
    const unsigned next = (link->head + 1u) % LINK_RING;
    if (next == link->tail) link->lost = true;
    else {
        link->rx[link->head] = link->byte;
        link->head = next;
    }
    if (HAL_UART_Receive_IT(&link->handle, &link->byte, 1u) != HAL_OK) link->lost = true;
}

static void link_error(Link *link)
{
    link->lost = true;
    if (link->handle.RxState == HAL_UART_STATE_READY &&
        HAL_UART_Receive_IT(&link->handle, &link->byte, 1u) != HAL_OK) {
        link->ready = false;
    }
}

static DeviceResult link_read(Link *link, uint8_t *out, unsigned capacity, unsigned *used)
{
    *used = 0u;
    if (!link->ready) return DEV_UNAVAILABLE;
    HAL_NVIC_DisableIRQ(link->irq);
    const bool lost = link->lost;
    if (lost) {
        link->tail = link->head;   /* drop the damaged burst */
        link->lost = false;
    }
    while (*used < capacity && link->tail != link->head) {
        out[(*used)++] = link->rx[link->tail];
        link->tail = (link->tail + 1u) % LINK_RING;
    }
    HAL_NVIC_EnableIRQ(link->irq);
    return lost ? DEV_IO : DEV_OK;
}

static DeviceResult link_write(Link *link, const uint8_t *data, unsigned length)
{
    if (!link->ready || link->handle.gState != HAL_UART_STATE_READY) return DEV_UNAVAILABLE;
    if (length == 0u || length > sizeof(link->tx)) return DEV_INVALID;
    memcpy(link->tx, data, length);
    return HAL_UART_Transmit_IT(&link->handle, link->tx, (uint16_t)length) == HAL_OK
               ? DEV_OK : DEV_IO;
}

/* --------------------------------------------------------------- adapters */

static DeviceResult ble_read(void *context, uint8_t *data, unsigned capacity, unsigned *used)
{
    (void)context;
    return link_read(&ble_link, data, capacity, used);
}

static DeviceResult ble_write(void *context, const uint8_t *data, unsigned length)
{
    (void)context;
    return link_write(&ble_link, data, length);
}

Transport board_ble_transport(void)
{
    const Transport t = {.context = NULL, .read = ble_read, .write = ble_write};
    return t;
}

bool board_ble_ready(void) { return ble_link.ready; }

#if CHEN_FEATURE_BLE
static bool ble_pins(void)
{
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_UART7_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_7 | GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF7_UART7;
    HAL_GPIO_Init(GPIOE, &gpio);

    /* LINK is an output from the module: input, pulled up so an unplugged
     * module reads "not connected" instead of floating. */
    GPIO_InitTypeDef status = {0};
    status.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    status.Mode = GPIO_MODE_INPUT;
    status.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOD, &status);
    return true;
}
#endif

bool board_links_init(void)
{
#if CHEN_FEATURE_BLE
    if (!ble_pins()) return false;
    if (!link_start(&ble_link, UART7, UART7_IRQn, CHEN_BLE_BAUD)) return false;
#endif
    return true;
}

void UART7_IRQHandler(void) { link_irq(&ble_link); }

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart == &ble_link.handle) link_rx_complete(&ble_link);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart == &ble_link.handle) link_error(&ble_link);
}
