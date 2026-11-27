#ifndef CHEN_DEVICE_KEYS_H
#define CHEN_DEVICE_KEYS_H
#include "devices.h"

/* Five-way key decoding for the DM_TFT-V1.1 ladder and its debounce filter.
 *
 * Ladder (module schematic, measured values):
 *   0.000 V centre, 0.658 V left, 1.318 V right, 1.980 V up, 2.643 V down,
 *   3.300 V released (100K pull-up).
 * Levels are fractions of the rail, so classification uses the nearest rung
 * rather than fixed windows: a few percent of ADC gain error must not drop a
 * key into an unassigned gap (that made up/down dead on the bench).
 */
typedef struct {
    Key stable, candidate;
    uint8_t repeats;
} KeyFilter;

Key key_classify_mv(uint16_t mv);
bool key_update(KeyFilter *filter, Key sample, Key *changed_to);

#endif
