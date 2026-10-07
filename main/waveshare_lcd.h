#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C5VRX_LCD_WIDTH  320u
#define C5VRX_LCD_HEIGHT 172u

/** Initialize the onboard ST7789 on the Waveshare ESP32-C5-LCD-1.47.
 *
 * The native panel is 172x320 portrait and is configured here as 320x172
 * landscape. Reset (GPIO26) and backlight (GPIO10) are direct ESP32 GPIOs;
 * this board has no CH32V003 I/O expander.
 */
esp_err_t c5vrx_waveshare_lcd_init(void);
esp_err_t c5vrx_waveshare_lcd_draw_line(unsigned y, const uint16_t *pixels);
esp_err_t c5vrx_waveshare_lcd_draw_bitmap(unsigned x, unsigned y,
                                          unsigned width, unsigned height,
                                          const uint16_t *pixels);
esp_err_t c5vrx_waveshare_lcd_fill(uint16_t rgb565);

#ifdef __cplusplus
}
#endif
