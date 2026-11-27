/* Clock tree adapted from the DM-MC-Board02 reference project
 * (mc02_control Core/Src/main.c) and the minimal pin set from board_config.h.
 * Copyright (c) 2025 STMicroelectronics; see the reference component licenses.
 */
#include "board.h"
#include "board_config.h"
#include "features.h"
#include "stm32h7xx_hal.h"

/* Legacy power-control pads on the MC02 PCB. They must stay low at reset. */
#define BOARD_FIXED_POWER_PINS (GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15)

static TIM_HandleTypeDef motor_pwm;
static TIM_HandleTypeDef servo_pwm;
static ADC_HandleTypeDef analog_adc;

static GPIO_TypeDef *port(char which)
{
    switch (which) {
    case 'A': return GPIOA;
    case 'B': return GPIOB;
    case 'C': return GPIOC;
    case 'D': return GPIOD;
    default: return GPIOE;
    }
}

static void output(char which, uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port(which), &gpio);
    HAL_GPIO_WritePin(port(which), pin, GPIO_PIN_RESET);
}

void board_safe_defaults(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOC, BOARD_FIXED_POWER_PINS, GPIO_PIN_RESET);

    /* Motor bridge: both direction pins low (coast), STBY low (disabled). */
    output('E', GPIO_PIN_13 | GPIO_PIN_14);
    output('D', GPIO_PIN_8 | GPIO_PIN_9);
    output('A', GPIO_PIN_1);

    /* Relays off. The coil drivers are active high on the MC02 board. */
    output('B', GPIO_PIN_0);
    output('A', GPIO_PIN_3);
}

void board_fatal(void)
{
    __disable_irq();
    board_motor_off();
    (void)board_power_set(false);
    for (;;) { __WFI(); }
}

void board_clock_init(void)
{
    if (HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY) != HAL_OK) board_fatal();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    const uint32_t start = HAL_GetTick();
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
        if (HAL_GetTick() - start >= 100u) board_fatal();
    }

    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 2;
    osc.PLL.PLLN = 40;
    osc.PLL.PLLP = 1;     /* 480 MHz VCO / 1 -> 480 MHz SYSCLK */
    osc.PLL.PLLQ = 6;     /* 80 MHz for SPI and friends */
    osc.PLL.PLLR = 2;
    osc.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
    osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) board_fatal();

    RCC_ClkInitTypeDef clock = {0};
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

/* ------------------------------------------------------------------- motor */

void board_motor_off(void)
{
    if (motor_pwm.Instance == TIM2) {
        __HAL_TIM_SET_COMPARE(&motor_pwm, TIM_CHANNEL_1, 0u);
        __HAL_TIM_SET_COMPARE(&motor_pwm, TIM_CHANNEL_3, 0u);
    }
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13 | GPIO_PIN_14, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
}

static void motor_pin_af(uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &gpio);
}

bool board_pwm_init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_TIM1_CLK_ENABLE();

    motor_pin_af(GPIO_PIN_0);   /* TIM2_CH1 */
    motor_pin_af(GPIO_PIN_2);   /* TIM2_CH3 */

    /* 240 MHz timer clock: /12 = 20 MHz, /1000 = 20 kHz. */
    motor_pwm.Instance = TIM2;
    motor_pwm.Init.Prescaler = 11u;
    motor_pwm.Init.CounterMode = TIM_COUNTERMODE_UP;
    motor_pwm.Init.Period = 999u;
    motor_pwm.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    motor_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&motor_pwm) != HAL_OK) return false;

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0u;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&motor_pwm, &oc, TIM_CHANNEL_1) != HAL_OK) return false;
    if (HAL_TIM_PWM_ConfigChannel(&motor_pwm, &oc, TIM_CHANNEL_3) != HAL_OK) return false;
    if (HAL_TIM_PWM_Start(&motor_pwm, TIM_CHANNEL_1) != HAL_OK) return false;
    if (HAL_TIM_PWM_Start(&motor_pwm, TIM_CHANNEL_3) != HAL_OK) return false;

    /* Servo: PE9 TIM1_CH1, 1 MHz tick, 20 ms period. */
    GPIO_InitTypeDef servo_pin = {0};
    servo_pin.Pin = GPIO_PIN_9;
    servo_pin.Mode = GPIO_MODE_AF_PP;
    servo_pin.Pull = GPIO_NOPULL;
    servo_pin.Speed = GPIO_SPEED_FREQ_HIGH;
    servo_pin.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOE, &servo_pin);

    servo_pwm.Instance = TIM1;
    servo_pwm.Init.Prescaler = 239u;
    servo_pwm.Init.CounterMode = TIM_COUNTERMODE_UP;
    servo_pwm.Init.Period = BOARD_SERVO_PERIOD_US - 1u;
    servo_pwm.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    servo_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&servo_pwm) != HAL_OK) return false;
    TIM_OC_InitTypeDef soc = {0};
    soc.OCMode = TIM_OCMODE_PWM1;
    soc.Pulse = BOARD_SERVO_CENTER_US;
    soc.OCPolarity = TIM_OCPOLARITY_HIGH;
    if (HAL_TIM_PWM_ConfigChannel(&servo_pwm, &soc, TIM_CHANNEL_1) != HAL_OK) return false;
    return HAL_TIM_PWM_Start(&servo_pwm, TIM_CHANNEL_1) == HAL_OK;
}

void board_motor_set(int16_t left, int16_t right)
{
    /* Direction pins follow the sign, PWM the magnitude. A zero command leaves
     * both direction pins low, which is coast, not brake. */
    const uint16_t left_mag = (uint16_t)(left < 0 ? -left : left);
    const uint16_t right_mag = (uint16_t)(right < 0 ? -right : right);

    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, left > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, left < 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, right > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9, right < 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);

    __HAL_TIM_SET_COMPARE(&motor_pwm, TIM_CHANNEL_1,
                          (uint32_t)left_mag * BOARD_MOTOR_PWM_PERIOD_TICKS / 1000u);
    __HAL_TIM_SET_COMPARE(&motor_pwm, TIM_CHANNEL_3,
                          (uint32_t)right_mag * BOARD_MOTOR_PWM_PERIOD_TICKS / 1000u);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, (left != 0 || right != 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_servo_set(uint16_t pulse_us)
{
    __HAL_TIM_SET_COMPARE(&servo_pwm, TIM_CHANNEL_1, pulse_us);
}

/* ------------------------------------------------------------------- power */

bool board_power_set(bool enable)
{
    if (!CHEN_REAL_OUTPUTS && enable) return false;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return true;
}

bool board_relay_state(unsigned relay, bool *closed)
{
    (void)relay;
    (void)closed;
    /* Contact feedback is not wired yet (FEATURE_RELAY_FEEDBACK = 0). */
    return false;
}

/* ------------------------------------------------------------------ analog */

bool board_analog_init(void)
{
    __HAL_RCC_ADC12_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

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

    GPIO_InitTypeDef pin = {0};
    pin.Mode = GPIO_MODE_ANALOG;
    pin.Pull = GPIO_NOPULL;
    pin.Pin = GPIO_PIN_4 | GPIO_PIN_6;   /* PC4 battery, PA6 e-stop contact */
    HAL_GPIO_Init(GPIOA, &pin);
    pin.Pin = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOC, &pin);
    /* The five-way ladder is only a resistor network: if the module's 100K
     * pull-up is missing (J13 pin 10 not making contact) the pin floats and the
     * converter reads noise. The internal pull-up keeps the input defined and
     * still allows the ladder to divide it down. */
    pin.Pin = GPIO_PIN_5;                /* PA5 five-way ladder, external 100K pull-up */
    HAL_GPIO_Init(GPIOA, &pin);

    analog_adc.Instance = ADC1;
    analog_adc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV8;
    analog_adc.Init.Resolution = ADC_RESOLUTION_12B;
    /* One channel per conversion, EOC per conversion. Scanning three ranks and
     * polling with ADC_EOC_SEQ_CONV only ever produced the last rank and then
     * timed out, which killed the five-way keys entirely. */
    /* Fixed three-rank sequence programmed once at init. The channel must not
     * be switched at run time: re-configuring a rank while the converter is
     * enabled left the ADC converting the previous channel, which made the key
     * ladder read the battery pin (a constant ~0.2 V). */
    analog_adc.Init.ScanConvMode = ADC_SCAN_ENABLE;
    analog_adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    analog_adc.Init.NbrOfConversion = 3u;
    analog_adc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    analog_adc.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    analog_adc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    if (HAL_ADC_Init(&analog_adc) != HAL_OK) return false;

    /* A default rank keeps the ADC in a valid state; every read reconfigures
     * the channel it needs. */
    ADC_ChannelConfTypeDef ch = {0};
    ch.SamplingTime = ADC_SAMPLETIME_32CYCLES_5;
    ch.SingleDiff = ADC_SINGLE_ENDED;
    ch.OffsetNumber = ADC_OFFSET_NONE;
    /* HAL passes Channel straight to LL, which decodes bits 26..30. A plain
     * decimal (4/3/19) decodes to channel 0, so all three ranks silently
     * converted PA0_C and every reading was the same floating ~0.2 V. */
    ch.Channel = __LL_ADC_DECIMAL_NB_TO_CHANNEL(BOARD_VBAT_ADC_CHANNEL);
    ch.Rank = ADC_REGULAR_RANK_1;
    if (HAL_ADC_ConfigChannel(&analog_adc, &ch) != HAL_OK) return false;
    ch.Channel = __LL_ADC_DECIMAL_NB_TO_CHANNEL(BOARD_ESTOP_ADC_CHANNEL);
    ch.Rank = ADC_REGULAR_RANK_2;
    if (HAL_ADC_ConfigChannel(&analog_adc, &ch) != HAL_OK) return false;
    ch.Channel = __LL_ADC_DECIMAL_NB_TO_CHANNEL(BOARD_KEY_ADC_CHANNEL);
    ch.Rank = ADC_REGULAR_RANK_3;
    if (HAL_ADC_ConfigChannel(&analog_adc, &ch) != HAL_OK) return false;

    return HAL_ADCEx_Calibration_Start(&analog_adc, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) == HAL_OK;
}

/* One scan of the fixed three-rank sequence: battery, e-stop contact, key
 * ladder (ranks 1..3, programmed once in board_analog_init). The results are
 * cached so the three getters share one conversion set per control tick. */
static uint16_t analog_cache[3];
static bool analog_cache_ok;

void board_analog_refresh(void)
{
    analog_cache_ok = false;
    HAL_StatusTypeDef started = HAL_ADC_Start(&analog_adc);
    if (started == HAL_BUSY) {          /* recover a stale transaction */
        (void)HAL_ADC_Stop(&analog_adc);
        started = HAL_ADC_Start(&analog_adc);
    }
    if (started != HAL_OK) return;

    bool ok = true;
    for (unsigned i = 0; i < 3u; ++i) {
        if (HAL_ADC_PollForConversion(&analog_adc, 2u) != HAL_OK) { ok = false; break; }
        analog_cache[i] = (uint16_t)HAL_ADC_GetValue(&analog_adc);
    }
    (void)HAL_ADC_Stop(&analog_adc);
    analog_cache_ok = ok;
}

bool board_analog_key(uint16_t *raw_counts)
{
    if (raw_counts == NULL || !analog_cache_ok) return false;
    *raw_counts = analog_cache[2];   /* raw 12-bit counts, official thresholds */
    return true;
}

bool board_analog_battery(Reading *out)
{
    if (out == NULL) return false;
    if (!CHEN_FEATURE_BATTERY_ADC || !analog_cache_ok) {
        *out = (Reading){0, HAL_GetTick(), DATA_ABSENT, false};
        return false;
    }
    *out = (Reading){BOARD_VBAT_MV(analog_cache[0]), HAL_GetTick(), DATA_UNCALIBRATED,
                     analog_cache[0] != 0u};
    return true;
}

bool board_analog_estop(Reading *out)
{
    if (out == NULL) return false;
    if (!analog_cache_ok) {
        *out = (Reading){0, HAL_GetTick(), DATA_INVALID, false};
        return false;
    }
    /* The contact divider is not documented yet, so report the pin voltage and
     * keep the uncalibrated tag. */
    *out = (Reading){(int32_t)(((uint32_t)analog_cache[1] * BOARD_KEY_ADC_VREF_MV) /
                               BOARD_KEY_ADC_FULL_SCALE),
                     HAL_GetTick(), DATA_UNCALIBRATED, true};
    return true;
}
