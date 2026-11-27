/* FDCAN transport for the PC link.
 *
 * Replaces the USART1 "USB 转串口" status link: the PC now talks to the board
 * through a DM USB2CANFD adapter on CAN1 (PD0/PD1, on-board transceiver).
 *
 * Classic CAN carries 8 bytes per frame, so the application protocol frame is
 * segmented: segment 0 holds the first 7 bytes, each later segment the next 7.
 * A receiver knows the total length from the protocol header (byte 4), so no
 * extra length field is needed. Segments must arrive in order; a gap resets the
 * reassembly buffer and is counted as a link error.
 *
 *   MCU -> PC : 11-bit id 0x601
 *   PC  -> MCU: 11-bit id 0x600
 */
#include "board.h"
#include "board_config.h"
#include "features.h"
#include "protocol.h"
#include "stm32h7xx_hal.h"
#include <string.h>

#if CHEN_FEATURE_PC_CAN

#define CAN_RX_ID BOARD_CAN_RX_ID
#define CAN_TX_ID BOARD_CAN_TX_ID
#define SEG_DATA 7u
#define RX_SLOTS 16u

typedef struct {
    uint8_t data[SEG_DATA];
    uint8_t length;
    uint8_t sequence;
} Segment;

static FDCAN_HandleTypeDef can;
static Transport can_transport;
static volatile Segment rx_slots[RX_SLOTS];
static volatile unsigned rx_head, rx_tail;
static volatile uint32_t rx_lost, tx_fail;

/* Reassembly state, only touched from the reader (task context). */
static uint8_t rx_bytes[PROTOCOL_FRAME_MAX];
static unsigned rx_used;
static unsigned rx_total;
static uint8_t rx_expect_seq;
static uint32_t rx_errors;

static bool link_ready;

static void can_pins(void)
{
    __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF9_FDCAN1;
    HAL_GPIO_Init(GPIOD, &gpio);
}

static void can_clock(void)
{
    RCC_PeriphCLKInitTypeDef clock = {0};
    clock.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    clock.FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL;
    (void)HAL_RCCEx_PeriphCLKConfig(&clock);
}

bool board_can_init(void)
{
    __HAL_RCC_FDCAN_CLK_ENABLE();
    can_clock();
    can_pins();

    /* 80 MHz kernel clock / 5 = 16 MHz; 16 tq per bit = 1 Mbit/s. */
    can.Instance = FDCAN1;
    can.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    can.Init.Mode = FDCAN_MODE_NORMAL;
    can.Init.AutoRetransmission = ENABLE;
    can.Init.TransmitPause = DISABLE;
    can.Init.ProtocolException = DISABLE;
    can.Init.NominalPrescaler = 5u;
    can.Init.NominalSyncJumpWidth = 2u;
    can.Init.NominalTimeSeg1 = 13u;
    can.Init.NominalTimeSeg2 = 2u;
    can.Init.DataPrescaler = 1u;
    can.Init.DataSyncJumpWidth = 1u;
    can.Init.DataTimeSeg1 = 1u;
    can.Init.DataTimeSeg2 = 1u;
    /* Message RAM must be laid out explicitly: with zero Tx FIFO elements
     * HAL_FDCAN_AddMessageToTxFifoQ() can never succeed, and with zero Rx FIFO
     * elements nothing is received. */
    can.Init.StdFiltersNbr = 1u;
    can.Init.ExtFiltersNbr = 0u;
    can.Init.RxFifo0ElmtsNbr = 8u;
    can.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
    can.Init.RxFifo1ElmtsNbr = 0u;
    can.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
    can.Init.RxBuffersNbr = 0u;
    can.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
    can.Init.TxEventsNbr = 0u;
    can.Init.TxBuffersNbr = 0u;
    can.Init.TxFifoQueueElmtsNbr = 8u;
    can.Init.TxElmtSize = FDCAN_DATA_BYTES_8;
    can.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    if (HAL_FDCAN_Init(&can) != HAL_OK) return false;

    FDCAN_FilterTypeDef filter = {0};
    filter.IdType = FDCAN_STANDARD_ID;
    filter.FilterIndex = 0u;
    filter.FilterType = FDCAN_FILTER_MASK;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1 = CAN_RX_ID;
    filter.FilterID2 = 0x7FFu;          /* exact match on CAN_RX_ID */
    if (HAL_FDCAN_ConfigFilter(&can, &filter) != HAL_OK) return false;
    if (HAL_FDCAN_ConfigGlobalFilter(&can, FDCAN_REJECT, FDCAN_REJECT,
                                     FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK)
        return false;
    if (HAL_FDCAN_Start(&can) != HAL_OK) return false;
    /* Also react to bus-off: without a recovery path a single period with no
     * ACK (adapter unplugged, 5 V missing) latches the node off the bus for
     * good, which then looks exactly like a wiring fault. */
    if (HAL_FDCAN_ActivateNotification(&can,
                                       FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_BUS_OFF,
                                       0u) != HAL_OK)
        return false;

    HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 7u, 0u);
    HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
    link_ready = true;
    return true;
}

bool board_can_ready(void)
{
    return link_ready && HAL_FDCAN_GetState(&can) == HAL_FDCAN_STATE_BUSY;
}

/* ISR side: move the frame out of the hardware FIFO into a small ring. No
 * protocol work happens here. */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *handle, uint32_t flags)
{
    if (handle != &can || (flags & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0u) return;

    FDCAN_RxHeaderTypeDef header;
    uint8_t raw[8];
    if (HAL_FDCAN_GetRxMessage(handle, FDCAN_RX_FIFO0, &header, raw) != HAL_OK) return;
    if (header.IdType != FDCAN_STANDARD_ID || header.Identifier != CAN_RX_ID) return;

    const unsigned next = (rx_head + 1u) % RX_SLOTS;
    if (next == rx_tail) {
        ++rx_lost;                      /* consumer too slow: drop, count it */
        return;
    }
    Segment *slot = (Segment *)&rx_slots[rx_head];
    slot->length = (uint8_t)(header.DataLength > SEG_DATA ? SEG_DATA : header.DataLength);
    slot->sequence = raw[0];
    memcpy((void *)slot->data, raw + 1, SEG_DATA);
    rx_head = next;
}

void FDCAN1_IT0_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&can);
}

void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *handle, uint32_t flags)
{
    if (handle != &can || (flags & FDCAN_IT_BUS_OFF) == 0u) return;
    /* Restart the node so it rejoins the bus once the wiring or power is back. */
    (void)HAL_FDCAN_Stop(handle);
    (void)HAL_FDCAN_Start(handle);
    (void)HAL_FDCAN_ActivateNotification(handle,
                                         FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_BUS_OFF,
                                         0u);
}

/* Reader side: unsegment into the protocol byte stream. */
static DeviceResult can_read(void *context, uint8_t *out, unsigned capacity, unsigned *used)
{
    (void)context;
    *used = 0u;
    if (!link_ready) return DEV_UNAVAILABLE;

    while (rx_tail != rx_head && *used < capacity) {
        const Segment *slot = (const Segment *)&rx_slots[rx_tail];
        const uint8_t expected = rx_expect_seq;

        /* A lost or reordered segment invalidates the whole frame. */
        if (slot->sequence != expected) {
            ++rx_errors;
            rx_used = 0u;
            rx_total = 0u;
            rx_expect_seq = 0u;
            rx_tail = (rx_tail + 1u) % RX_SLOTS;
            continue;
        }

        const unsigned take = (slot->length <= SEG_DATA) ? slot->length : SEG_DATA;
        for (unsigned i = 0; i < take && rx_used < sizeof(rx_bytes); ++i) {
            rx_bytes[rx_used++] = slot->data[i];
        }
        rx_expect_seq = (uint8_t)(rx_expect_seq + 1u);
        rx_tail = (rx_tail + 1u) % RX_SLOTS;

        if (rx_total == 0u && rx_used >= 5u) {
            rx_total = (unsigned)rx_bytes[4] + 9u;
            if (rx_total > PROTOCOL_FRAME_MAX) {   /* malformed header */
                ++rx_errors;
                rx_used = 0u;
                rx_total = 0u;
                rx_expect_seq = 0u;
                continue;
            }
        }
        if (rx_total != 0u && rx_used >= rx_total) {
            for (unsigned i = 0; i < rx_total && *used < capacity; ++i) out[(*used)++] = rx_bytes[i];
            rx_used = 0u;
            rx_total = 0u;
            rx_expect_seq = 0u;
        }
    }
    return (rx_lost != 0u) ? DEV_IO : DEV_OK;
}

static bool can_send(uint8_t sequence, const uint8_t *payload, unsigned length)
{
    FDCAN_TxHeaderTypeDef header = {0};
    uint8_t data[8] = {0};
    header.Identifier = CAN_TX_ID;
    header.IdType = FDCAN_STANDARD_ID;
    header.TxFrameType = FDCAN_DATA_FRAME;
    header.DataLength = (uint32_t)(length + 1u);
    header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    header.BitRateSwitch = FDCAN_BRS_OFF;
    header.FDFormat = FDCAN_CLASSIC_CAN;
    header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    header.MessageMarker = 0u;
    data[0] = sequence;
    memcpy(data + 1, payload, length);
    return HAL_FDCAN_AddMessageToTxFifoQ(&can, &header, data) == HAL_OK;
}

static DeviceResult can_write(void *context, const uint8_t *data, unsigned length)
{
    (void)context;
    if (!link_ready || data == NULL || length == 0u) return DEV_INVALID;

    unsigned sent = 0u;
    uint8_t sequence = 0u;
    while (sent < length) {
        const unsigned chunk = (length - sent) < SEG_DATA ? (length - sent) : SEG_DATA;
        if (!can_send(sequence, data + sent, chunk)) {
            ++tx_fail;
            return DEV_IO;
        }
        sent += chunk;
        ++sequence;
    }
    return DEV_OK;
}

Transport board_can_transport(void)
{
    can_transport.context = NULL;
    can_transport.read = can_read;
    can_transport.write = can_write;
    return can_transport;
}

uint32_t board_can_errors(void)
{
    return rx_errors + rx_lost + tx_fail;
}

#else  /* feature disabled */

bool board_can_init(void) { return false; }
bool board_can_ready(void) { return false; }
Transport board_can_transport(void)
{
    const Transport none = {0};
    return none;
}
uint32_t board_can_errors(void) { return 0u; }
void FDCAN1_IT0_IRQHandler(void) {}
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *handle, uint32_t flags)
{
    (void)handle;
    (void)flags;
}

#endif
