#ifndef SPI_SECONDARY_H
#define SPI_SECONDARY_H

#include <stdint.h>
#include "esp_err.h"

/* ── 32-byte binary protocol (must match Pi's esp_comm.py) ─────────── */
#define SPI_BUF_SIZE  32

/* Command IDs (Pi -> ESP, byte 0 of received message) */
#define CMD_STATE     0x01   /* Pi is sending a state command          */
#define CMD_QUERY     0x02   /* Pi is querying completion status       */

/* Response IDs (ESP -> Pi, byte 0 of sent message) */
#define RSP_STATE_ACK 0x81   /* ESP acknowledges the state command     */
#define RSP_STATUS    0x82   /* ESP status response                    */

/* State codes (byte 1, shared between Pi and ESP) */
#define STATE_IDLE    0x00
#define STATE_DUCKS   0x01
#define STATE_ANTENNA 0x02

/* Status flags (byte 2 of ESP response) */
#define STATUS_IN_PROGRESS 0x00
#define STATUS_COMPLETED   0x01

/**
 * Initialize SPI slave hardware and start the communication task.
 */
esp_err_t spi_secondary_init(void);

/**
 * Get the most recent state command received from the Pi.
 * Returns the state code (STATE_IDLE, STATE_DUCKS, STATE_ANTENNA).
 */
uint8_t get_current_command(void);

/**
 * Tell the SPI layer that the requested state has been completed.
 * The next time the Pi sends a CMD_QUERY, the ESP will respond
 * with STATUS_COMPLETED.
 */
void report_state_complete(void);

#endif  /* SPI_SECONDARY_H */
