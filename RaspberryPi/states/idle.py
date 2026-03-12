import time
from core.state_machine import State

class IdleState(State):
    """
    Idle state for the robot.
    """
    def __init__(self, controller):
        super().__init__("Idle")
        self.controller = controller
        self.start_time = 0

    def enter(self):
        self.logger.info("Entering IDLE state.")
        self.start_time = time.time()

    def exit(self):
        self.logger.info("Exiting IDLE state.")

    def update(self):
        pass

    def check_transitions(self):
        # Transition to DETECT after 1 second of idleness
        if time.time() - self.start_time >= 1.0:
            from states.detect import DetectState
            return DetectState(self.controller)
        return None
