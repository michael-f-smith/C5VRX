#pragma once
#include "esp_err.h"

/* Start a receive-only 40 MS/s PARLIO capture for the Waveshare coexistence
 * test. The RF frontend must already be running. No PARLIO TX or BitScrambler
 * resource is created. */
esp_err_t c5vrx_waveshare_rf_lcd_rx_start(void);
