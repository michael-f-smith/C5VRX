#include "waveshare_lcd.h"

#include <stdbool.h>
#include <stdlib.h>
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_log.h"

#define LCD_HOST       SPI2_HOST
#define LCD_SCLK_GPIO  6
#define LCD_MOSI_GPIO  7
#define LCD_DC_GPIO    9
#define LCD_CS_GPIO    10
#define LCD_PCLK_HZ    (80 * 1000 * 1000)

static const char *TAG = "c5vrx_lcd";
static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t s_panel;
static bool s_ready;

esp_err_t c5vrx_waveshare_lcd_init(void)
{
    if (s_ready) return ESP_OK;

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
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &s_io);
    if (err != ESP_OK) return err;

    /* LCD reset/backlight are controlled by the Waveshare CH32V003 expander. */
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    err = esp_lcd_new_panel_st7789(s_io, &panel_cfg, &s_panel);
    if (err != ESP_OK) return err;
    if ((err = esp_lcd_panel_init(s_panel)) != ESP_OK) return err;
    if ((err = esp_lcd_panel_invert_color(s_panel, true)) != ESP_OK) return err;
    if ((err = esp_lcd_panel_swap_xy(s_panel, true)) != ESP_OK) return err;
    if ((err = esp_lcd_panel_mirror(s_panel, true, false)) != ESP_OK) return err;
    if ((err = esp_lcd_panel_disp_on_off(s_panel, true)) != ESP_OK) return err;

    s_ready = true;
    ESP_LOGI(TAG, "ST7789 ready: %ux%u landscape @ %u MHz SPI",
             C5VRX_LCD_WIDTH, C5VRX_LCD_HEIGHT, LCD_PCLK_HZ / 1000000u);
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
    free(buf);
    return err;
}
