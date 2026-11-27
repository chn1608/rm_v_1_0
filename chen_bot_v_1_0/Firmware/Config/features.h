#ifndef CHEN_CONFIG_FEATURES_H
#define CHEN_CONFIG_FEATURES_H

/* Hardware feature switches.
 *
 * A disabled feature is not the same thing as a passed safety check. Anything
 * that is still 0 is compiled out, reported as "not fitted" and never treated
 * as a measurement. Enable a feature only after the corresponding wiring is
 * signed off in docs/HARDWARE_TODO.md.
 */

#ifndef CHEN_FEATURE_ACTUATION
#define CHEN_FEATURE_ACTUATION 0   /* TB6612 bridge + servo output stage */
#endif
#ifndef CHEN_FEATURE_LCD
#define CHEN_FEATURE_LCD 1
#endif
#ifndef CHEN_FEATURE_LCD_KEY
#define CHEN_FEATURE_LCD_KEY 1
#endif
#ifndef CHEN_FEATURE_BLE
#define CHEN_FEATURE_BLE 1
#endif
#ifndef CHEN_FEATURE_PC_DEBUG
#define CHEN_FEATURE_PC_DEBUG 1
#endif
#ifndef CHEN_FEATURE_BATTERY_ADC
#define CHEN_FEATURE_BATTERY_ADC 1
#endif
#ifndef CHEN_FEATURE_CURRENT_SENSOR
#define CHEN_FEATURE_CURRENT_SENSOR 0
#endif
#ifndef CHEN_FEATURE_RELAY_FEEDBACK
#define CHEN_FEATURE_RELAY_FEEDBACK 0
#endif
#ifndef CHEN_FEATURE_SERVO_FEEDBACK
#define CHEN_FEATURE_SERVO_FEEDBACK 0
#endif
#ifndef CHEN_FEATURE_ENCODER
#define CHEN_FEATURE_ENCODER 0
#endif
#ifndef CHEN_FEATURE_ESTOP_INPUT
#define CHEN_FEATURE_ESTOP_INPUT 0  /* hard-wired interlock, not yet fitted */
#endif
#ifndef CHEN_FEATURE_PARAM_NVM
#define CHEN_FEATURE_PARAM_NVM 0    /* Flash persistence of the parameter block */
#endif

/* Real actuation is compiled in only when the output stage has been reviewed
 * and the build is not the simulated one. */
#if CHEN_PROFILE == CHEN_SIM
#define CHEN_REAL_OUTPUTS 0
#else
#define CHEN_REAL_OUTPUTS CHEN_FEATURE_ACTUATION
#endif

#endif
