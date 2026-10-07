#include "waveshare_rf_lcd_test.h"

#include <stdint.h>
#include "board_config.h"
#include "driver/parlio_rx.h"
#include "esp_attr.h"
#include "esp_log.h"

#define IQ_RATE_HZ     40000000u
#define RAW_RING_BYTES 32768u

static const char *TAG = "c5vrx_rf_lcd";
static DMA_ATTR __attribute__((aligned(64))) uint8_t s_raw_ring[RAW_RING_BYTES];
static parlio_rx_unit_handle_t s_rx;
static parlio_rx_delimiter_handle_t s_delimiter;

esp_err_t c5vrx_waveshare_rf_lcd_rx_start(void)
{
    const parlio_rx_unit_config_t cfg = {
        .trans_queue_depth = 1u,
        .max_recv_size = sizeof(s_raw_ring),
        .dma_burst_size = 32u,
        .data_width = 8u,
        .clk_src = PARLIO_CLK_SRC_DEFAULT,
        .ext_clk_freq_hz = 0u,
        .exp_clk_freq_hz = IQ_RATE_HZ,
        .clk_in_gpio_num = -1,
        .clk_out_gpio_num = -1,
        .valid_gpio_num = -1,
        .data_gpio_nums = C5VRX_IQ_GPIOS,
        .flags = {
            .free_clk = true,
            .clk_gate_en = false,
            .allow_pd = false,
        },
    };

    esp_err_t err = parlio_new_rx_unit(&cfg, &s_rx);
    if (err != ESP_OK) return err;

    const parlio_rx_soft_delimiter_config_t delim_cfg = {
        .sample_edge = PARLIO_SAMPLE_EDGE_POS,
        .bit_pack_order = PARLIO_BIT_PACK_ORDER_LSB,
        .eof_data_len = sizeof(s_raw_ring),
        .timeout_ticks = 0u,
    };
    err = parlio_new_rx_soft_delimiter(&delim_cfg, &s_delimiter);
    if (err != ESP_OK) return err;

    err = parlio_rx_unit_enable(s_rx, false);
    if (err != ESP_OK) return err;

    err = parlio_rx_soft_delimiter_start_stop(s_rx, s_delimiter, true);
    if (err != ESP_OK) return err;

    const parlio_receive_config_t receive_cfg = {
        .delimiter = s_delimiter,
        .flags = {
            .partial_rx_en = true,
            .indirect_mount = false,
        },
    };
    err = parlio_rx_unit_receive(s_rx, s_raw_ring, sizeof(s_raw_ring), &receive_cfg);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "PARLIO RX active: 8-bit @ 40 MS/s, ring=%u bytes; analog TX/DAC disabled",
             (unsigned)sizeof(s_raw_ring));
    return ESP_OK;
}
