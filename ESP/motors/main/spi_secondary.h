#ifndef SPI_SECONDARY_H
#define SPI_SECONDARY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"

#define CHUNK_SIZE 64  // Define chunk size for SPI transactions
#define INITIALIZATION_MESSAGE_TRANSMIT     "Establishing Communication"
#define INITIALIZATION_MESSAGE_RECEIVE      "Communication Established"

extern SemaphoreHandle_t data_mutex;

typedef struct {
    cJSON *jsonInput;
    char *messageInput;
} SPI_received_data_t;

typedef struct {
    bool initialized;
    double alpha;  // Smoothing factor (between 0.0 and 1.0)
    char* pipeline_type;

    double point_bottom_left[2];
    double point_bottom_right[2];
    double point_top_right[2];
    double point_top_left[2];

    double ta_ema;
    double tx_ema;
    double tx_nocross_ema;
    double txp_ema;
    double ty_ema;
    double ty_nocross_ema;
    double typ_ema;

} EMAState;

// Function to initialize the SPI secondary stage
esp_err_t spi_secondary_init(void);

void spi_secondary_task(void *arg);

void process_received_data(char* input);

void send_message(char *message);

char* get_message();

cJSON* get_last_json();

cJSON* get_retro();

double get_retro_ta();

double get_retro_tx();

double get_retro_tx_nocross();

double get_retro_txp();

double get_retro_ty();

double get_retro_ty_nocross();

double get_retro_typ();

cJSON* get_fiducial();

int get_fiducial_fID();

char* get_fiducial_fam();

cJSON* get_fiducial_pts();

void get_point_at_index(int index, double* ret);

double get_fiducial_ta();

double get_fiducial_tx();

double get_fiducial_tx_nocross();

double get_fiducial_txp();

double get_fiducial_ty();

double get_fiducial_ty_nocross();

double get_fiducial_typ();

double get_pID();

char* get_pTYPE();

int get_v();

void init_ema(EMAState *ema, double alpha, char* type);

void reset_ema(EMAState *ema);

void update_ema(EMAState *ema);

void get_ema_point_bottom_left(const EMAState *ema, double ret[2]);
void get_ema_point_bottom_right(const EMAState *ema, double ret[2]);
void get_ema_point_top_right(const EMAState *ema, double ret[2]);
void get_ema_point_top_left(const EMAState *ema, double ret[2]);
double get_ema_ta(const EMAState *ema);
double get_ema_tx(const EMAState *ema);
double get_ema_tx_nocross(const EMAState *ema);
double get_ema_txp(const EMAState *ema);
double get_ema_ty(const EMAState *ema);
double get_ema_ty_nocross(const EMAState *ema);
double get_ema_typ(const EMAState *ema);

#endif  // SPI_SECONDARY_H
