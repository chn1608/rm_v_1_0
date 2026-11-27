#ifndef CHEN_BSP_LCD_H
#define CHEN_BSP_LCD_H
#include <stdbool.h>
#include <stdint.h>

/* ST7789V2 panel on SPI1 (DM_TFT-V1.1). 240x280 visible inside a 240x320 RAM. */
#define PANEL_WIDTH 240u
#define PANEL_HEIGHT 280u

bool smoke_panel_init(void);
void smoke_panel_set_backlight(bool enabled);
bool smoke_panel_fill(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t rgb565);
/* RGB565, high byte first, row major. Used by the HMI strip renderer. */
bool smoke_panel_blit565(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                         const uint8_t *source);

#endif
