/**
 * through_bore_encoder.c
 *
 * ESP-IDF driver for the REV Through Bore Encoder (REV-11-1271).
 *
 * Quadrature decoding uses the ESP32's hardware PCNT peripheral,
 * which handles 4× decoding in hardware with zero CPU overhead.
 * A PCNT watch-point fires an ISR on overflow/underflow to extend
 * the 16-bit counter to a full int64.
 *
 * The absolute duty-cycle signal (~976 Hz) is timed via GPIO ISR
 * using esp_timer_get_time() (µs resolution).
 */

#include "encoder.h"

#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

#define TAG "TBE"

/* ── PCNT overflow handling ──────────────────────────────────────── */

/* PCNT hardware counter is 16-bit signed (–32768 to +32767).
 * We set watch points near the limits and accumulate into total_count. */
#define PCNT_HIGH_LIMIT  30000
#define PCNT_LOW_LIMIT  -30000

static bool IRAM_ATTR pcnt_overflow_cb(pcnt_unit_handle_t unit,
                                        const pcnt_watch_event_data_t *edata,
                                        void *user_ctx)
{
    through_bore_encoder_t *enc = (through_bore_encoder_t *)user_ctx;
    enc->total_count += edata->watch_point_value;
    return false; /* no need to yield */
}

/* ── GPIO ISR for index pulse ────────────────────────────────────── */

static void IRAM_ATTR index_isr_handler(void *arg)
{
    through_bore_encoder_t *enc = (through_bore_encoder_t *)arg;
    enc->index_count++;
}

/* ── GPIO ISR for absolute duty-cycle ────────────────────────────── */

static void IRAM_ATTR abs_isr_handler(void *arg)
{
    through_bore_encoder_t *enc = (through_bore_encoder_t *)arg;
    uint32_t now = (uint32_t)esp_timer_get_time(); /* µs timestamp */

    if (gpio_get_level(enc->pin_abs)) {
        /* Rising edge — record timestamp */
        enc->abs_rise_us = now;
        enc->abs_edge_count++;
    } else {
        /* Falling edge — store raw pulse width (integer only, no floats in ISR!) */
        uint32_t rise = enc->abs_rise_us;
        if (rise != 0) {
            enc->abs_high_us = now - rise;
            enc->abs_edge_count++;
        }
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

esp_err_t tbe_init(through_bore_encoder_t *enc)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing Through Bore Encoder  A=%d B=%d Idx=%d Abs=%d",
             enc->pin_a, enc->pin_b, enc->pin_index, enc->pin_abs);

    /* Zero state */
    enc->total_count   = 0;
    enc->index_count   = 0;
    enc->abs_rise_us   = 0;
    enc->abs_high_us   = 0;
    enc->abs_angle_deg = 0.0f;
    enc->abs_edge_count = 0;

    /* ── 1. Configure PCNT for quadrature decoding ───────────────── */

    /* Pre-configure A/B pins: input-only GPIOs (34-39) have no internal
     * pull-up/pull-down resistors. Set them explicitly before PCNT init
     * to suppress the GPIO driver warnings. */
    gpio_config_t quad_conf = {
        .pin_bit_mask = (1ULL << enc->pin_a) | (1ULL << enc->pin_b),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&quad_conf);

    pcnt_unit_config_t unit_config = {
        .high_limit = PCNT_HIGH_LIMIT,
        .low_limit  = PCNT_LOW_LIMIT,
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    ret = pcnt_new_unit(&unit_config, &pcnt_unit);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PCNT unit: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Glitch filter: reject pulses < 1 µs (noise rejection) */
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config);

    /* Channel A: count on A edges, use B as direction */
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num  = enc->pin_a,
        .level_gpio_num = enc->pin_b,
    };
    pcnt_channel_handle_t chan_a = NULL;
    pcnt_new_channel(pcnt_unit, &chan_a_config, &chan_a);
    pcnt_channel_set_edge_action(chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE,  /* pos edge of A */
        PCNT_CHANNEL_EDGE_ACTION_INCREASE); /* neg edge of A */
    pcnt_channel_set_level_action(chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,     /* B high → keep direction */
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE); /* B low  → invert */

    /* Channel B: count on B edges, use A as direction */
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num  = enc->pin_b,
        .level_gpio_num = enc->pin_a,
    };
    pcnt_channel_handle_t chan_b = NULL;
    pcnt_new_channel(pcnt_unit, &chan_b_config, &chan_b);
    pcnt_channel_set_edge_action(chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    /* Watch points for overflow accumulation */
    pcnt_unit_add_watch_point(pcnt_unit, PCNT_HIGH_LIMIT);
    pcnt_unit_add_watch_point(pcnt_unit, PCNT_LOW_LIMIT);

    pcnt_event_callbacks_t pcnt_cbs = {
        .on_reach = pcnt_overflow_cb,
    };
    pcnt_unit_register_event_callbacks(pcnt_unit, &pcnt_cbs, enc);

    pcnt_unit_enable(pcnt_unit);
    pcnt_unit_clear_count(pcnt_unit);
    pcnt_unit_start(pcnt_unit);

    enc->pcnt_unit = (int)(intptr_t)pcnt_unit; /* stash handle */

    /* ── 2. GPIO ISR for index pulse ─────────────────────────────── */

    gpio_config_t idx_conf = {
        .pin_bit_mask = (1ULL << enc->pin_index),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_POSEDGE,
    };
    gpio_config(&idx_conf);
    gpio_install_isr_service(0); /* safe to call multiple times */
    gpio_isr_handler_add(enc->pin_index, index_isr_handler, enc);

    /* ── 3. GPIO ISR for absolute duty-cycle ─────────────────────── */

    gpio_config_t abs_conf = {
        .pin_bit_mask = (1ULL << enc->pin_abs),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&abs_conf);
    gpio_isr_handler_add(enc->pin_abs, abs_isr_handler, enc);

    enc->initialized = true;
    ESP_LOGI(TAG, "Through Bore Encoder initialized OK");
    return ESP_OK;
}

int64_t tbe_get_count(through_bore_encoder_t *enc)
{
    int hw_count = 0;
    pcnt_unit_get_count((pcnt_unit_handle_t)(intptr_t)enc->pcnt_unit, &hw_count);
    return enc->total_count + (int64_t)hw_count;
}

float tbe_get_quad_angle(through_bore_encoder_t *enc)
{
    int64_t count = tbe_get_count(enc);
    int64_t mod = count % TBE_COUNTS_PER_REV;
    if (mod < 0) mod += TBE_COUNTS_PER_REV;
    return (float)mod / (float)TBE_COUNTS_PER_REV * 360.0f;
}

float tbe_get_revolutions(through_bore_encoder_t *enc)
{
    return (float)tbe_get_count(enc) / (float)TBE_COUNTS_PER_REV;
}

float tbe_get_abs_angle(through_bore_encoder_t *enc)
{
    /* Float math happens here in task context, NOT in the ISR.
     * ISR only stores the raw integer pulse width (abs_high_us). */
    uint32_t high_us = enc->abs_high_us;
    float frac = (float)((int32_t)high_us - TBE_ABS_MIN_PULSE_US)
               / (float)(TBE_ABS_MAX_PULSE_US - TBE_ABS_MIN_PULSE_US);
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    return frac * 360.0f;
}

uint32_t tbe_get_index_count(through_bore_encoder_t *enc)
{
    return enc->index_count;
}

void tbe_reset(through_bore_encoder_t *enc)
{
    pcnt_unit_clear_count((pcnt_unit_handle_t)(intptr_t)enc->pcnt_unit);
    enc->total_count  = 0;
    enc->index_count  = 0;
}