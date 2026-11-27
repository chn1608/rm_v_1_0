#include "keys.h"
#include "board_config.h"

/* Windows copied from the official example; see keys.h for the source path. */
Key key_classify_raw(uint16_t raw)
{
    if (raw <= BOARD_KEY_WIN_CENTER_MAX) return KEY_CENTER;
    if (raw >= BOARD_KEY_WIN_RIGHT_MIN && raw <= BOARD_KEY_WIN_RIGHT_MAX) return KEY_RIGHT;
    if (raw >= BOARD_KEY_WIN_LEFT_MIN && raw <= BOARD_KEY_WIN_LEFT_MAX) return KEY_LEFT;
    if (raw >= BOARD_KEY_WIN_UP_MIN && raw <= BOARD_KEY_WIN_UP_MAX) return KEY_UP;
    if (raw >= BOARD_KEY_WIN_DOWN_MIN && raw <= BOARD_KEY_WIN_DOWN_MAX) return KEY_DOWN;
    return KEY_NONE;
}

bool key_update(KeyFilter *filter, Key sample, uint32_t now_ms, Key *changed_to)
{
    if (sample == KEY_UNKNOWN) {
        filter->candidate = KEY_UNKNOWN;
        filter->candidate_since = now_ms;
        return false;
    }

    if (sample != filter->candidate) {
        filter->candidate = sample;
        filter->candidate_since = now_ms;
        return false;
    }

    /* The level has to survive the whole debounce window; the RC ramp after a
     * release only lingers on an intermediate rung for a few milliseconds. */
    if (now_ms - filter->candidate_since < CHEN_KEY_STABLE_MS) return false;

    if (filter->last_event_ms != 0u && now_ms - filter->last_event_ms < CHEN_KEY_BLANK_MS)
        return false;

    if (sample == filter->stable) return false;

    filter->stable = sample;
    filter->last_event_ms = now_ms;
    *changed_to = sample;
    return true;
}
