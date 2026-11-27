#ifndef CHEN_CONFIG_BOARD_CONFIG_H
#define CHEN_CONFIG_BOARD_CONFIG_H

/* Single source of truth for pin, peripheral and scaling constants.
 *
 * Every entry carries the evidence level from docs/GPIO_MAP.md:
 *   L0  fixed on the DM-MC-Board02 PCB, must follow the vendor schematic
 *   L1  wired and confirmed by a human
 *   L2  planned, not yet wired/validated, free to re-plan
 *   L3  newly assigned or reserved
 *
 * Nothing here is a measurement. ADC factors and servo limits are explicitly
 * marked as uncalibrated until bench data exists.
 */

/* ---- L0: on-board LCD module (DM_TFT-V1.1, ST7789V2, 240x280) ---------- */
#define BOARD_LCD_SPI_INSTANCE 1
#define BOARD_LCD_SCK_PIN 3u          /* PB3  SPI1_SCK  */
#define BOARD_LCD_MOSI_PIN 7u         /* PD7  SPI1_MOSI */
#define BOARD_LCD_CS_PORT 'E'
#define BOARD_LCD_CS_PIN 15u          /* PE15 */
#define BOARD_LCD_DC_PORT 'D'
#define BOARD_LCD_DC_PIN 10u          /* PD10 */
#define BOARD_LCD_BLK_PORT 'B'
#define BOARD_LCD_BLK_PIN 10u         /* PB10 */
#define BOARD_LCD_RST_PORT 'B'
#define BOARD_LCD_RST_PIN 11u         /* PB11 */
#define BOARD_LCD_ROW_OFFSET 20u      /* visible glass starts at RAM row 20 */
#define BOARD_LCD_WIDTH 240u
#define BOARD_LCD_HEIGHT 280u

/* ---- L0: five-way keys on the LCD module ------------------------------ */
#define BOARD_KEY_ADC_CHANNEL 19u     /* PA5 = ADC1_INP19 */
#define BOARD_KEY_ADC_GPIO_PIN 5u     /* GPIOA */
#define BOARD_KEY_ADC_FULL_SCALE 4095u
#define BOARD_KEY_ADC_VREF_MV 3300u   /* nominal; see key_calibration below */
#define BOARD_KEY_IDLE_MV 3300u
#define BOARD_KEY_DETECT_MV 3120u

/* Raw-count windows from the official DM-MC02 KEY example (see keys.h). */
#define BOARD_KEY_WIN_CENTER_MAX 200u
#define BOARD_KEY_WIN_RIGHT_MIN 700u
#define BOARD_KEY_WIN_RIGHT_MAX 1000u
#define BOARD_KEY_WIN_LEFT_MIN 1500u
#define BOARD_KEY_WIN_LEFT_MAX 1800u
#define BOARD_KEY_WIN_UP_MIN 2200u
#define BOARD_KEY_WIN_UP_MAX 2500u
#define BOARD_KEY_WIN_DOWN_MIN 2800u
#define BOARD_KEY_WIN_DOWN_MAX 3500u

/* ---- L0: debug UART (USART1) ------------------------------------------ */
#define BOARD_DEBUG_UART 1u
#define BOARD_DEBUG_TX_PIN 9u         /* PA9  */
#define BOARD_DEBUG_RX_PIN 10u        /* PA10 */

/* ---- L2: ATK-BLE02 on UART7 ------------------------------------------- */
#define BOARD_BLE_UART 7u
#define BOARD_BLE_TX_PIN 8u           /* PE8 */
#define BOARD_BLE_RX_PIN 7u           /* PE7 */
#define BOARD_BLE_LINK_PIN 14u        /* PD14, active low when connected */
#define BOARD_BLE_STA_PIN 15u         /* PD15 */

/* ---- L2: chassis motors (TB6612FNG) ----------------------------------- */
#define BOARD_MOTOR_PWM_A_PIN 0u      /* PA0  TIM2_CH1 */
#define BOARD_MOTOR_PWM_B_PIN 2u      /* PA2  TIM2_CH3 */
#define BOARD_MOTOR_AIN1_PIN 13u      /* PE13 */
#define BOARD_MOTOR_AIN2_PIN 14u      /* PE14 */
#define BOARD_MOTOR_BIN1_PIN 8u       /* PD8  (L3, was USART3_TX) */
#define BOARD_MOTOR_BIN2_PIN 9u       /* PD9  (L3, was USART3_RX) */
#define BOARD_MOTOR_STBY_PIN 1u       /* PA1 */
#define BOARD_MOTOR_PWM_PERIOD_TICKS 1000u  /* 20 kHz from 200 MHz / 1000 */

/* ---- L2: steering servo (LD-220MG) ------------------------------------ */
#define BOARD_SERVO_TIM_CHANNEL 1u    /* PE9  TIM1_CH1 */
#define BOARD_SERVO_PERIOD_US 20000u
#define BOARD_SERVO_TIMER_HZ 1000000u

/* ---- L2: power stage --------------------------------------------------- */
#define BOARD_RELAY_K1_PIN 0u         /* PB0 */
#define BOARD_RELAY_K2_PIN 3u         /* PA3 */
#define BOARD_ESTOP_ADC_CHANNEL 3u    /* PA6 = ADC1_INP3, contact voltage */
#define BOARD_ESTOP_ADC_PIN 6u
#define BOARD_VBAT_ADC_CHANNEL 4u     /* PC4 = ADC1_INP4 */
#define BOARD_VBAT_ADC_PIN 4u
#define BOARD_VBAT_DIVIDER_NUM 11u    /* R86 100k / R87 10k -> 11:1 */
#define BOARD_VBAT_DIVIDER_DEN 1u

/* ---- L2: CAN1 (PD0/PD1, on-board transceiver) ------------------------- */
#define BOARD_CAN_RX_ID 0x600u        /* PC -> MCU  */
#define BOARD_CAN_TX_ID 0x601u        /* MCU -> PC  */
#define BOARD_CAN_BITRATE 1000000u    /* classic CAN, 1 Mbit/s */

/* ---- L0: board indicators --------------------------------------------- */
#define BOARD_BUZZER_PIN 15u          /* PB15 */
#define BOARD_USER_KEY_PIN 15u        /* PA15, active low, 100 ms debounce */

/* ---- Scaling helpers --------------------------------------------------- */
/* Battery pack millivolts from a raw 12-bit sample. The divider is documented
 * but not yet calibrated against a meter, so the result is marked uncalibrated
 * by the caller until ADC_CALIBRATED is set. */
#define BOARD_VBAT_MV(raw) \
    ((int32_t)(((uint32_t)(raw) * BOARD_KEY_ADC_VREF_MV * BOARD_VBAT_DIVIDER_NUM) / \
               (BOARD_KEY_ADC_FULL_SCALE * BOARD_VBAT_DIVIDER_DEN)))

/* Servo travel. Defaults are the LD-220MG datasheet centre (1500 us) and the
 * mechanical end stops are deliberately narrower than the electrical range. */
#define BOARD_SERVO_MIN_US 500u
#define BOARD_SERVO_CENTER_US 1500u
#define BOARD_SERVO_MAX_US 2500u
#define BOARD_SERVO_MIN_MDEG (-90000)
#define BOARD_SERVO_CENTER_MDEG 0
#define BOARD_SERVO_MAX_MDEG 90000
#define BOARD_SERVO_SLEW_US_PER_TICK 8u   /* 8 us per 20 ms task tick */

/* ---- Limits ------------------------------------------------------------ */
#define BOARD_MOTOR_LIMIT_PERMILLE 1000
#define BOARD_MOTOR_RAMP_STEP 40          /* permille per 5 ms tick */
#define BOARD_MOTOR_REVERSAL_TICKS 20u    /* 100 ms at zero before reversing */
#define BOARD_VBAT_MIN_MV 12000           /* 4S pack: 3.0 V/cell hard floor */
#define BOARD_VBAT_MAX_MV 17400           /* 4.35 V/cell */

#endif
