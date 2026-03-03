#include "spi_secondary.h"

#define TAG "SPI_SECONDARY"
#define END_SIGNAL "<END>"  // Special signal from master indicating the end of transmission

// SPI Pin Configuration
#define PIN_MISO  19
#define PIN_MOSI  23
#define PIN_SCLK  18
#define PIN_CS    5

static char command_to_send[CHUNK_SIZE] = {0};
static SPI_received_data_t receivedData = { .messageInput = "default" };
EMAState purple_object_ema;
EMAState april_tag_ema;
EMAState line_following_ema;
SemaphoreHandle_t data_mutex;

esp_err_t spi_secondary_init(void) {
    data_mutex = xSemaphoreCreateMutex();

    // SPI Bus Configuration
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,  // Not used
        .quadhd_io_num = -1,  // Not used
        .max_transfer_sz = CHUNK_SIZE
    };

    // SPI Slave Interface Configuration
    spi_slave_interface_config_t slvcfg = {
        .spics_io_num = PIN_CS,
        .flags = 0,
        .queue_size = 1,
        .mode = 0,  // SPI mode 0 (should match master)
        .post_setup_cb = NULL,
        .post_trans_cb = NULL
    };

    // Initialize SPI bus
    esp_err_t ret = spi_slave_initialize(SPI2_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI Slave initialization failed!");
        return ret;
    }

    ESP_LOGI(TAG, "SPI Slave Initialized");

    // Start SPI communication task
    xTaskCreate(spi_secondary_task, "spi_secondary_task", 4096, NULL, 5, NULL);

    return ESP_OK;
}


void spi_secondary_task(void *arg) {
    receivedData.jsonInput = cJSON_CreateObject();
    init_ema(&purple_object_ema, 0.2f, "retro");
    init_ema(&april_tag_ema, 0.2, "fiducial");
    init_ema(&line_following_ema, 0.2, "retro");
    int count = 0;
    char *received_buffer = NULL;
    int received_buffer_size = 0;
    esp_err_t ret;
    bool command_flag = false;
    char new_buf[CHUNK_SIZE + 1] = {0};
    char send_buf[CHUNK_SIZE] = "ACK"; // Response to master

    spi_slave_transaction_t transaction;
    memset(&transaction, 0, sizeof(transaction));

    transaction.length = CHUNK_SIZE * 8;  // Transaction size in bits
    transaction.tx_buffer = send_buf;  // Data to send
    transaction.rx_buffer = new_buf;  // Buffer to receive data

    int64_t last_heartbeat_time = esp_timer_get_time();

    while (1) {
        // Send a heartbeat every second
        int64_t current_time = esp_timer_get_time();
        if (current_time - last_heartbeat_time >= 1000000) { // 1 second in microseconds
            if (strlen(command_to_send) == 0) {
                send_message("HEARTBEAT");
            }
            last_heartbeat_time = current_time;
        }

        // Wait for master to send data
        memset(new_buf, 0, sizeof(new_buf));
        if (strlen(command_to_send) > 0) {
            memset(send_buf, 0, sizeof(send_buf));  // Clear old data
            size_t len = strlen(command_to_send);
            if (len >= CHUNK_SIZE) len = CHUNK_SIZE - 1;
            memcpy(send_buf, command_to_send, len);
            send_buf[len] = '\0';  // Null-terminate
            command_to_send[0] = '\0';  // Mark message as sent
            command_flag = true;
            // ESP_LOGI(TAG, "sent: %s", command_to_send);
            // ESP_LOGI(TAG, "sent");
        }

        ret = spi_slave_transmit(SPI2_HOST, &transaction, pdMS_TO_TICKS(100));
        if (command_flag) {
            memset(send_buf, 0, sizeof(send_buf));
            memcpy(send_buf, "ACK", strlen("ACK"));
            command_flag = false;
        }
        
        if (ret == ESP_OK) {
            new_buf[CHUNK_SIZE] = '\0'; // Ensure null-termination
            // Append received data to json_buffer

            if (strncmp(new_buf, END_SIGNAL, strlen(END_SIGNAL)) == 0) {
                // ESP_LOGI(TAG, "End of Transmission received");
                //ESP_LOGI(TAG, "%s", received_buffer);
                process_received_data(received_buffer);
                ++count;
                // ESP_LOGI(TAG, "%d", count);
                if (!received_buffer) {
                    free(received_buffer);
                    received_buffer = NULL;
                }
                received_buffer_size = 0;
            } else {

                int new_size = received_buffer_size + strlen(new_buf);

                received_buffer = realloc(received_buffer, new_size + 1);
                if (!received_buffer) {
                    ESP_LOGE(TAG, "Memory allocation failed!");
                    free(received_buffer);
                    received_buffer = NULL;
                    return;
                }
                
                memcpy(received_buffer + received_buffer_size, new_buf, strlen(new_buf));
                received_buffer_size = new_size;
                received_buffer[received_buffer_size] = '\0';
            }
        }
    }
}

void process_received_data(char *input) {
    if (input == NULL) {
        ESP_LOGE(TAG, "nuh uh bud");
        return;
    }
    // ESP_LOGI(TAG, "HEAP: %u", (unsigned int)esp_get_free_heap_size());
    char message_type = input[0];
    char *message_data = input + 1;
    switch (message_type) {
        case 'J':
            //ESP_LOGI(TAG, "Processing JSON...");
            cJSON *receivedJson = cJSON_Parse(message_data);
            if (!receivedJson) {
                const char *error_ptr = cJSON_GetErrorPtr();
                if (error_ptr) {
                    printf("JSON Parsing Error: %s\n", error_ptr);
                } else {
                    printf("Unknown error occurred while parsing JSON.\n");
                }
                ESP_LOGE(TAG, "Invalid JSON!");
                ESP_LOGI(TAG, "Message Data: %s", message_data);
            } else {
                // ESP_LOGI(TAG, "Valid JSON received");
                if(xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100))) {
                    if(receivedData.jsonInput != NULL) {
                        cJSON_Delete(receivedData.jsonInput);
                    }
                    receivedData.jsonInput = cJSON_Duplicate(receivedJson, true);
                    cJSON_Delete(receivedJson);
                    xSemaphoreGive(data_mutex);
                } else {
                    cJSON_Delete(receivedJson);
                }
            }
            
            break;
        case 'M':
            // ESP_LOGI(TAG, "Processing Message...");
            if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100))) {
                receivedData.messageInput = message_data;
                xSemaphoreGive(data_mutex);
            }
            // ESP_LOGI(TAG, "message: %s", get_message());
            break;
    }
}

void send_message(char *message) {
    memset(command_to_send, 0, sizeof(command_to_send));  // Clear entire buffer
    memcpy(command_to_send, message, strlen(message));
    command_to_send[0] = '\0';
    memcpy(command_to_send, message, strlen(message));
}

char* get_message() {
    char* returnMessage = "";
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100))) {
        if (receivedData.messageInput != NULL) {
            returnMessage = receivedData.messageInput;
        } else {
            // ESP_LOGW(TAG, "messageInput is NULL");
            returnMessage = NULL;
        }
        xSemaphoreGive(data_mutex);
    }
    return returnMessage;
}

cJSON* get_last_json() {
    cJSON *returnJSON = NULL;

    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000))) {
        if (receivedData.jsonInput != NULL) {
            // Instead of duplicating, simply return the pointer.
            // NOTE: The caller MUST NOT free this pointer.
            returnJSON = receivedData.jsonInput;
        } else {
            // ESP_LOGW(TAG, "receivedData.jsonInput is NULL");
            // Create an empty object; the caller is responsible for freeing
            // this object if it's used, but ideally this branch is rarely hit.
            returnJSON = cJSON_CreateObject();
        }

        xSemaphoreGive(data_mutex);
    } else {
        // ESP_LOGW(TAG, "Failed to take semaphore");
        returnJSON = cJSON_CreateObject();
    }

    return returnJSON;
}


cJSON* get_retro() {
    cJSON *retro = cJSON_GetObjectItem(get_last_json(), "Retro");
    return retro;
}

double get_retro_ta() {
    cJSON *retro = get_retro();
    if (!retro) return 0.0;

    cJSON *retro_item = cJSON_GetArrayItem(retro, 0);
    if (!retro_item) return 0.0;

    cJSON *ta = cJSON_GetObjectItem(retro_item, "ta");
    if (!ta || !cJSON_IsNumber(ta)) {
        // ESP_LOGW(TAG, "ta not found or not a number");
        return 0.0;
    }

    // ESP_LOGI(TAG, "Retro ta: %f", ta->valuedouble);
    return ta->valuedouble;
}

double get_retro_tx() {
    cJSON *retro = get_retro();
    if (!retro) return 0.0;

    cJSON *retro_item = cJSON_GetArrayItem(retro, 0);
    if (!retro_item) return 0.0;

    cJSON *tx = cJSON_GetObjectItem(retro_item, "tx");
    if (!tx || !cJSON_IsNumber(tx)) {
        // ESP_LOGW(TAG, "tx not found or not a number");
        return 0.0;
    }

    // ESP_LOGI(TAG, "Retro tx: %f", tx->valuedouble);
    return tx->valuedouble;
}

double get_retro_tx_nocross() {
    cJSON *retro = get_retro();
    if (!retro) return 0.0;

    cJSON *retro_item = cJSON_GetArrayItem(retro, 0);
    if (!retro_item) return 0.0;

    cJSON *tx_nocross = cJSON_GetObjectItem(retro_item, "tx_nocross");
    if (!tx_nocross || !cJSON_IsNumber(tx_nocross)) {
        // ESP_LOGW(TAG, "tx_nocross not found or not a number");
        return 0.0;
    }

    // ESP_LOGI(TAG, "Retro tx_nocross: %f", tx_nocross->valuedouble);
    return tx_nocross->valuedouble;
}

double get_retro_txp() {
    cJSON *retro = get_retro();
    if (!retro) return 0.0;

    cJSON *retro_item = cJSON_GetArrayItem(retro, 0);
    if (!retro_item) return 0.0;

    cJSON *txp = cJSON_GetObjectItem(retro_item, "txp");
    if (!txp || !cJSON_IsNumber(txp)) {
        // ESP_LOGW(TAG, "txp not found or not a number");
        return 0.0;
    }

    // ESP_LOGI(TAG, "Retro txp: %f", txp->valuedouble);
    return txp->valuedouble;
}

double get_retro_ty() {
    cJSON *retro = get_retro();
    if (!retro) return 0.0;

    cJSON *retro_item = cJSON_GetArrayItem(retro, 0);
    if (!retro_item) return 0.0;

    cJSON *ty = cJSON_GetObjectItem(retro_item, "ty");
    if (!ty || !cJSON_IsNumber(ty)) {
        // ESP_LOGW(TAG, "ty not found or not a number");
        return 0.0;
    }

    // ESP_LOGI(TAG, "Retro ty: %f", ty->valuedouble);
    return ty->valuedouble;
}

double get_retro_ty_nocross() {
    cJSON *retro = get_retro();
    if (!retro) return 0.0;

    cJSON *retro_item = cJSON_GetArrayItem(retro, 0);
    if (!retro_item) return 0.0;

    cJSON *ty_nocross = cJSON_GetObjectItem(retro_item, "ty_nocross");
    if (!ty_nocross || !cJSON_IsNumber(ty_nocross)) {
        // ESP_LOGW(TAG, "ty_nocross not found or not a number");
        return 0.0;
    }

    // ESP_LOGI(TAG, "Retro ty_nocross: %f", ty_nocross->valuedouble);
    return ty_nocross->valuedouble;
}

double get_retro_typ() {
    cJSON *retro = get_retro();
    if (!retro) return 0.0;

    cJSON *retro_item = cJSON_GetArrayItem(retro, 0);
    if (!retro_item) return 0.0;

    cJSON *typ = cJSON_GetObjectItem(retro_item, "typ");
    if (!typ || !cJSON_IsNumber(typ)) {
        // ESP_LOGW(TAG, "typ not found or not a number");
        return 0.0;
    }

    // ESP_LOGI(TAG, "Retro typ: %f", typ->valuedouble);
    return typ->valuedouble;
}

cJSON* get_fiducial() {
    cJSON *fiducial = cJSON_GetObjectItem(get_last_json(), "Fiducial");
    if (!fiducial) {
        return NULL;
    }
    return fiducial;
}

int get_fiducial_fID() {
    cJSON *fiducial = get_fiducial();
    if (!fiducial) return 0;

    cJSON *fiducial_item = cJSON_GetArrayItem(fiducial, 0);
    if (!fiducial_item) return 0;

    cJSON *fID = cJSON_GetObjectItem(fiducial_item, "fID");
    if (!fID || !cJSON_IsNumber(fID)) {
        ESP_LOGW(TAG, "fID not found or not a number");
        return 0;
    }

    ESP_LOGI(TAG, "Fiducial fID: %d", fID->valueint);
    return fID->valueint;
}

cJSON* get_fiducial_pts() {
    cJSON *fiducial = get_fiducial();
    if (!fiducial) return 0;

    cJSON *fiducial_item = cJSON_GetArrayItem(fiducial, 0);
    if (!fiducial_item) return 0;

    cJSON *pts = cJSON_GetObjectItem(fiducial_item, "pts");
    if (!pts) return 0;
    return pts;
}

void get_point_at_index(int index, double* ret) {
    // Initialize output to zero
    ret[0] = 0;
    ret[1] = 0;

    cJSON *pts = get_fiducial_pts();
    if (!pts) return;

    // Check if the requested index is within bounds.
    int array_size = cJSON_GetArraySize(pts);
    if (index < 0 || index >= array_size) {
        // ESP_LOGW(TAG, "Index %d out of bounds (array size: %d)", index, array_size);
        return;
    }

    // Get the point at the desired index (0 for bottom left, 1 for bottom right)
    cJSON *point = cJSON_GetArrayItem(pts, index);
    if (!point) {
        //cJSON_Delete(pts);
        return;
    }

    cJSON *x_item = cJSON_GetArrayItem(point, 0);
    cJSON *y_item = cJSON_GetArrayItem(point, 1);
    if (x_item && y_item) {
        ret[0] = x_item->valuedouble;
        ret[1] = y_item->valuedouble;
    }
}


char* get_fiducial_fam() {
    cJSON *fiducial = get_fiducial();
    if (!fiducial) return NULL;

    cJSON *fiducial_item = cJSON_GetArrayItem(fiducial, 0);
    if (!fiducial_item) return NULL;

    cJSON *fam = cJSON_GetObjectItem(fiducial_item, "fam");
    if (!fam || !cJSON_IsString(fam)) {
        ESP_LOGW(TAG, "fam not found or not a string");
        return NULL;
    }

    ESP_LOGI(TAG, "Fiducial fam: %s", fam->valuestring);
    return fam->valuestring;
}

double get_fiducial_ta() {
    cJSON *fiducial = get_fiducial();
    if (!fiducial) return 0.0;

    cJSON *fiducial_item = cJSON_GetArrayItem(fiducial, 0);
    if (!fiducial_item) return 0.0;

    cJSON *ta = cJSON_GetObjectItem(fiducial_item, "ta");
    if (!ta || !cJSON_IsNumber(ta)) {
        ESP_LOGW(TAG, "ta not found or not a number");
        return 0.0;
    }

    // ESP_LOGI(TAG, "Fiducial ta: %f", ta->valuedouble);
    return ta->valuedouble;
}

double get_fiducial_tx() {
    cJSON *fiducial = get_fiducial();
    if (!fiducial) return 0.0;

    cJSON *fiducial_item = cJSON_GetArrayItem(fiducial, 0);
    if (!fiducial_item) return 0.0;

    cJSON *tx = cJSON_GetObjectItem(fiducial_item, "tx");
    if (!tx || !cJSON_IsNumber(tx)) {
        ESP_LOGW(TAG, "tx not found or not a number");
        return 0.0;
    }

    double txd = tx->valuedouble;
    if (!txd) return 0.0;
    // ESP_LOGI(TAG, "Fiducial tx: %f", tx->valuedouble);
    return txd;
}

double get_fiducial_tx_nocross() {
    cJSON *fiducial = get_fiducial();
    if (!fiducial) return 0.0;

    cJSON *fiducial_item = cJSON_GetArrayItem(fiducial, 0);
    if (!fiducial_item) return 0.0;

    cJSON *tx_nocross = cJSON_GetObjectItem(fiducial_item, "tx_nocross");
    if (!tx_nocross || !cJSON_IsNumber(tx_nocross)) {
        ESP_LOGW(TAG, "tx_nocross not found or not a number");
        return 0.0;
    }

    ESP_LOGI(TAG, "Fiducial tx_nocross: %f", tx_nocross->valuedouble);
    return tx_nocross->valuedouble;
}

double get_fiducial_txp() {
    cJSON *fiducial = get_fiducial();
    if (!fiducial) return 0.0;

    cJSON *fiducial_item = cJSON_GetArrayItem(fiducial, 0);
    if (!fiducial_item) return 0.0;

    cJSON *txp = cJSON_GetObjectItem(fiducial_item, "txp");
    if (!txp || !cJSON_IsNumber(txp)) {
        ESP_LOGW(TAG, "txp not found or not a number");
        return 0.0;
    }

    ESP_LOGI(TAG, "Fiducial txp: %f", txp->valuedouble);
    return txp->valuedouble;
}

double get_fiducial_ty() {
    cJSON *fiducial = get_fiducial();
    if (!fiducial) return 0.0;

    cJSON *fiducial_item = cJSON_GetArrayItem(fiducial, 0);
    if (!fiducial_item) return 0.0;

    cJSON *ty = cJSON_GetObjectItem(fiducial_item, "ty");
    if (!ty || !cJSON_IsNumber(ty)) {
        ESP_LOGW(TAG, "ty not found or not a number");
        return 0.0;
    }

    ESP_LOGI(TAG, "Fiducial ty: %f", ty->valuedouble);
    return ty->valuedouble;
}

double get_fiducial_ty_nocross() {
    cJSON *fiducial = get_fiducial();
    if (!fiducial) return 0.0;

    cJSON *fiducial_item = cJSON_GetArrayItem(fiducial, 0);
    if (!fiducial_item) return 0.0;

    cJSON *ty_nocross = cJSON_GetObjectItem(fiducial_item, "ty_nocross");
    if (!ty_nocross || !cJSON_IsNumber(ty_nocross)) {
        ESP_LOGW(TAG, "ty_nocross not found or not a number");
        return 0.0;
    }

    ESP_LOGI(TAG, "Fiducial ty_nocross: %f", ty_nocross->valuedouble);
    return ty_nocross->valuedouble;
}

double get_fiducial_typ() {
    cJSON *fiducial = get_fiducial();
    if (!fiducial) return 0.0;

    cJSON *fiducial_item = cJSON_GetArrayItem(fiducial, 0);
    if (!fiducial_item) return 0.0;

    cJSON *typ = cJSON_GetObjectItem(fiducial_item, "typ");
    if (!typ || !cJSON_IsNumber(typ)) {
        ESP_LOGW(TAG, "typ not found or not a number");
        return 0.0;
    }

    ESP_LOGI(TAG, "Fiducial typ: %f", typ->valuedouble);
    return typ->valuedouble;
}

double get_pID() {
    // Get the last JSON object
    cJSON *json = get_last_json();
    if (!json) {
        ESP_LOGW(TAG, "JSON object is NULL");
        return -1;  // Return an invalid value to indicate an error
    }
    
    // Get the pID item from the JSON
    cJSON *pID = cJSON_GetObjectItem(json, "pID");
    
    if (!pID) {
        // ESP_LOGW(TAG, "pID not found in JSON");
        return -1;  // Return an error value if pID is missing
    }
    
    // Ensure pID is a number
    if (!cJSON_IsNumber(pID)) {
        ESP_LOGW(TAG, "pID is not a number");
        return -1;  // Return an error value if pID is not a number
    }
    
    // If all checks pass, return the pID value
    // ESP_LOGI(TAG, "pID: %f", pID->valuedouble);
    return pID->valuedouble;
}


char* get_pTYPE() {
    cJSON *pTYPE = cJSON_GetObjectItem(get_last_json(), "pTYPE");
    if (!pTYPE || !cJSON_IsString(pTYPE)) {
        ESP_LOGW(TAG, "pTYPE not found or not a string");
        return NULL;
    }

    ESP_LOGI(TAG, "pTYPE: %s", pTYPE->valuestring);
    return pTYPE->valuestring;
}

int get_v() {
    cJSON *v = cJSON_GetObjectItem(get_last_json(), "v");
    if (!v || !cJSON_IsNumber(v)) {
        // ESP_LOGW(TAG, "v not found or not a number");
        return 0;
    }
    return v->valueint;
}

void init_ema(EMAState *ema, double alpha, char* type) {
    if (!ema) return;

    ema->alpha = alpha;
    ema->initialized = false;
    ema->pipeline_type = type;

    ema->ta_ema = 0.0f;
    ema->tx_ema = 0.0f;
    ema->tx_nocross_ema = 0.0f;
    ema->txp_ema = 0.0f;
    ema->ty_ema = 0.0f;
    ema->ty_nocross_ema = 0.0f;
    ema->typ_ema = 0.0f;

    ESP_LOGI(TAG, "EMA initialized with alpha=%.2f", alpha);
}

void reset_ema(EMAState *ema) {
    if (!ema) return;
    ema->initialized = false;
    ESP_LOGI(TAG, "EMA state reset");
}


void update_ema(EMAState *ema) {
    if (!ema) return;
    double point_bottom_left[2];
    double point_bottom_right[2];
    double point_top_right[2];
    double point_top_left[2];
    double ta = 0.0;
    double tx = 0.0;
    double tx_nocross = 0.0;
    double txp = 0.0;
    double ty = 0.0;
    double ty_nocross = 0.0;
    double typ = 0.0;
    if (strcmp(ema->pipeline_type, "fiducial") == 0) {
        get_point_at_index(0, point_bottom_left);
        get_point_at_index(1, point_bottom_right);
        get_point_at_index(2, point_top_right);
        get_point_at_index(3, point_top_left);

        ta          = get_fiducial_ta();
        tx          = get_fiducial_tx();
        tx_nocross  = get_fiducial_tx_nocross();
        txp         = get_fiducial_txp();
        ty          = get_fiducial_ty();
        ty_nocross  = get_fiducial_ty_nocross();
        typ         = get_fiducial_typ();
    } else if (strcmp(ema->pipeline_type, "retro") == 0) {
        ta          = get_retro_ta();
        tx          = get_retro_tx();
        tx_nocross  = get_retro_tx_nocross();
        txp         = get_retro_txp();
        ty          = get_retro_ty();
        ty_nocross  = get_retro_ty_nocross();
        typ         = get_retro_typ();
    }

    if (!ema->initialized) {
        ema->point_bottom_left[0] = point_bottom_left[0];
        ema->point_bottom_left[1] = point_bottom_left[1];

        ema->point_bottom_right[0] = point_bottom_right[0];
        ema->point_bottom_right[1] = point_bottom_right[1];

        ema->point_top_right[0] = point_top_right[0];
        ema->point_top_right[1] = point_top_right[1];

        ema->point_top_left[0] = point_top_left[0];
        ema->point_top_left[1] = point_top_left[1];

        ema->ta_ema = ta;
        ema->tx_ema = tx;
        ema->tx_nocross_ema = tx_nocross;
        ema->txp_ema = txp;
        ema->ty_ema = ty;
        ema->ty_nocross_ema = ty_nocross;
        ema->typ_ema = typ;
        ema->initialized = true;
        ESP_LOGI(TAG, "EMA first update: initialization complete");
    } else {
        ema->ta_ema          = ema->alpha * ta         + (1.0f - ema->alpha) * ema->ta_ema;
        ema->tx_ema          = ema->alpha * tx         + (1.0f - ema->alpha) * ema->tx_ema;
        ema->tx_nocross_ema  = ema->alpha * tx_nocross + (1.0f - ema->alpha) * ema->tx_nocross_ema;
        ema->txp_ema         = ema->alpha * txp        + (1.0f - ema->alpha) * ema->txp_ema;
        ema->ty_ema          = ema->alpha * ty         + (1.0f - ema->alpha) * ema->ty_ema;
        ema->ty_nocross_ema  = ema->alpha * ty_nocross + (1.0f - ema->alpha) * ema->ty_nocross_ema;
        ema->typ_ema         = ema->alpha * typ        + (1.0f - ema->alpha) * ema->typ_ema;

        ema->point_bottom_left[0] = ema->alpha * point_bottom_left[0] + (1.0f - ema->alpha) * ema->point_bottom_left[0];
        ema->point_bottom_left[1] = ema->alpha * point_bottom_left[1] + (1.0f - ema->alpha) * ema->point_bottom_left[1];

        ema->point_bottom_right[0] = ema->alpha * point_bottom_right[0] + (1.0f - ema->alpha) * ema->point_bottom_right[0];
        ema->point_bottom_right[1] = ema->alpha * point_bottom_right[1] + (1.0f - ema->alpha) * ema->point_bottom_right[1];

        ema->point_top_right[0] = ema->alpha * point_top_right[0] + (1.0f - ema->alpha) * ema->point_top_right[0];
        ema->point_top_right[1] = ema->alpha * point_top_right[1] + (1.0f - ema->alpha) * ema->point_top_right[1];

        ema->point_top_left[0] = ema->alpha * point_top_left[0] + (1.0f - ema->alpha) * ema->point_top_left[0];
        ema->point_top_left[1] = ema->alpha * point_top_left[1] + (1.0f - ema->alpha) * ema->point_top_left[1];
    }
}

void get_ema_point_bottom_left(const EMAState *ema, double ret[2]) {
    if (!ema || !ema->initialized) {
        ESP_LOGW(TAG, "EMA not initialized or invalid");
        return;
    }
    ret[0] = ema->point_bottom_left[0];
    ret[1] = ema->point_bottom_left[1];
}

void get_ema_point_bottom_right(const EMAState *ema, double ret[2]) {
    if (!ema || !ema->initialized) {
        ESP_LOGW(TAG, "EMA not initialized or invalid");
        return;
    }
    ret[0] = ema->point_bottom_right[0];
    ret[1] = ema->point_bottom_right[1];
}

void get_ema_point_top_right(const EMAState *ema, double ret[2]) {
    if (!ema || !ema->initialized) {
        ESP_LOGW(TAG, "EMA not initialized or invalid");
        return;
    }
    ret[0] = ema->point_top_right[0];
    ret[1] = ema->point_top_right[1];
}

void get_ema_point_top_left(const EMAState *ema, double ret[2]) {
    if (!ema || !ema->initialized) {
        ESP_LOGW(TAG, "EMA not initialized or invalid");
        return;
    }
    ret[0] = ema->point_top_left[0];
    ret[1] = ema->point_top_left[1];
}

double get_ema_ta(const EMAState *ema) {
    if (!ema || !ema->initialized) {
        ESP_LOGW(TAG, "EMA not initialized or invalid");
        return 0.0f;
    }
    return ema->ta_ema;
}

double get_ema_tx(const EMAState *ema) {
    if (!ema || !ema->initialized) {
        ESP_LOGW(TAG, "EMA not initialized or invalid");
        return 0.0f;
    }
    return ema->tx_ema;
}

double get_ema_tx_nocross(const EMAState *ema) {
    if (!ema || !ema->initialized) {
        ESP_LOGW(TAG, "EMA not initialized or invalid");
        return 0.0f;
    }
    return ema->tx_nocross_ema;
}

double get_ema_txp(const EMAState *ema) {
    if (!ema || !ema->initialized) {
        ESP_LOGW(TAG, "EMA not initialized or invalid");
        return 0.0f;
    }
    return ema->txp_ema;
}

double get_ema_ty(const EMAState *ema) {
    if (!ema || !ema->initialized) {
        ESP_LOGW(TAG, "EMA not initialized or invalid");
        return 0.0f;
    }
    return ema->ty_ema;
}

double get_ema_ty_nocross(const EMAState *ema) {
    if (!ema || !ema->initialized) {
        ESP_LOGW(TAG, "EMA not initialized or invalid");
        return 0.0f;
    }
    return ema->ty_nocross_ema;
}

double get_ema_typ(const EMAState *ema) {
    if (!ema || !ema->initialized) {
        ESP_LOGW(TAG, "EMA not initialized or invalid");
        return 0.0f;
    }
    return ema->typ_ema;
}

