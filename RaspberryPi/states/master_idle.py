from core.state_machine import State
from states.master_antenna import MasterAntennaState
from states.master_ducks import MasterDucksState

class MasterIdleState(State):
    """
    Idle state for the Pi as the master.
    It waits for a command to transition to another state.
    Sends 'IDLE' to the ESP upon entering.
    """
    def __init__(self, controller):
        super().__init__("MasterIdle")
        self.controller = controller
        self.next_state = None

    def enter(self):
        self.logger.info("Entering MASTER IDLE state.")
        self.controller.esp_comm.send_state("IDLE")
        self.next_state = None

    def exit(self):
        self.logger.info("Exiting MASTER IDLE state.")

    def update(self):
        # Poll the pushbutton and keyboard to trigger the transition to DUCKS
        key = self.controller.keyboard.get_key()
        # if self.controller.pushbutton.is_pressed() or (key and key.lower() == 'e'):
        if key and key.lower() == 'e':
            self.logger.info("Pushbutton or 'e' key pressed. Transitioning to DUCKS.")
            self.next_state = "DUCKS"

    def check_transitions(self):
        if self.next_state == "ANTENNA":
            return MasterAntennaState(self.controller)
        elif self.next_state == "DUCKS":
            return MasterDucksState(self.controller)
        return None

    def trigger_transition(self, state_name: str):
        """
        Helper method to force a transition from outside (e.g., from the test script).
        """
        if state_name in ["ANTENNA", "DUCKS"]:
            self.logger.info(f"Trigger received for {state_name}")
            self.next_state = state_name
        else:
            self.logger.warning(f"Unknown trigger {state_name}")
