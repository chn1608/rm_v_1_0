#ifndef RM_LCD_RUNTIME_H
#define RM_LCD_RUNTIME_H

#include "hmi.h"

typedef void (*LcdSnapshotProvider)(HmiSnapshot *snapshot);

typedef struct {
    uint32_t stage, fault, heartbeat, adc_errors, key_changes, seen_keys;
    uint16_t adc_raw, key_mv;
    uint8_t page, key_candidate, key_stable;
    bool panel_ready, adc_ready, key_sample_valid;
} LcdRuntimeStatus;

extern volatile LcdRuntimeStatus lcd_runtime;
bool lcd_runtime_start(LcdSnapshotProvider provider);

#endif
