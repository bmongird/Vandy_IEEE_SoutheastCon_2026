/**
 * encoder_test.c
 *
 * Test program for the REV Through Bore Encoder on ESP32.
 * Runs a series of diagnostic checks, then live-prints encoder data.
 *
 * Wiring (JST-PH 6-pin → 4×3-pin breakout cable):
 * ┌───────────────────────────────────────────────────┐
 * │  Wire Color    Signal         ESP32 GPIO          │
 * │  ───────────   ────────────   ──────────          │
 * │  Red           VCC (3.3V)     3V3                 │
 * │  Black         GND            GND                 │
 * │  Yellow        A  (quad)      GPIO 34 *           │
 * │  Green         B  (quad)      GPIO 35 *           │
 * │  Blue          Index          GPIO 36 *           │
 * │  White         Abs / PWM      GPIO 39 *           │
 * └───────────────────────────────────────────────────┘
 * * These are input-only pins on ESP32 — perfect for an encoder
 *   since they won't conflict with your motor PWM outputs.
 *   Change below if needed. Avoid GPIOs already in use by motors.
 *
 * Usage:
 *   Call encoder_test_main() from your app_main, or integrate
 *   into your state machine as a calibration/test state.
 */

#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "encoder.h"

#define TAG "ENC_TEST"

/* ── Pin assignments ─────────────────────────────────────────────── 
 * Using input-only GPIOs to avoid conflicts with your motor pins
 * (17, 21, 25, 32, 33, 4, 27). Adjust if these are taken.        */
#define ENC_PIN_A      GPIO_NUM_34
#define ENC_PIN_B      GPIO_NUM_35
#define ENC_PIN_INDEX  GPIO_NUM_36
#define ENC_PIN_ABS    GPIO_NUM_39

/* ── Diagnostics ─────────────────────────────────────────────────── */

static bool run_diagnostics(through_bore_encoder_t *enc)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  REV Through Bore Encoder — Diagnostics     ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════╝");

    /* Check absolute signal is toggling (should be ~976 Hz even when still) */
    uint32_t start_abs_edges = enc->abs_edge_count;
    int64_t  start_count     = tbe_get_count(enc);

    ESP_LOGI(TAG, "Watching for signal activity (3s — rotate the shaft)...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    uint32_t abs_edges  = enc->abs_edge_count - start_abs_edges;
    int64_t  quad_moved = llabs(tbe_get_count(enc) - start_count);
    uint32_t idx_pulses = enc->index_count;

    ESP_LOGI(TAG, "  Quadrature counts:  %lld", (long long)quad_moved);
    ESP_LOGI(TAG, "  Index pulses:       %lu", (unsigned long)idx_pulses);
    ESP_LOGI(TAG, "  Abs signal edges:   %lu", (unsigned long)abs_edges);

    bool ok = true;

    if (quad_moved == 0) {
        ESP_LOGW(TAG, "  ⚠ No quadrature movement!");
        ESP_LOGW(TAG, "    → Check wiring to GPIO %d (A) and GPIO %d (B)",
                 enc->pin_a, enc->pin_b);
        ESP_LOGW(TAG, "    → Confirm encoder is powered (3.3V + GND)");
        ok = false;
    }

    if (abs_edges == 0) {
        ESP_LOGW(TAG, "  ⚠ No absolute duty-cycle edges!");
        ESP_LOGW(TAG, "    → Check wiring to GPIO %d", enc->pin_abs);
        ESP_LOGW(TAG, "    → Abs signal should toggle even at standstill (~976 Hz)");
        ok = false;
    } else {
        float approx_freq = (float)abs_edges / 2.0f / 3.0f;
        ESP_LOGI(TAG, "  Abs frequency: ~%.0f Hz  (expected ~976 Hz)", approx_freq);
        if (approx_freq < 500 || approx_freq > 1500) {
            ESP_LOGW(TAG, "  ⚠ Frequency outside expected range");
            ok = false;
        }
    }

    if (ok) {
        ESP_LOGI(TAG, "  ✓ All signals look good!");
    } else {
        ESP_LOGE(TAG, "  ✗ Some signals missing — check wiring");
    }
    return ok;
}

/* ── Live monitoring ─────────────────────────────────────────────── */

static void run_live_test(through_bore_encoder_t *enc, int duration_sec)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  Live Encoder Test  (%ds)                   ║", duration_sec);
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "Rotate the encoder shaft and watch values update.");

    int64_t prev_count = tbe_get_count(enc);
    int64_t prev_time  = esp_timer_get_time();

    for (int i = 0; i < duration_sec * 20; i++) {  /* 50 ms intervals */
        vTaskDelay(pdMS_TO_TICKS(50));

        int64_t count = tbe_get_count(enc);
        int64_t now   = esp_timer_get_time();

        /* Compute RPM from delta */
        float dt_sec = (float)(now - prev_time) / 1e6f;
        float rpm = 0.0f;
        if (dt_sec > 0.001f) {
            float delta_rev = (float)(count - prev_count) / (float)TBE_COUNTS_PER_REV;
            rpm = (delta_rev / dt_sec) * 60.0f;
        }
        prev_count = count;
        prev_time  = now;

        float quad_deg = tbe_get_quad_angle(enc);
        float abs_deg  = tbe_get_abs_angle(enc);
        float revs     = tbe_get_revolutions(enc);

        ESP_LOGI(TAG, "Quad: %7lld counts | %6.1f° | %+.3f rev | %+.1f RPM | "
                      "Abs: %5.1f° (%lu µs) | Idx: %lu",
                 (long long)count,
                 quad_deg,
                 revs,
                 rpm,
                 abs_deg,
                 (unsigned long)enc->abs_high_us,
                 (unsigned long)tbe_get_index_count(enc));
    }
}

/* ── Quad vs Absolute agreement test ─────────────────────────────── */

static void run_agreement_test(through_bore_encoder_t *enc)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  Quad vs Absolute Agreement Test (10s)      ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "Slowly rotate the shaft one full turn.");

    float max_err = 0.0f;
    float sum_err = 0.0f;
    int   samples = 0;

    for (int i = 0; i < 200; i++) {  /* 10s at 50ms */
        vTaskDelay(pdMS_TO_TICKS(50));

        float q = tbe_get_quad_angle(enc);
        float a = tbe_get_abs_angle(enc);

        /* Angular difference, normalized to -180..+180 */
        float diff = q - a;
        diff = fmodf(diff + 180.0f, 360.0f) - 180.0f;
        float abs_diff = fabsf(diff);

        sum_err += abs_diff;
        if (abs_diff > max_err) max_err = abs_diff;
        samples++;
    }

    float avg_err = sum_err / (float)samples;

    ESP_LOGI(TAG, "  Samples: %d", samples);
    ESP_LOGI(TAG, "  Avg |quad - abs| error:  %.2f°", avg_err);
    ESP_LOGI(TAG, "  Max |quad - abs| error:  %.2f°", max_err);

    if (max_err < 5.0f) {
        ESP_LOGI(TAG, "  ✓ Good agreement — both outputs are consistent.");
    } else if (max_err < 15.0f) {
        ESP_LOGW(TAG, "  ~ Moderate disagreement — check magnet alignment.");
    } else {
        ESP_LOGE(TAG, "  ✗ Large disagreement — wiring issue or magnet problem.");
    }
}

/* ── Public entry point ──────────────────────────────────────────── */

void encoder_test_main(void)
{
    ESP_LOGI(TAG, "Starting Through Bore Encoder test...");

    through_bore_encoder_t enc = {
        .pin_a     = ENC_PIN_A,
        .pin_b     = ENC_PIN_B,
        .pin_index = ENC_PIN_INDEX,
        .pin_abs   = ENC_PIN_ABS,
    };

    esp_err_t ret = tbe_init(&enc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Encoder init failed: %s", esp_err_to_name(ret));
        return;
    }

    /* Step 1: Diagnostics */
    run_diagnostics(&enc);

    /* Step 2: Live monitoring (15 seconds) */
    run_live_test(&enc, 15);

    /* Step 3: Agreement test */
    tbe_reset(&enc);  /* zero out quad count before comparison */
    run_agreement_test(&enc);

    ESP_LOGI(TAG, "Encoder test complete.");
}

/* ─────────────────────────────────────────────────────────────────
 * Integration with your existing robot code:
 *
 * In your app_main() or setup(), add the encoder to robot_singleton:
 *
 *   through_bore_encoder_t left_enc = {
 *       .pin_a = GPIO_NUM_34, .pin_b = GPIO_NUM_35,
 *       .pin_index = GPIO_NUM_36, .pin_abs = GPIO_NUM_39,
 *   };
 *   tbe_init(&left_enc);
 *
 * Then in your main loop or a FreeRTOS task, read odometry:
 *
 *   int64_t counts = tbe_get_count(&left_enc);
 *   float distance_mm = (float)counts / TBE_COUNTS_PER_REV
 *                     * wheel_circumference_mm;
 *
 * For a second encoder on the other wheel, just create another
 * through_bore_encoder_t with different pins — the ESP32 has
 * multiple PCNT units available.
 * ────────────────────────────────────────────────────────────────── */