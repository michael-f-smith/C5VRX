#include "waveshare_board_power.h"

#include "driver/i2c_master.h"
#include "esp_io_expander.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "custom_io_expander_ch32v003.h"

#define WS_I2C_PORT I2C_NUM_0
#define WS_I2C_SDA  0
#define WS_I2C_SCL  1
#define WS_CH32_ADDR CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS

#define WS_LCD_RST IO_EXPANDER_PIN_NUM_1

static const char *TAG = "waveshare_power";
static i2c_master_bus_handle_t s_i2c;
static esp_io_expander_handle_t s_expander;

esp_err_t c5vrx_waveshare_board_power_init(void)
{
    if (s_expander) {
        return ESP_OK;
    }

    /*
     * Match Waveshare's ESP-IDF-V554/01_ch32_test bus configuration exactly.
     * Keep this deliberately minimal while bringing the board up.
     */
    const i2c_master_bus_config_t i2c_cfg = {
        .i2c_port = WS_I2C_PORT,
        .sda_io_num = WS_I2C_SDA,
        .scl_io_num = WS_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&i2c_cfg, &s_i2c);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "I2C0 initialized: SDA=GPIO%d SCL=GPIO%d",
             WS_I2C_SDA, WS_I2C_SCL);

    /*
     * Probe the CH32 before constructing the expander object.  This makes a
     * wiring/address/NACK failure distinguishable from a driver reset failure.
     */
    err = i2c_master_probe(s_i2c, WS_CH32_ADDR, 250);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CH32V003 not responding at I2C address 0x%02x: %s",
                 WS_CH32_ADDR, esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "CH32V003 ACK at I2C address 0x%02x", WS_CH32_ADDR);

    err = custom_io_expander_new_i2c_ch32v003(
        s_i2c, WS_CH32_ADDR, &s_expander);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CH32V003 expander init failed: %s", esp_err_to_name(err));
        s_expander = NULL;
        return err;
    }
    ESP_LOGI(TAG, "CH32V003 expander initialized");

    err = esp_io_expander_set_dir(s_expander, WS_LCD_RST, IO_EXPANDER_OUTPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD reset direction setup failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_io_expander_set_level(s_expander, WS_LCD_RST, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD reset release failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t c5vrx_waveshare_lcd_reset(void)
{
    esp_err_t err = c5vrx_waveshare_board_power_init();
    if (err != ESP_OK) return err;

    err = esp_io_expander_set_level(s_expander, WS_LCD_RST, 0);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(200));

    err = esp_io_expander_set_level(s_expander, WS_LCD_RST, 1);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(200));

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
