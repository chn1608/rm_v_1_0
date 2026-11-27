#ifndef RM_LCD_SMOKE_PANEL_H
#define RM_LCD_SMOKE_PANEL_H

#include <stdbool.h>
#include <stdint.h>

#define PANEL_WIDTH 240u
#define PANEL_HEIGHT 280u

bool smoke_panel_init(void);
void smoke_panel_set_backlight(bool enabled);
bool smoke_panel_fill(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t rgb565);
bool smoke_panel_blit565(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                         const uint8_t *source);
bool smoke_key_adc_init(void);
bool smoke_key_adc_sample(uint16_t *raw);

#endif
