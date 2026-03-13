import logging
import time
import spidev

# --- SPI Protocol Constants ---
# Command IDs (Pi -> ESP)
CMD_STATE   = 0x01  # Send a state command
CMD_QUERY   = 0x02  # Query current status

# Response IDs (ESP -> Pi)
RSP_STATE_ACK = 0x81  # ESP acknowledged the state command
RSP_STATUS    = 0x82  # ESP status response

# State codes (shared between Pi and ESP)
STATE_CODES = {
    "IDLE":    0x00,
    "DUCKS":   0x01,
    "ANTENNA": 0x02,
}
STATE_NAMES = {v: k for k, v in STATE_CODES.items()}

# Status flags (byte 2 of ESP response)
STATUS_IN_PROGRESS = 0x00
STATUS_COMPLETED   = 0x01

# Fixed message size in bytes
BUF_SIZE = 32


class ESPCommunicator:
    """
    Communicates with the ESP32 over SPI (bus 0, device 0).

    Protocol (32-byte fixed-size messages):
        Byte 0: Command / Response ID
        Byte 1: State code
        Byte 2: Status flag (in responses)
        Bytes 3-31: Reserved (zero-padded)
    """

    def __init__(self, bus=0, device=0, speed_hz=1_000_000, mode=0):
        self.logger = logging.getLogger("ESPCommunicator")
        self.current_esp_state = "IDLE"
        self._target_state = "IDLE"

        # Open SPI bus
        self.spi = spidev.SpiDev()
        self.spi.open(bus, device)
        self.spi.max_speed_hz = speed_hz
        self.spi.mode = mode

        self.logger.info(
            f"SPI ESPCommunicator initialized on bus {bus}, device {device}, "
            f"{speed_hz / 1e6:.1f} MHz, mode {mode}."
        )

    # ------------------------------------------------------------------
    # Public API (same interface the state machine already uses)
    # ------------------------------------------------------------------

    def send_state(self, state_name: str):
        """
        Send a state command to the ESP over SPI.
        """
        code = STATE_CODES.get(state_name.upper())
        if code is None:
            self.logger.error(f"Unknown state '{state_name}'. Valid: {list(STATE_CODES.keys())}")
            return

        payload = self._build_message(CMD_STATE, code)
        self.logger.info(f"Sending state '{state_name}' (0x{code:02X}) to ESP.")

        try:
            self.spi.xfer2(payload)
        except Exception as e:
            self.logger.error(f"SPI send_state failed: {e}")
            return

        self.current_esp_state = state_name
        self._target_state = state_name

    def is_state_completed(self) -> bool:
        """
        Query the ESP for completion status.

        Two SPI transfers are needed:
          1. Send the query command so the ESP can prepare its response.
          2. Read the response from the ESP.

        Returns True if the ESP reports the task as completed.
        """
        if self._target_state == "IDLE":
            return True

        state_code = STATE_CODES.get(self._target_state.upper(), 0x00)

        # Transfer 1: send the query
        query = self._build_message(CMD_QUERY, state_code)
        try:
            self.spi.xfer2(query)
        except Exception as e:
            self.logger.error(f"SPI status query failed: {e}")
            return False

        # Small delay to let the ESP prepare its response buffer
        time.sleep(0.005)

        # Transfer 2: read the response (send zeros)
        dummy = [0x00] * BUF_SIZE
        try:
            response = self.spi.xfer2(dummy)
        except Exception as e:
            self.logger.error(f"SPI status read failed: {e}")
            return False

        # Parse response
        resp_id = response[0]
        resp_state = response[1]
        resp_status = response[2]

        if resp_id == RSP_STATUS and resp_status == STATUS_COMPLETED:
            completed_name = STATE_NAMES.get(resp_state, f"0x{resp_state:02X}")
            self.logger.info(f"ESP reports completion for state '{completed_name}'.")
            self._target_state = "IDLE"
            return True

        return False

    def close(self):
        """
        Close the SPI connection. Call this during cleanup.
        """
        self.spi.close()
        self.logger.info("SPI connection closed.")

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    @staticmethod
    def _build_message(command: int, state_code: int, status: int = 0x00) -> list:
        """
        Build a 32-byte message list.
        """
        msg = [0x00] * BUF_SIZE
        msg[0] = command
        msg[1] = state_code
        msg[2] = status
        return msg
