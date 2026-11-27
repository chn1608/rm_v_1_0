#ifndef CHEN_DEVICE_KEYS_H
#define CHEN_DEVICE_KEYS_H
#include <stdint.h>
#include "devices.h"

/* Five-way key decoding for the DM_TFT-V1.1 module.
 *
 * Thresholds and the left/right assignment follow the official DM-MC02 KEY
 * example (gitee.com/kit-miao/dm-mc02, 例程/CtrBoard-H7_KEY/User/adc_modlue.c):
 *
 *   raw ADC count   key
 *   0    .. 200     centre
 *   700  .. 1000    right
 *   1500 .. 1800    left
 *   2200 .. 2500    up
 *   2800 .. 3500    down
 *   anything else   released
 *
 * Note this disagrees with the module schematic's voltage table, which swaps
 * left and right. The vendor example is the tested implementation for this
 * board, so it wins; see docs/AI_Collaboration/COLLABORATION.md.
 *
 * Debounce is time based and followed by a blanking window: releasing a key
 * lets the 100K/100nF node ramp back through the intermediate rungs, and a
 * sample-counting filter would report those as extra keys.
 */
#define CHEN_KEY_STABLE_MS 40u
#define CHEN_KEY_BLANK_MS 40u

typedef struct {
    Key candidate;
    Key stable;
    uint32_t candidate_since;
    uint32_t last_event_ms;
} KeyFilter;

Key key_classify_raw(uint16_t raw);
/* Returns true when a new stable key is accepted (KEY_NONE on release). */
bool key_update(KeyFilter *filter, Key sample, uint32_t now_ms, Key *changed_to);

#endif
