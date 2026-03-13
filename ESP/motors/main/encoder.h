/**
 * through_bore_encoder.h
 * 
 * Driver for the REV Through Bore Encoder (REV-11-1271) on ESP32.
 * Uses the Broadcom AEAT-8800-Q24 magnetic encoder IC.
 *
 * Encoder specs:
 *   - Quadrature:  2048 CPR → 8192 counts/rev (4× hardware decoding via PCNT)
 *   - Index pulse:  once per revolution, 90°e wide (default)
 *   - Absolute:     duty-cycle output, period ≈ 1025 µs (≈ 975.6 Hz)
 *                   pulse 1 µs → 0°, 1024 µs → 360°, 10-bit resolution
 *   - Logic level:  3.3 V (ESP32 safe, no level shifter needed)
 */

#ifndef ENCODER_H
#define ENCODER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Encoder specs ───────────────────────────────────────────────── */

#define TBE_CPR              2048       /* Cycles per revolution          */
#define TBE_COUNTS_PER_REV   8192       /* 4× decoded counts per rev      */
#define TBE_ABS_PERIOD_US    1025       /* Absolute output period (µs)    */
#define TBE_ABS_MIN_PULSE_US 1          /* Pulse at 0° (µs)              */
#define TBE_ABS_MAX_PULSE_US 1024       /* Pulse at 360° (µs)            */

/* Set this to your odometry wheel's circumference in mm */
#define WHEEL_CIRCUMFERENCE_MM  100.53f   /* ~65mm diameter wheel → π×65   */

/* ── Encoder instance ────────────────────────────────────────────── */

typedef struct {
    /* Pin assignments */
    gpio_num_t pin_a;          /* Quadrature channel A              */
    gpio_num_t pin_b;          /* Quadrature channel B              */
    gpio_num_t pin_index;      /* Index pulse (once/rev)            */
    gpio_num_t pin_abs;        /* Absolute duty-cycle output        */

    /* PCNT unit assigned during init */
    int pcnt_unit;

    /* Quadrature state (accumulated across PCNT overflows) */
    volatile int64_t total_count;

    /* Index pulse counter */
    volatile uint32_t index_count;

    /* Absolute duty-cycle measurement (updated by ISR) */
    volatile uint32_t abs_rise_us;     /* Timestamp of last rising edge  */
    volatile uint32_t abs_high_us;     /* Most recent high-pulse width   */
    volatile float    abs_angle_deg;   /* Decoded absolute angle 0–360   */

    /* Diagnostics */
    volatile uint32_t abs_edge_count;

    bool initialized;
} through_bore_encoder_t;

/* ── API ─────────────────────────────────────────────────────────── */

/**
 * Initialize the encoder: configures PCNT for quadrature decoding,
 * and GPIO ISRs for the index and absolute duty-cycle signals.
 *
 * @param enc  Encoder struct with pin_a/b/index/abs already set.
 * @return     ESP_OK on success.
 */
esp_err_t tbe_init(through_bore_encoder_t *enc);

/**
 * Get the cumulative quadrature count (signed, unbounded).
 * Positive = forward rotation, negative = reverse.
 */
int64_t tbe_get_count(through_bore_encoder_t *enc);

/**
 * Get the current angle from quadrature (0.0–360.0°), wrapping each rev.
 */
float tbe_get_quad_angle(through_bore_encoder_t *enc);

/**
 * Get total revolutions (signed, fractional).
 */
float tbe_get_revolutions(through_bore_encoder_t *enc);

/**
 * Get the absolute angle from the duty-cycle output (0.0–360.0°).
 */
float tbe_get_abs_angle(through_bore_encoder_t *enc);

/**
 * Get the number of index pulses seen since init.
 */
uint32_t tbe_get_index_count(through_bore_encoder_t *enc);

/**
 * Reset the quadrature count and index counter to zero.
 */
void tbe_reset(through_bore_encoder_t *enc);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_H */