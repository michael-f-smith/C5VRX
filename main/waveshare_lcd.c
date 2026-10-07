#include "waveshare_lcd.h"

#include <stdbool.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LCD_HOST       SPI2_HOST
#define LCD_SCLK_GPIO  7
#define LCD_MOSI_GPIO  6
#define LCD_DC_GPIO    24
#define LCD_CS_GPIO    23
#define LCD_RST_GPIO   26
#define LCD_BL_GPIO    10
#define LCD_PCLK_HZ    (40 * 1000 * 1000)

#define LCD_BL_TIMER   LEDC_TIMER_0
#define LCD_BL_MODE    LEDC_LOW_SPEED_MODE
#define LCD_BL_CHANNEL LEDC_CHANNEL_0

static const char *TAG = "c5vrx_lcd";
static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t s_panel;
static bool s_ready;

static esp_err_t backlight_init(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LCD_BL_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LCD_BL_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) return err;

    const ledc_channel_config_t channel = {
        .gpio_num = LCD_BL_GPIO,
        .speed_mode = LCD_BL_MODE,
        .channel = LCD_BL_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LCD_BL_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    return ledc_channel_config(&channel);
}

static esp_err_t backlight_set(unsigned percent)
{
    if (percent > 100u) percent = 100u;
    uint32_t duty = (percent * 255u) / 100u;
    esp_err_t err = ledc_set_duty(LCD_BL_MODE, LCD_BL_CHANNEL, duty);
    if (err != ESP_OK) return err;
    return ledc_update_duty(LCD_BL_MODE, LCD_BL_CHANNEL);
}

esp_err_t c5vrx_waveshare_lcd_init(void)
{
    if (s_ready) return ESP_OK;

    ESP_RETURN_ON_ERROR(backlight_init(), TAG, "backlight init failed");

    const spi_bus_config_t bus = {
        .sclk_io_num = LCD_SCLK_GPIO,
        .mosi_io_num = LCD_MOSI_GPIO,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = C5VRX_LCD_WIDTH * 40u * sizeof(uint16_t),
    };
    esp_err_t err = spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = LCD_DC_GPIO,
        .cs_gpio_num = LCD_CS_GPIO,
        .pclk_hz = LCD_PCLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &s_io),
        TAG, "panel IO init failed");

    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(s_io, &panel_cfg, &s_panel),
                        TAG, "ST7789 create failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, true), TAG, "panel invert failed");

    /* Native panel is 172x320 portrait. Rotate into 320x172 landscape. */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, true), TAG, "swap XY failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, true, false), TAG, "mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(s_panel, 0, 34), TAG, "panel gap failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "display enable failed");

    s_ready = true;
    ESP_RETURN_ON_ERROR(backlight_set(80), TAG, "backlight enable failed");

    ESP_LOGI(TAG,
             "ESP32-C5-LCD-1.47 ST7789 ready: %ux%u landscape @ %u MHz SPI; CLK=%d MOSI=%d CS=%d DC=%d RST=%d BL=%d",
             C5VRX_LCD_WIDTH, C5VRX_LCD_HEIGHT, LCD_PCLK_HZ / 1000000u,
             LCD_SCLK_GPIO, LCD_MOSI_GPIO, LCD_CS_GPIO, LCD_DC_GPIO,
             LCD_RST_GPIO, LCD_BL_GPIO);
    return ESP_OK;
}

esp_err_t c5vrx_waveshare_lcd_draw_bitmap(unsigned x, unsigned y,
                                          unsigned width, unsigned height,
                                          const uint16_t *pixels)
{
    if (!s_ready || !pixels || width == 0 || height == 0) return ESP_ERR_INVALID_STATE;
    if (x + width > C5VRX_LCD_WIDTH || y + height > C5VRX_LCD_HEIGHT) return ESP_ERR_INVALID_ARG;
    return esp_lcd_panel_draw_bitmap(s_panel, x, y, x + width, y + height, pixels);
}

esp_err_t c5vrx_waveshare_lcd_draw_line(unsigned y, const uint16_t *pixels)
{
    if (y >= C5VRX_LCD_HEIGHT) return ESP_ERR_INVALID_ARG;
    return c5vrx_waveshare_lcd_draw_bitmap(0, y, C5VRX_LCD_WIDTH, 1, pixels);
}

esp_err_t c5vrx_waveshare_lcd_fill(uint16_t rgb565)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    const unsigned rows = 20;
    size_t count = C5VRX_LCD_WIDTH * rows;
    uint16_t *buf = heap_caps_malloc(count * sizeof(*buf), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!buf) return ESP_ERR_NO_MEM;
    for (size_t i = 0; i < count; ++i) buf[i] = rgb565;

    esp_err_t err = ESP_OK;
    for (unsigned y = 0; y < C5VRX_LCD_HEIGHT && err == ESP_OK; y += rows) {
        unsigned h = (y + rows <= C5VRX_LCD_HEIGHT) ? rows : (C5VRX_LCD_HEIGHT - y);
        err = c5vrx_waveshare_lcd_draw_bitmap(0, y, C5VRX_LCD_WIDTH, h, buf);
    }
    /*
     * Smoke-test helper only. Production video output will use persistent DMA
     * line buffers with transfer-completion synchronization.
     */
    vTaskDelay(pdMS_TO_TICKS(20));
    free(buf);
    return err;
}
