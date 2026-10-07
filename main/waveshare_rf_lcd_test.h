#pragma once
#include "esp_err.h"

/* Start a receive-only 40 MS/s PARLIO capture for the Waveshare coexistence
 * test. The RF frontend must already be running. No PARLIO TX or BitScrambler
 * resource is created. */
esp_err_t c5vrx_waveshare_rf_lcd_rx_start(void);

/* Start the first-light software decoder. It observes the live IQ DMA ring,
 * mirrors the production Phase5 sync detector at 20 MS/s, logs PAL/NTSC line
 * timing, and paints recovered luma scanlines to the LCD. */
esp_err_t c5vrx_waveshare_rf_lcd_preview_start(void);
