#include "waveshare_board_power.h"

#include "driver/i2c_master.h"
#include "esp_io_expander.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "custom_io_expander_ch32v003.h"

#define WS_I2C_PORT I2C_NUM_0
#define WS_I2C_SDA  0
#define WS_I2C_SCL  1

#define WS_LCD_RST IO_EXPANDER_PIN_NUM_1

static i2c_master_bus_handle_t s_i2c;
static esp_io_expander_handle_t s_expander;

esp_err_t c5vrx_waveshare_board_power_init(void)
{
    if (s_expander) return ESP_OK;

    const i2c_master_bus_config_t i2c_cfg = {
        .i2c_port = WS_I2C_PORT,
        .sda_io_num = WS_I2C_SDA,
        .scl_io_num = WS_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&i2c_cfg, &s_i2c);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    err = custom_io_expander_new_i2c_ch32v003(
        s_i2c, CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS, &s_expander);
    if (err != ESP_OK) return err;

    err = esp_io_expander_set_dir(s_expander, WS_LCD_RST, IO_EXPANDER_OUTPUT);
    if (err != ESP_OK) return err;
    return esp_io_expander_set_level(s_expander, WS_LCD_RST, 1);
}

esp_err_t c5vrx_waveshare_lcd_reset(void)
{
    esp_err_t err = c5vrx_waveshare_board_power_init();
    if (err != ESP_OK) return err;
    err = esp_io_expander_set_level(s_expander, WS_LCD_RST, 0);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(20));
    err = esp_io_expander_set_level(s_expander, WS_LCD_RST, 1);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(120));
    return ESP_OK;
}

esp_err_t c5vrx_waveshare_backlight_set(unsigned percent)
{
    esp_err_t err = c5vrx_waveshare_board_power_init();
    if (err != ESP_OK) return err;
    if (percent > 100u) percent = 100u;
    uint8_t pwm = (uint8_t)((percent * 255u) / 100u);
    return custom_io_expander_set_pwm(s_expander, pwm);
}
