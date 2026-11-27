#ifndef RM_KEYS_H
#define RM_KEYS_H
#include "devices.h"

typedef struct {
    Key stable, candidate;
    uint8_t repeats;
} KeyFilter;
Key key_classify_mv(uint16_t mv);
bool key_update(KeyFilter *filter, Key sample, Key *changed_to);
#endif
