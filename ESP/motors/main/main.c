#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_slave.h"
#include "esp_log.h"

// pin definitions
#define BLINK_GPIO 2

// ESP32 SPI Slave configuration
#define GPIO_MOSI 23
#define GPIO_MISO 19
#define GPIO_SCLK 18
#define GPIO_CS   5

static const char *TAG = "spi_demo";

void blink_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Configuring LED blink pin!");
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    int led_state = 0;
    while (1) {
        led_state = !led_state;
        gpio_set_level(BLINK_GPIO, led_state);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void spi_slave_task(void *pvParameters)
{
    // Configuration for the SPI bus
    spi_bus_config_t buscfg={
        .mosi_io_num=GPIO_MOSI,
        .miso_io_num=GPIO_MISO,
        .sclk_io_num=GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    // Configuration for the SPI slave interface
    spi_slave_interface_config_t slvcfg={
        .mode=0,
        .spics_io_num=GPIO_CS,
        .queue_size=3,
        .flags=0,
    };

    // Enable pull-ups on SPI lines so we don't have rogue pulses when no master is connected.
    gpio_set_pull_mode(GPIO_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_SCLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_CS, GPIO_PULLUP_ONLY);

    // Initialize SPI slave interface
    esp_err_t ret = spi_slave_initialize(SPI2_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI slave");
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "SPI Slave initialized. Ext: MOSI=%d MISO=%d SCLK=%d CS=%d", GPIO_MOSI, GPIO_MISO, GPIO_SCLK, GPIO_CS);

    // Buffers for SPI transactions
    WORD_ALIGNED_ATTR char sendbuf[32] = "";
    WORD_ALIGNED_ATTR char recvbuf[32] = "";

    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));

    int trans_count = 0;

    while (1) {
        // Prepare data to send to master
        snprintf(sendbuf, sizeof(sendbuf), "ESP Msg: %d", trans_count++);

        // Set up the transaction
        t.length = 32 * 8; // 32 bytes in bits
        t.tx_buffer = sendbuf;
        t.rx_buffer = recvbuf;

        // This call will block until a transaction is completed by the master
        ret = spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Received from Master: %s", recvbuf);
            ESP_LOGI(TAG, "Sent to Master: %s", sendbuf);
        } else {
            ESP_LOGE(TAG, "SPI Transmit Error");
        }

        memset(recvbuf, 0, 32);
    }
}

void app_main(void)
{
    // Run blink task and spi slave task concurrently
    xTaskCreate(blink_task, "blink_task", 2048, NULL, 5, NULL);
    xTaskCreate(spi_slave_task, "spi_slave_task", 4096, NULL, 5, NULL);
}
