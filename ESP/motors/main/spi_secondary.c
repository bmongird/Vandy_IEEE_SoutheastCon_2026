#include "spi_secondary.h"

#include <stdio.h>
#include <string.h>
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define TAG "SPI_SECONDARY"

/* ── SPI Pin Configuration (must match Pi wiring) ──────────────────── */
#define PIN_MISO  19
#define PIN_MOSI  23
#define PIN_SCLK  18
#define PIN_CS     5

/* ── Shared state (protected by mutex) ─────────────────────────────── */
static SemaphoreHandle_t spi_mutex;
static uint8_t current_command  = STATE_IDLE; /* last state the Pi told us */
static uint8_t state_completed  = 1;         /* 1 = idle / done, 0 = busy */

/* ── Buffers (DMA-capable, word-aligned) ───────────────────────────── */
WORD_ALIGNED_ATTR static uint8_t rx_buf[SPI_BUF_SIZE];
WORD_ALIGNED_ATTR static uint8_t tx_buf[SPI_BUF_SIZE];

/* ── Forward declarations ──────────────────────────────────────────── */
static void spi_task(void *arg);
static void handle_received(const uint8_t *rx);
static void prepare_response(uint8_t cmd_id, uint8_t state, uint8_t status);

/* ================================================================== */

esp_err_t spi_secondary_init(void)
{
    spi_mutex = xSemaphoreCreateMutex();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num   = PIN_MOSI,
        .miso_io_num   = PIN_MISO,
        .sclk_io_num   = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SPI_BUF_SIZE,
    };

    spi_slave_interface_config_t slave_cfg = {
        .spics_io_num   = PIN_CS,
        .flags          = 0,
        .queue_size     = 1,
        .mode           = 0,            /* SPI mode 0 – matches Pi */
        .post_setup_cb  = NULL,
        .post_trans_cb  = NULL,
    };

    esp_err_t ret = spi_slave_initialize(SPI2_HOST, &bus_cfg, &slave_cfg,
                                         SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI slave init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPI slave initialised (mode 0, %d-byte frames)", SPI_BUF_SIZE);

    xTaskCreate(spi_task, "spi_task", 4096, NULL, 5, NULL);
    return ESP_OK;
}

/* ── Public getters / setters ─────────────────────────────────────── */

uint8_t get_current_command(void)
{
    uint8_t cmd = STATE_IDLE;
    if (xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(50))) {
        cmd = current_command;
        xSemaphoreGive(spi_mutex);
    }
    return cmd;
}

void report_state_complete(void)
{
    if (xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(50))) {
        state_completed = 1;
        xSemaphoreGive(spi_mutex);
    }
    ESP_LOGI(TAG, "State marked as completed");
}

/* ── SPI receive / respond task ───────────────────────────────────── */

static void spi_task(void *arg)
{
    spi_slave_transaction_t txn;
    memset(&txn, 0, sizeof(txn));

    txn.length    = SPI_BUF_SIZE * 8;   /* length in bits */
    txn.tx_buffer = tx_buf;
    txn.rx_buffer = rx_buf;

    /* Start with an empty (all-zeros) response buffer */
    memset(tx_buf, 0, SPI_BUF_SIZE);

    while (1) {
        memset(rx_buf, 0, SPI_BUF_SIZE);

        esp_err_t ret = spi_slave_transmit(SPI2_HOST, &txn,
                                           portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "SPI transmit error: %s", esp_err_to_name(ret));
            continue;
        }

        /* Process whatever the Pi sent us */
        handle_received(rx_buf);
    }
}

/* ── Protocol handling ────────────────────────────────────────────── */

static void handle_received(const uint8_t *rx)
{
    uint8_t cmd_id    = rx[0];
    uint8_t state_code = rx[1];

    switch (cmd_id) {

    case CMD_STATE:
        /* Pi is commanding us to enter a new state */
        if (xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(50))) {
            current_command = state_code;
            state_completed = 0;          /* mark as in-progress */
            xSemaphoreGive(spi_mutex);
        }
        ESP_LOGI(TAG, "Received state command: 0x%02X", state_code);

        /* Queue an ACK for the Pi to read on its next transfer */
        prepare_response(RSP_STATE_ACK, state_code, STATUS_IN_PROGRESS);
        break;

    case CMD_QUERY: {
        /* Pi is asking whether we've finished */
        uint8_t done = 0;
        if (xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(50))) {
            done = state_completed;
            xSemaphoreGive(spi_mutex);
        }
        prepare_response(RSP_STATUS, state_code,
                         done ? STATUS_COMPLETED : STATUS_IN_PROGRESS);
        break;
    }

    default:
        /* Unknown or all-zeros (dummy read) — keep whatever is in tx_buf */
        break;
    }
}

static void prepare_response(uint8_t cmd_id, uint8_t state, uint8_t status)
{
    memset(tx_buf, 0, SPI_BUF_SIZE);
    tx_buf[0] = cmd_id;
    tx_buf[1] = state;
    tx_buf[2] = status;
}
