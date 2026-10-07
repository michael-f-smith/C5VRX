#include "waveshare_rf_lcd_test.h"

#include <stdint.h>
#include "board_config.h"
#include "driver/parlio_rx.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "waveshare_lcd.h"

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


/* Exact raw-Q4/I4 -> Phase5 state map used by the production fm.bsasm path. */
static const uint8_t s_phase5_state_lut[256] = {
     4,6,7,7,7,8,8,8,24,24,24,25,25,25,26,28, 2,4,5,6,6,7,7,7,25,25,25,26,26,27,28,30,
     1,3,4,5,5,6,6,6,26,26,26,27,27,28,29,31, 1,2,3,4,5,5,5,6,26,27,27,27,28,29,30,31,
     1,2,3,3,4,5,5,5,27,27,27,28,29,29,30,31, 0,1,2,3,3,4,4,5,27,28,28,28,29,30,31,0,
     0,1,2,3,3,4,4,4,28,28,28,29,29,30,31,0, 0,1,2,2,3,3,4,4,28,28,29,29,30,30,31,0,
     16,15,14,14,13,13,12,12,20,20,19,19,18,18,17,16, 16,15,14,13,13,12,12,12,20,20,20,19,19,18,17,16,
     16,15,14,13,13,12,12,11,21,20,20,19,19,18,17,16, 15,14,13,13,12,12,11,11,21,21,21,20,19,19,18,17,
     15,14,13,12,11,11,11,10,22,21,21,21,20,19,18,17, 15,13,12,11,11,10,10,10,22,22,22,21,21,20,19,17,
     14,12,11,10,10,9,9,9,23,23,23,22,22,21,20,18, 12,10,9,9,9,8,8,8,24,24,24,23,23,23,22,20
};

static const uint8_t s_fm_delta_lut[1024] = {
    20,26,31,38,44,50,57,62,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,3,9,14,
    14,20,25,32,38,44,51,56,62,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,3,8,
    9,15,20,27,33,39,46,51,57,63,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,3,
    2,8,13,20,26,32,39,44,50,56,61,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,
    0,2,7,14,20,26,33,38,44,50,55,61,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,
    0,0,1,8,14,20,27,32,38,44,49,55,62,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,
    0,0,0,1,7,13,20,25,31,37,43,49,55,61,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,
    0,0,0,0,2,8,15,20,26,32,37,43,50,56,63,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,
    0,0,0,0,0,2,9,14,20,26,31,37,44,50,57,62,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,
    8,0,0,0,0,0,3,8,14,20,25,31,38,44,51,56,62,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,
    14,8,0,0,0,0,0,3,9,15,20,26,33,39,46,51,57,63,63,60,48,36,20,20,20,20,20,20,20,20,20,18,
    18,14,8,0,0,0,0,0,3,9,14,20,27,33,40,45,51,57,62,63,60,48,36,20,20,20,20,20,20,20,20,20,
    20,18,14,8,0,0,0,0,0,2,7,13,20,26,33,38,44,50,55,62,63,60,48,36,20,20,20,20,20,20,20,20,
    20,20,18,14,8,0,0,0,0,0,1,7,14,20,27,32,38,44,49,56,62,63,60,48,36,20,20,20,20,20,20,20,
    20,20,20,18,14,8,0,0,0,0,0,0,7,13,20,25,31,37,43,49,55,61,63,60,48,36,20,20,20,20,20,20,
    20,20,20,20,18,14,8,0,0,0,0,0,2,8,15,20,26,32,37,44,50,56,63,63,60,48,36,20,20,20,20,20,
    20,20,20,20,20,18,14,8,0,0,0,0,0,2,9,14,20,26,31,38,44,50,57,62,63,60,48,36,20,20,20,20,
    20,20,20,20,20,20,18,14,8,0,0,0,0,0,3,8,14,20,25,32,38,44,51,56,62,63,60,48,36,20,20,20,
    20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,3,9,15,20,27,33,39,46,51,57,63,63,60,48,36,20,20,
    20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,2,8,13,20,26,32,39,44,50,56,61,63,60,48,36,20,
    20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,2,7,14,20,26,33,38,44,50,55,62,63,60,48,36,
    36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,1,8,14,20,27,32,38,44,49,56,62,63,60,48,
    48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,1,7,13,20,25,31,37,43,49,55,62,63,60,
    60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,2,8,15,20,26,32,37,44,50,57,63,63,
    63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,2,9,14,20,26,31,38,44,51,57,62,
    62,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,3,8,14,20,25,32,38,45,51,56,
    57,63,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,3,9,15,20,27,33,40,46,51,
    50,56,61,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,2,8,13,20,26,33,39,44,
    44,50,55,62,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,2,7,14,20,27,33,38,
    37,43,49,55,61,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,0,7,13,20,26,31,
    31,37,43,49,55,61,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,1,7,14,20,25,
    26,32,37,44,50,56,63,63,60,48,36,20,20,20,20,20,20,20,20,20,18,14,8,0,0,0,0,0,2,9,15,20,
};

static const uint8_t s_phase5_sync_mask[128] = {
    0x00,0x00,0x80,0x3f,0x00,0x00,0x00,0xff,0x00,0x00,0x00,0xfe,0x03,0x00,0x00,0xfc,
    0x07,0x00,0x00,0xf8,0x0f,0x00,0x00,0xf0,0x1f,0x00,0x00,0xe0,0x3f,0x00,0x00,0xc0,
    0x3f,0x00,0x00,0x80,0xff,0x00,0x00,0x00,0xfe,0x00,0x00,0x00,0xfc,0x01,0x00,0x00,
    0xf8,0x07,0x00,0x00,0xf0,0x0f,0x00,0x00,0xe0,0x1f,0x00,0x00,0xc0,0x3f,0x00,0x00,
    0x80,0x3f,0x00,0x00,0x00,0xff,0x00,0x00,0x00,0xfe,0x00,0x00,0x00,0xfc,0x03,0x00,
    0x00,0xf8,0x07,0x00,0x00,0xf0,0x0f,0x00,0x00,0xe0,0x1f,0x00,0x00,0xc0,0x3f,0x00,
    0x00,0x80,0x3f,0x00,0x00,0x00,0xff,0x00,0x00,0x00,0xfe,0x00,0x00,0x00,0xfc,0x03,
    0x00,0x00,0xf8,0x07,0x00,0x00,0xf0,0x0f,0x00,0x00,0xe0,0x1f,0x00,0x00,0xc0,0x1f
};

static inline bool phase5_sync(uint8_t a, uint8_t b)
{
    unsigned idx = ((unsigned)a << 5u) | b;
    return (s_phase5_sync_mask[idx >> 3u] & (1u << (idx & 7u))) != 0;
}

static inline uint8_t preview_luma(uint8_t prev, uint8_t cur)
{
    /* Exact low-six-bit output of fm.bsasm's second LUT lookup. */
    return s_fm_delta_lut[((unsigned)prev << 5u) | cur];
}

static inline uint16_t gray565(uint8_t g)
{
    /* fm.bsasm produces calibrated 6-bit CVBS/DAC codes. Expand 0..63. */
    g = (uint8_t)((g * 255u) / 63u);
    return (uint16_t)(((g >> 3) << 11) | ((g >> 2) << 5) | (g >> 3));
}

static void preview_task(void *arg)
{
    (void)arg;
    uint16_t *line = heap_caps_malloc(C5VRX_LCD_WIDTH * sizeof(*line),
                                      MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!line) {
        ESP_LOGE(TAG, "preview line allocation failed");
        vTaskDelete(NULL);
        return;
    }

    unsigned y = 0, report_lines = 0, report_pal = 0, report_ntsc = 0;
    TickType_t report_at = xTaskGetTickCount();

    for (;;) {
        /* RX DMA writes behind the CPU cache. Invalidate the ring before
         * observing it; without this the task can keep seeing the zero-filled
         * cache lines that existed before PARLIO started. */
        /* s_raw_ring is internal SRAM and is directly CPU coherent on this
         * target. esp_cache_msync() rejects this address class, so do not call
         * it here. The changing raw-byte diagnostic below verifies that GDMA
         * writes are visible to the CPU. */
        __asm__ __volatile__("fence rw, rw" ::: "memory");

        /*
         * First-light observer only: RX continuously overwrites this cyclic
         * ring. A torn observation may drop a line, but cannot stall DMA.
         * The production decoder will consume completed descriptors instead.
         */
        /* fm.bsasm consumes one of the two 40-MS/s byte parities to produce
         * the 20-MS/s unique CVBS stream. Try both alignments explicitly. */
        unsigned parity = 0;
        unsigned parity_hits[2] = {0, 0};
        for (unsigned test_p = 0; test_p < 2; ++test_p) {
            uint8_t tp = s_phase5_state_lut[s_raw_ring[test_p]];
            bool tin = false;
            unsigned trun = 0;
            for (unsigned i = test_p + 2; i < RAW_RING_BYTES; i += 2) {
                uint8_t tc = s_phase5_state_lut[s_raw_ring[i]];
                bool low = preview_luma(tp, tc) <= 8u;
                tp = tc;
                if (low) { tin = true; ++trun; }
                else if (tin) {
                    if (trun >= 70u && trun <= 125u) ++parity_hits[test_p];
                    tin = false; trun = 0;
                }
            }
        }
        parity = parity_hits[1] > parity_hits[0] ? 1u : 0u;

        uint8_t prev = s_phase5_state_lut[s_raw_ring[parity]];
        bool in_sync = false;
        unsigned run_start = 0, run_len = 0, last_start = 0;

        for (unsigned n = 1, i = parity + 2; i < RAW_RING_BYTES; i += 2, ++n) {
            uint8_t cur = s_phase5_state_lut[s_raw_ring[i]];
            bool low = preview_luma(prev, cur) <= 8u;
            prev = cur;

            if (low) {
                if (!in_sync) { in_sync = true; run_start = n; run_len = 1; }
                else ++run_len;
                continue;
            }
            if (!in_sync) continue;
            in_sync = false;

            if (run_len < 70 || run_len > 125) continue;

            if (last_start) {
                unsigned period = run_start - last_start;
                if (period >= 1266 && period <= 1275) ++report_ntsc;
                else if (period >= 1276 && period <= 1284) ++report_pal;
            }
            last_start = run_start;
            ++report_lines;

            /* ~9.5 us after H-sync start is a conservative first-light
             * active-video position. Sample 1000 20-MS/s points into 320 px. */
            unsigned active = run_start + 190u;
            if (active + 1000u >= RAW_RING_BYTES / 2u) continue;
            uint8_t p = s_phase5_state_lut[s_raw_ring[active * 2u + parity]];
            for (unsigned x = 0; x < C5VRX_LCD_WIDTH; ++x) {
                unsigned sn = active + (x * 1000u) / C5VRX_LCD_WIDTH;
                uint8_t q = s_phase5_state_lut[s_raw_ring[sn * 2u + parity]];
                line[x] = gray565(preview_luma(p, q));
                p = q;
            }
            if (c5vrx_waveshare_lcd_draw_line(y, line) == ESP_OK) {
                y = (y + 1u) % C5VRX_LCD_HEIGHT;
                /* Keep the single persistent line buffer out of the SPI DMA
                 * queue before rewriting it. This is diagnostic, not final. */
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }

        TickType_t now = xTaskGetTickCount();
        if (now - report_at >= pdMS_TO_TICKS(1000)) {
            unsigned nonzero = 0, changes = 0;
            uint8_t minv = 255, maxv = 0, last = s_raw_ring[0];
            for (unsigned i = 0; i < 4096; ++i) {
                uint8_t v = s_raw_ring[i];
                nonzero += v != 0;
                changes += (i != 0 && v != last);
                if (v < minv) minv = v;
                if (v > maxv) maxv = v;
                last = v;
            }
            unsigned nibble_mixed = 0, hi_only = 0, lo_only = 0, both = 0;
            unsigned hist[16] = {0};
            for (unsigned i = 0; i < 4096; ++i) {
                uint8_t v = s_raw_ring[i];
                bool hi = (v & 0xf0u) != 0;
                bool lo = (v & 0x0fu) != 0;
                if (hi && lo) ++both;
                else if (hi) ++hi_only;
                else if (lo) ++lo_only;
                if (hi && lo) ++nibble_mixed;
                ++hist[v & 0x0fu];
            }
            ESP_LOGI(TAG, "IQ window: min=%u max=%u nonzero=%u/4096 changes=%u nibble both=%u hi=%u lo=%u",
                     minv, maxv, nonzero, changes, both, hi_only, lo_only);
            ESP_LOGI(TAG, "IQ low-nibble hist: %u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                     hist[0],hist[1],hist[2],hist[3],hist[4],hist[5],hist[6],hist[7],
                     hist[8],hist[9],hist[10],hist[11],hist[12],hist[13],hist[14],hist[15]);
            unsigned bit_ones[8] = {0};
            for (unsigned i = 0; i < 4096; ++i) {
                uint8_t v = s_raw_ring[i];
                for (unsigned b = 0; b < 8; ++b) bit_ones[b] += (v >> b) & 1u;
            }
            ESP_LOGI(TAG, "IQ bit ones/4096: b0=%u b1=%u b2=%u b3=%u b4=%u b5=%u b6=%u b7=%u",
                     bit_ones[0], bit_ones[1], bit_ones[2], bit_ones[3],
                     bit_ones[4], bit_ones[5], bit_ones[6], bit_ones[7]);
            unsigned eq01=0, eq12=0, eq23=0, eq45=0, eq56=0, eq67=0;
            unsigned uniq[256] = {0}, uniq_count = 0;
            for (unsigned i = 0; i < 4096; ++i) {
                uint8_t v = s_raw_ring[i];
                eq01 += (((v >> 0) ^ (v >> 1)) & 1u) == 0;
                eq12 += (((v >> 1) ^ (v >> 2)) & 1u) == 0;
                eq23 += (((v >> 2) ^ (v >> 3)) & 1u) == 0;
                eq45 += (((v >> 4) ^ (v >> 5)) & 1u) == 0;
                eq56 += (((v >> 5) ^ (v >> 6)) & 1u) == 0;
                eq67 += (((v >> 6) ^ (v >> 7)) & 1u) == 0;
                if (!uniq[v]) { uniq[v] = 1; ++uniq_count; }
            }
            ESP_LOGI(TAG, "IQ lane equality/4096: 01=%u 12=%u 23=%u 45=%u 56=%u 67=%u unique_bytes=%u",
                     eq01, eq12, eq23, eq45, eq56, eq67, uniq_count);
            /* Independent observation path: read the physical GPIO pads
             * directly. If these eight bits vary independently while PARLIO
             * reports duplicated nibbles, the fault is PARLIO input routing.
             * If the pad reads are duplicated too, MODEM_DIAG output routing
             * (or the selected DIAG signals) is already wrong before PARLIO. */
            static const gpio_num_t iq_gpio[8] = C5VRX_IQ_GPIOS;
            unsigned pad_ones[8] = {0};
            unsigned pad_eq01=0, pad_eq12=0, pad_eq23=0;
            unsigned pad_eq45=0, pad_eq56=0, pad_eq67=0;
            unsigned pad_unique[256] = {0}, pad_unique_count = 0;
            for (unsigned n = 0; n < 4096; ++n) {
                uint8_t v = 0;
                for (unsigned b = 0; b < 8; ++b) {
                    unsigned level = (unsigned)gpio_get_level(iq_gpio[b]) & 1u;
                    v |= (uint8_t)(level << b);
                    pad_ones[b] += level;
                }
                pad_eq01 += (((v >> 0) ^ (v >> 1)) & 1u) == 0;
                pad_eq12 += (((v >> 1) ^ (v >> 2)) & 1u) == 0;
                pad_eq23 += (((v >> 2) ^ (v >> 3)) & 1u) == 0;
                pad_eq45 += (((v >> 4) ^ (v >> 5)) & 1u) == 0;
                pad_eq56 += (((v >> 5) ^ (v >> 6)) & 1u) == 0;
                pad_eq67 += (((v >> 6) ^ (v >> 7)) & 1u) == 0;
                if (!pad_unique[v]) { pad_unique[v] = 1; ++pad_unique_count; }
            }
            ESP_LOGI(TAG, "PAD bit ones/4096: b0=%u b1=%u b2=%u b3=%u b4=%u b5=%u b6=%u b7=%u",
                     pad_ones[0], pad_ones[1], pad_ones[2], pad_ones[3],
                     pad_ones[4], pad_ones[5], pad_ones[6], pad_ones[7]);
            ESP_LOGI(TAG, "PAD lane equality/4096: 01=%u 12=%u 23=%u 45=%u 56=%u 67=%u unique_bytes=%u",
                     pad_eq01, pad_eq12, pad_eq23, pad_eq45, pad_eq56, pad_eq67,
                     pad_unique_count);
            const char *std = report_pal > report_ntsc ? "PAL" :
                              report_ntsc > report_pal ? "NTSC" : "?";
            ESP_LOGI(TAG, "preview sync: hsync=%u/s ntsc_votes=%u pal_votes=%u standard=%s row=%u parity=%u hits=%u/%u",
                     report_lines, report_ntsc, report_pal, std, y, parity,
                     parity_hits[0], parity_hits[1]);
            report_lines = report_pal = report_ntsc = 0;
            report_at = now;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

esp_err_t c5vrx_waveshare_rf_lcd_preview_start(void)
{
    BaseType_t ok = xTaskCreate(preview_task, "rf_lcd_preview", 4096, NULL, 4, NULL);
    if (ok != pdPASS) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG, "first-light grayscale preview started (software Phase5 observer)");
    return ESP_OK;
}
