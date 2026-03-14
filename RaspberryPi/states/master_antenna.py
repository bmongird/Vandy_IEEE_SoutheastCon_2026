from core.state_machine import State
from subsystems.vision.pixy_detect import detect_led_color

class MasterAntennaState(State):
    """
    Pi state for handling the Antenna interaction.
    Sends 'ANTENNA' to the ESP, waits for completion, 
    then initiates reading the LED and updates the screen.
    """
    def __init__(self, controller):
        super().__init__("MasterAntenna")
        self.controller = controller
        self.is_done = False
        self.action_completed = False

    def enter(self):
        self.logger.info("Entering MASTER ANTENNA state.")
        self.controller.esp_comm.send_state("ANTENNA")
        self.is_done = False
        self.action_completed = False

    def exit(self):
        self.logger.info("Exiting MASTER ANTENNA state.")

    def update(self):
        if not self.is_done:
            if self.controller.esp_comm.is_state_completed():
                self.logger.info("ESP Antenna state completed.")
                
                # Read LED color from Pixy2
                self.logger.info("Reading LED from Vision System...")
                color = detect_led_color()
                    
                self.logger.info(f"Detected LED Color: {color}")
                
                # Update screen accordingly
                if hasattr(self.controller, 'lcd'):
                    self.logger.info(f"Updating LCD with color {color}")
                    self.controller.lcd.set_antenna(1, color)
                else:
                    self.logger.info(f"[SIMULATED] LCD updated with color {color}")
                
                self.is_done = True

    def check_transitions(self):
        if self.is_done:
            from states.master_idle import MasterIdleState
            return MasterIdleState(self.controller)
        return None
