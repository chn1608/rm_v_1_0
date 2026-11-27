#include "keys.h"
#include "board_config.h"

#define RUNG_COUNT 5u

Key key_classify_mv(uint16_t mv)
{
    static const uint16_t rung[RUNG_COUNT] = {0u, 660u, 1320u, 1980u, 2640u};

    if (mv > BOARD_KEY_ADC_VREF_MV) return KEY_UNKNOWN;  /* above the rail */
    if (mv >= BOARD_KEY_DETECT_MV) return KEY_NONE;      /* released */

    unsigned best = 0u;
    unsigned best_distance = 0xFFFFu;
    for (unsigned i = 0; i < RUNG_COUNT; ++i) {
        const unsigned distance = mv > rung[i] ? (unsigned)(mv - rung[i])
                                               : (unsigned)(rung[i] - mv);
        if (distance < best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    /* Rung order is centre, left, right, up, down. */
    return (Key)(best + (unsigned)KEY_CENTER);
}

bool key_update(KeyFilter *filter, Key sample, Key *changed_to)
{
    if (sample == KEY_UNKNOWN) {
        filter->repeats = 0u;
        return false;
    }
    if (sample != filter->candidate) {
        filter->candidate = sample;
        filter->repeats = 0u;
    }
    if (filter->repeats < 3u) ++filter->repeats;
    if (filter->repeats < 3u || sample == filter->stable) return false;
    filter->stable = sample;
    *changed_to = sample;
    return true;
}
