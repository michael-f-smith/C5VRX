#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C5VRX_LCD_WIDTH  320u
#define C5VRX_LCD_HEIGHT 240u

/** Initialize the Waveshare ESP32-C5-Touch-LCD-2.8 ST7789 transport.
 *
 * The panel is configured in 320x240 landscape mode. LCD reset/backlight live
 * behind the board's CH32V003 I/O expander and are intentionally handled by
 * waveshare_board_power.c rather than pretending they are ESP32 GPIOs.
 */
esp_err_t c5vrx_waveshare_lcd_init(void);

/** Draw one RGB565 scanline. pixels must contain exactly 320 pixels. */
esp_err_t c5vrx_waveshare_lcd_draw_line(unsigned y, const uint16_t *pixels);

/** Draw a rectangular RGB565 block. Coordinates are in landscape orientation. */
esp_err_t c5vrx_waveshare_lcd_draw_bitmap(unsigned x, unsigned y,
                                          unsigned width, unsigned height,
                                          const uint16_t *pixels);

/** Fill the complete landscape panel with one RGB565 value. */
esp_err_t c5vrx_waveshare_lcd_fill(uint16_t rgb565);

#ifdef __cplusplus
}
#endif
