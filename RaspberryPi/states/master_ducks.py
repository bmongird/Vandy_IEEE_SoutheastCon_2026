from core.state_machine import State

class MasterDucksState(State):
    """
    Pi state for handling the Ducks interaction.
    Sends 'DUCKS' to the ESP, waits for completion, 
    then returns to Idle.
    """
    def __init__(self, controller):
        super().__init__("MasterDucks")
        self.controller = controller
        self.is_done = False

    def enter(self):
        self.logger.info("Entering MASTER DUCKS state.")
        self.controller.esp_comm.send_state("DUCKS")
        self.is_done = False

    def exit(self):
        self.logger.info("Exiting MASTER DUCKS state.")

    def update(self):
        if not self.is_done:
            if self.controller.esp_comm.is_state_completed():
                self.logger.info("ESP Ducks state completed.")
                self.is_done = True

    def check_transitions(self):
        if self.is_done:
            from states.master_idle import MasterIdleState
            return MasterIdleState(self.controller)
        return None
