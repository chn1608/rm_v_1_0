#include "panel.h"
#include "stm32h7xx_hal.h"

/* DM_TFT-V1.1 wiring and portrait register values:
 * dmBots/DM-MC02, d65a368, CtrBoard-H7_ALL/Bsp/lcd.c.
 * The 240x280 glass occupies rows 20..299 of the ST7789V2 RAM.
 */
#define LCD_CS GPIO_PIN_15
#define LCD_DC GPIO_PIN_10
#define LCD_BLK GPIO_PIN_10
#define LCD_RESET GPIO_PIN_11
#define LCD_ROW_OFFSET 20u
#define SPI_TIMEOUT_MS 20u
#define ADC_TIMEOUT_MS 2u
#define PIXEL_BATCH 240u

enum {
    LCD_SLEEP_OUT = 0x11,
    LCD_DISPLAY_ON = 0x29,
    LCD_COLUMN_ADDRESS = 0x2a,
    LCD_ROW_ADDRESS = 0x2b,
    LCD_MEMORY_WRITE = 0x2c
};

static SPI_HandleTypeDef lcd_spi;
static ADC_HandleTypeDef key_adc;
static uint8_t pixels[2u * PIXEL_BATCH];

static void init_lcd_pins(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOE, LCD_CS, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOD, LCD_DC, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, LCD_BLK | LCD_RESET, GPIO_PIN_RESET);

    GPIO_InitTypeDef pin = {0};
    pin.Mode = GPIO_MODE_OUTPUT_PP;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_LOW;
    pin.Pin = LCD_CS;
    HAL_GPIO_Init(GPIOE, &pin);
    pin.Pin = LCD_DC;
    HAL_GPIO_Init(GPIOD, &pin);
    pin.Pin = LCD_BLK | LCD_RESET;
    HAL_GPIO_Init(GPIOB, &pin);

    pin.Mode = GPIO_MODE_AF_PP;
    pin.Speed = GPIO_SPEED_FREQ_MEDIUM;
    pin.Alternate = GPIO_AF5_SPI1;
    pin.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOB, &pin);
    pin.Pin = GPIO_PIN_7;
    HAL_GPIO_Init(GPIOD, &pin);
}

static bool spi_write(const uint8_t *data, uint16_t length)
{
    return HAL_SPI_Transmit(&lcd_spi, data, length, SPI_TIMEOUT_MS) == HAL_OK;
}

static bool command(uint8_t opcode, const uint8_t *data, uint16_t length)
{
    HAL_GPIO_WritePin(GPIOD, LCD_DC, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, LCD_CS, GPIO_PIN_RESET);
    bool ok = spi_write(&opcode, 1u);
    if (ok && length != 0u) {
        HAL_GPIO_WritePin(GPIOD, LCD_DC, GPIO_PIN_SET);
        ok = spi_write(data, length);
    }
    HAL_GPIO_WritePin(GPIOE, LCD_CS, GPIO_PIN_SET);
    return ok;
}

bool smoke_panel_init(void)
{
    /* PLL1Q is 80 MHz with the shared board clock configuration. */
    RCC_PeriphCLKInitTypeDef clock = {0};
    clock.PeriphClockSelection = RCC_PERIPHCLK_SPI1;
    clock.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL;
    if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK) return false;
    __HAL_RCC_SPI1_CLK_ENABLE();
    init_lcd_pins();

    lcd_spi.Instance = SPI1;
    lcd_spi.Init.Mode = SPI_MODE_MASTER;
    lcd_spi.Init.Direction = SPI_DIRECTION_2LINES_TXONLY;
    lcd_spi.Init.DataSize = SPI_DATASIZE_8BIT;
    /* ST7789V2 4-wire interface latches on SCL rising edges. */
    lcd_spi.Init.CLKPolarity = SPI_POLARITY_LOW;
    lcd_spi.Init.CLKPhase = SPI_PHASE_1EDGE;
    lcd_spi.Init.NSS = SPI_NSS_SOFT;
    lcd_spi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    lcd_spi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    lcd_spi.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
    lcd_spi.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
    if (HAL_SPI_Init(&lcd_spi) != HAL_OK) return false;

    HAL_Delay(100u);
    HAL_GPIO_WritePin(GPIOB, LCD_RESET, GPIO_PIN_SET);
    HAL_Delay(120u);
    if (!command(LCD_SLEEP_OUT, NULL, 0u)) return false;
    HAL_Delay(120u);

    static const struct {
        uint8_t opcode;
        uint8_t length;
        uint8_t data[14];
    } init[] = {
        {0x36, 1, {0x00}},                              /* Portrait RGB */
        {0x3a, 1, {0x05}},                              /* RGB565 */
        {0xb2, 5, {0x0c, 0x0c, 0x00, 0x33, 0x33}},      /* Porch */
        {0xb7, 1, {0x35}},                              /* Gate */
        {0xbb, 1, {0x32}},                              /* VCOM */
        {0xc2, 1, {0x01}},                              /* VDV/VRH enable */
        {0xc3, 1, {0x15}},                              /* VRH */
        {0xc4, 1, {0x20}},                              /* VDV */
        {0xc6, 1, {0x0f}},                              /* Frame rate */
        {0xd0, 2, {0xa4, 0xa1}},                         /* Power */
        {0xe0, 14, {0xd0, 0x08, 0x0e, 0x09, 0x09, 0x05, 0x31,
                    0x33, 0x48, 0x17, 0x14, 0x15, 0x31, 0x34}},
        {0xe1, 14, {0xd0, 0x08, 0x0e, 0x09, 0x09, 0x15, 0x31,
                    0x33, 0x48, 0x17, 0x14, 0x15, 0x31, 0x34}},
        {0x21, 0, {0}},                                 /* Display inversion */
        {LCD_DISPLAY_ON, 0, {0}},
    };
    for (unsigned i = 0; i < sizeof(init) / sizeof(init[0]); ++i) {
        if (!command(init[i].opcode, init[i].data, init[i].length)) return false;
    }
    return true;
}

void smoke_panel_set_backlight(bool enabled)
{
    HAL_GPIO_WritePin(GPIOB, LCD_BLK, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool address(uint8_t opcode, uint16_t first, uint16_t last)
{
    const uint8_t data[] = {(uint8_t)(first >> 8), (uint8_t)first,
                            (uint8_t)(last >> 8), (uint8_t)last};
    return command(opcode, data, sizeof(data));
}

bool smoke_panel_fill(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t rgb565)
{
    if (width == 0u || height == 0u || x + width > PANEL_WIDTH || y + height > PANEL_HEIGHT)
        return false;
    const uint16_t row = y + LCD_ROW_OFFSET;
    if (!address(LCD_COLUMN_ADDRESS, x, x + width - 1u) ||
        !address(LCD_ROW_ADDRESS, row, row + height - 1u))
        return false;

    for (unsigned i = 0; i < sizeof(pixels); i += 2u) {
        pixels[i] = (uint8_t)(rgb565 >> 8);
        pixels[i + 1u] = (uint8_t)rgb565;
    }

    const uint8_t opcode = LCD_MEMORY_WRITE;
    HAL_GPIO_WritePin(GPIOD, LCD_DC, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, LCD_CS, GPIO_PIN_RESET);
    bool ok = spi_write(&opcode, 1u);
    HAL_GPIO_WritePin(GPIOD, LCD_DC, GPIO_PIN_SET);
    uint32_t remaining = (uint32_t)width * height;
    while (ok && remaining != 0u) {
        const uint16_t batch = remaining < PIXEL_BATCH ? (uint16_t)remaining : PIXEL_BATCH;
        ok = spi_write(pixels, (uint16_t)(2u * batch));
        remaining -= batch;
    }
    HAL_GPIO_WritePin(GPIOE, LCD_CS, GPIO_PIN_SET);
    return ok;
}

bool smoke_panel_blit565(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                         const uint8_t *source)
{
    if (source == NULL || width == 0u || height == 0u ||
        x + width > PANEL_WIDTH || y + height > PANEL_HEIGHT)
        return false;
    const uint16_t row = y + LCD_ROW_OFFSET;
    if (!address(LCD_COLUMN_ADDRESS, x, x + width - 1u) ||
        !address(LCD_ROW_ADDRESS, row, row + height - 1u))
        return false;

    const uint8_t opcode = LCD_MEMORY_WRITE;
    HAL_GPIO_WritePin(GPIOD, LCD_DC, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, LCD_CS, GPIO_PIN_RESET);
    bool ok = spi_write(&opcode, 1u);
    HAL_GPIO_WritePin(GPIOD, LCD_DC, GPIO_PIN_SET);
    /* Rows are row-major RGB565 with the high byte first, which is the order
     * the ST7789V2 expects after the memory write command. */
    uint32_t remaining = (uint32_t)width * height * 2u;
    while (ok && remaining != 0u) {
        const uint16_t batch = remaining < (uint32_t)(2u * PIXEL_BATCH)
                                   ? (uint16_t)remaining
                                   : (uint16_t)(2u * PIXEL_BATCH);
        ok = spi_write(source, batch);
        source += batch;
        remaining -= batch;
    }
    HAL_GPIO_WritePin(GPIOE, LCD_CS, GPIO_PIN_SET);
    return ok;
}

bool smoke_key_adc_init(void)
{
    /* The reference PLL2 settings produce 96 MHz at PLL2P. */
    RCC_PeriphCLKInitTypeDef clock = {0};
    clock.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    clock.PLL2.PLL2M = 2u;
    clock.PLL2.PLL2N = 16u;
    clock.PLL2.PLL2P = 2u;
    clock.PLL2.PLL2Q = 2u;
    clock.PLL2.PLL2R = 2u;
    clock.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
    clock.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    clock.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
    if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK) return false;
    __HAL_RCC_ADC12_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef pin = {0};
    pin.Pin = GPIO_PIN_5;
    pin.Mode = GPIO_MODE_ANALOG;
    pin.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &pin);

    key_adc.Instance = ADC1;
    key_adc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV8;
    key_adc.Init.Resolution = ADC_RESOLUTION_12B;
    key_adc.Init.ScanConvMode = ADC_SCAN_DISABLE;
    key_adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    key_adc.Init.NbrOfConversion = 1u;
    key_adc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    key_adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    key_adc.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    key_adc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    key_adc.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
    if (HAL_ADC_Init(&key_adc) != HAL_OK) return false;

    ADC_ChannelConfTypeDef channel = {0};
    channel.Channel = ADC_CHANNEL_19;
    channel.Rank = ADC_REGULAR_RANK_1;
    channel.SingleDiff = ADC_SINGLE_ENDED;
    channel.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;
    channel.OffsetNumber = ADC_OFFSET_NONE;
    if (HAL_ADC_ConfigChannel(&key_adc, &channel) != HAL_OK) return false;
    return HAL_ADCEx_Calibration_Start(&key_adc, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) == HAL_OK;
}

bool smoke_key_adc_sample(uint16_t *raw)
{
    if (HAL_ADC_Start(&key_adc) != HAL_OK) return false;
    const bool ready = HAL_ADC_PollForConversion(&key_adc, ADC_TIMEOUT_MS) == HAL_OK;
    if (ready) *raw = (uint16_t)HAL_ADC_GetValue(&key_adc);
    return HAL_ADC_Stop(&key_adc) == HAL_OK && ready;
}
