#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the Waveshare board I2C bus and CH32V003 I/O expander. */
esp_err_t c5vrx_waveshare_board_power_init(void);

/** Pulse the ST7789 reset line through the CH32V003 expander. */
esp_err_t c5vrx_waveshare_lcd_reset(void);

/** Set LCD backlight PWM (0..100 percent) through the CH32V003 expander. */
esp_err_t c5vrx_waveshare_backlight_set(unsigned percent);

#ifdef __cplusplus
}
#endif
