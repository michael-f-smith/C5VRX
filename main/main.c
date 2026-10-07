/**
 * main.c - C5VRX application entry point.
 */

#include "sdkconfig.h"

#if CONFIG_C5VRX_WAVESHARE_LCD_SMOKE_TEST

#include "waveshare_lcd.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "c5vrx_lcd_test";
static uint16_t *s_smoke_frame;

static void lcd_smoke_test(void)
{
    const size_t pixels = C5VRX_LCD_WIDTH * C5VRX_LCD_HEIGHT;
    s_smoke_frame = heap_caps_malloc(pixels * sizeof(*s_smoke_frame),
                                     MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_ERROR_CHECK(s_smoke_frame ? ESP_OK : ESP_ERR_NO_MEM);

    static const uint16_t bars[] = {
        0xF800, /* red */
        0x07E0, /* green */
        0x001F, /* blue */
        0xFFFF, /* white */
        0x0000, /* black */
        0xFFE0, /* yellow */
        0x07FF, /* cyan */
        0xF81F, /* magenta */
    };
    const unsigned n_bars = sizeof(bars) / sizeof(bars[0]);

    for (unsigned y = 0; y < C5VRX_LCD_HEIGHT; ++y) {
        for (unsigned x = 0; x < C5VRX_LCD_WIDTH; ++x) {
            unsigned bar = (x * n_bars) / C5VRX_LCD_WIDTH;
            s_smoke_frame[y * C5VRX_LCD_WIDTH + x] = bars[bar];
        }
    }

    ESP_ERROR_CHECK(c5vrx_waveshare_lcd_draw_bitmap(
        0, 0, C5VRX_LCD_WIDTH, C5VRX_LCD_HEIGHT, s_smoke_frame));
    ESP_LOGI(TAG, "LCD smoke test running: 8 vertical color bars");
}

void app_main(void)
{
    ESP_LOGI(TAG, "Waveshare display-only smoke test; RF/video pipeline disabled");
    ESP_ERROR_CHECK(c5vrx_waveshare_lcd_init());
    lcd_smoke_test();
}

#elif CONFIG_C5VRX_WAVESHARE_RF_LCD_TEST

#include "waveshare_lcd.h"
#include "waveshare_rf_lcd_test.h"
#include "rf.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "c5vrx_rf_lcd_test";
static uint16_t *s_test_frame;

static void draw_coexistence_bars(void)
{
    const size_t pixels = C5VRX_LCD_WIDTH * C5VRX_LCD_HEIGHT;
    s_test_frame = heap_caps_malloc(pixels * sizeof(*s_test_frame),
                                    MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_ERROR_CHECK(s_test_frame ? ESP_OK : ESP_ERR_NO_MEM);
    static const uint16_t bars[] = {
        0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000, 0xFFE0, 0x07FF, 0xF81F,
    };
    for (unsigned y = 0; y < C5VRX_LCD_HEIGHT; ++y) {
        for (unsigned x = 0; x < C5VRX_LCD_WIDTH; ++x) {
            s_test_frame[y * C5VRX_LCD_WIDTH + x] =
                bars[(x * (sizeof(bars) / sizeof(bars[0]))) / C5VRX_LCD_WIDTH];
        }
    }
    ESP_ERROR_CHECK(c5vrx_waveshare_lcd_draw_bitmap(
        0, 0, C5VRX_LCD_WIDTH, C5VRX_LCD_HEIGHT, s_test_frame));
}

void app_main(void)
{
    ESP_LOGI(TAG, "Waveshare RF + LCD coexistence test; analog video TX/DAC disabled");
    ESP_LOGI(TAG, "IQ GPIO map: D0=1 D1=0 D2=25 D3=2 D4=9 D5=5 D6=3 D7=4");

    ESP_ERROR_CHECK(c5vrx_waveshare_lcd_init());
    draw_coexistence_bars();
    ESP_LOGI(TAG, "LCD heartbeat ready");

    ESP_ERROR_CHECK(rf_start());
    ESP_LOGI(TAG, "RF frontend ready");

    ESP_ERROR_CHECK(c5vrx_waveshare_rf_lcd_rx_start());
    ESP_LOGI(TAG, "RF/LCD coexistence test running");

    ESP_ERROR_CHECK(c5vrx_waveshare_rf_lcd_preview_start());
    ESP_LOGI(TAG, "first-light grayscale decoder running");
}

#else

#include "rf.h"
#include "board_config.h"
#include "video.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if CONFIG_C5VRX_BS_RELATIVE_WORKER_PROBE
#include "bs_relative_worker_probe.h"
#endif
#if CONFIG_C5VRX_BS_RELATIVE_MIDDLE_PROBE
#include "bs_relative_middle_probe.h"
#endif
#if CONFIG_C5VRX_BS_ADDCTIA_PROBE
#include "bs_addctia_probe.h"
#endif
#if CONFIG_C5VRX_PHY_PHASE_TAP_PROBE
#include "phy_phase_tap_probe.h"
#endif
#ifdef C5VRX4_EXPERIMENT
#include "c5vrx4.h"
#endif

void app_main(void)
{
    ESP_ERROR_CHECK(board_config_validate());
#if CONFIG_C5VRX_BS_RELATIVE_WORKER_PROBE
    bs_relative_worker_probe_run();
#endif
#if CONFIG_C5VRX_BS_RELATIVE_MIDDLE_PROBE
    bs_relative_middle_probe_run();
#endif
#if CONFIG_C5VRX_BS_ADDCTIA_PROBE
    bs_addctia_probe_run();
#endif
    ESP_ERROR_CHECK(rf_start());
#if CONFIG_C5VRX_PHY_PHASE_TAP_PROBE
    vTaskDelay(pdMS_TO_TICKS(8000));
    phy_phase_tap_probe_run();
#endif
    ESP_ERROR_CHECK(video_start());
#ifdef C5VRX4_EXPERIMENT
    c5vrx4_start();
#endif
}

#endif
