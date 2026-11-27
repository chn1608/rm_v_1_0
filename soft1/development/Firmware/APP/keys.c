#include "keys.h"

/* DM_TFT-V1.1 five-way ladder (module schematic, DM-MC02 d65a368):
 *
 *   0.000 V  centre   0R
 *   0.658 V  left     24.9K      fed from VCC_3V3 through a 100K pull-up
 *   1.318 V  right    66.5K
 *   1.980 V  up       150K
 *   2.643 V  down     402K
 *   3.300 V  released
 *
 * The rungs are fractions 0, 0.2, 0.4, 0.6 and 0.8 of the rail, so the absolute
 * voltages move with VDDA, the 100K pull-up and the divider tolerances. Fixed
 * +/-180 mV windows around the nominal values left unassigned gaps between
 * rungs; a reading that drifted into one (a few percent of ADC gain error is
 * enough) became KEY_UNKNOWN and the key looked dead. Classify by the nearest
 * rung instead, which assigns every reading to exactly one key and tolerates
 * roughly +/-15% of scale error. The released level keeps a slightly wider
 * band below the rail so a high-side gain error still reads as "down".
 *
 * Boundaries (mV): 330 | 990 | 1650 | 2310 | 3120 | rail
 */
#define KEY_RUNG_COUNT 5u

Key key_classify_mv(uint16_t mv)
{
    static const uint16_t rung[KEY_RUNG_COUNT] = {0u, 660u, 1320u, 1980u, 2640u};

    if (mv > 3300u) return KEY_UNKNOWN;  /* above the rail: not a ladder level */
    if (mv >= 3120u) return KEY_NONE;    /* released, pulled up to VCC_3V3 */

    unsigned best = 0u;
    unsigned best_distance = 0xffffu;
    for (unsigned i = 0; i < KEY_RUNG_COUNT; ++i) {
        const unsigned distance = mv > rung[i] ? (unsigned)(mv - rung[i])
                                               : (unsigned)(rung[i] - mv);
        if (distance < best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    /* Key order matches rung order: centre, left, right, up, down. */
    return (Key)(best + (unsigned)KEY_CENTER);
}

bool key_update(KeyFilter *f, Key sample, Key *changed_to)
{
    if (sample == KEY_UNKNOWN) {
        f->repeats = 0u;
        return false;
    }
    if (sample != f->candidate) {
        f->candidate = sample;
        f->repeats = 0u;
    }
    if (f->repeats < 3u) ++f->repeats;
    if (f->repeats < 3u || sample == f->stable) return false;
    f->stable = sample;
    *changed_to = sample;
    return true;
}
