import logging
import time
import random

class ESPCommunicator:
    """
    Mocks ESP communication for testing the Master Pi states.
    In a real system, this would use Serial, I2C, SPI, or network sockets.
    """
    def __init__(self):
        self.logger = logging.getLogger("ESPCommunicator")
        self.current_esp_state = "IDLE"
        self._target_state = "IDLE"
        self._completion_time = 0
        self.logger.info("Mock ESP Communicator initialized.")

    def send_state(self, state_name: str):
        """
        Send a state command to the ESP.
        """
        self.logger.info(f"Sending state '{state_name}' to ESP.")
        self.current_esp_state = state_name
        self._target_state = state_name
        # Simulate an operation taking between 2 and 5 seconds
        self._completion_time = time.time() + random.uniform(2.0, 5.0)

    def is_state_completed(self) -> bool:
        """
        Check if the ESP has signaled that it completed the requested state.
        Returns True if complete, False if still working.
        """
        if self._target_state == "IDLE":
            # If target is idle, there's no long-running process to "complete"
            return True
        
        if time.time() >= self._completion_time:
            self.logger.info(f"ESP simulated completion for state '{self._target_state}'.")
            # In a real system the ESP might return to IDLE automatically,
            # but usually the Pi tells it to go IDLE. We'll just say the job is done.
            self._target_state = "IDLE"
            return True
            
        return False
