import time
from core.state_machine import State
from subsystems.vision.pixy_detect import detect_led_color

class DetectState(State):
    """
    Detect state for the robot.
    """
    def __init__(self, controller):
        super().__init__("Detect")
        self.controller = controller
        self.done = False

    def enter(self):
        self.logger.info("Entering DETECT state.")
        self.done = False

    def exit(self):
        self.logger.info("Exiting DETECT state.")

    def update(self):
        if not self.done:
            self.logger.info("Detecting LED color...")
            # Blocking call for up to 5 seconds looking for color
            color = detect_led_color(timeout_seconds=5.0)
            self.logger.info(f"Color detected: {color}")
            
            # Update the screen; defaults to N if None or F
            if color in ['R', 'G', 'B', 'P']:
                self.controller.lcd.set_antenna(1, color)
            else:
                self.logger.warning(f"No valid color found (got '{color}'). Setting LCD to 'N' for failed detection visibility.")
                self.controller.lcd.set_antenna(1, 'N')
            
            self.done = True

    def check_transitions(self):
        if self.done:
            from states.idle import IdleState
            return IdleState(self.controller)
        return None
