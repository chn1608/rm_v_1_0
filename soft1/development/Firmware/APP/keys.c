#include "keys.h"

Key key_classify_mv(uint16_t mv)
{
    static const uint16_t nominal[] = {3300, 0, 660, 1320, 1980, 2640};
    for (unsigned i = 0; i < sizeof(nominal) / sizeof(nominal[0]); ++i) {
        const int difference = (int)mv - (int)nominal[i];
        if (difference >= -180 && difference <= 180 && mv <= 3300u) return (Key)i;
    }
    return KEY_UNKNOWN;
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
