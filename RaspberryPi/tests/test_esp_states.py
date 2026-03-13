import logging
import time
import sys
import os

# Add Raspberry Pi root to sys.path so we can import modules
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from core.state_machine import StateMachine
from subsystems.esp_comm import ESPCommunicator
from states.master_idle import MasterIdleState

class MockRobotController(StateMachine):
    """
    Mock controller that only has the components needed for testing the ESP states.
    """
    def __init__(self):
        super().__init__()
        self.esp_comm = ESPCommunicator()
        
        # Start in the Master Idle state
        self.change_state(MasterIdleState(self))
        
    def trigger(self, state_name):
        """
        Trigger a transition in the idle state.
        """
        if isinstance(self.current_state, MasterIdleState):
            self.current_state.trigger_transition(state_name)

def test_states():
    # Set up basic logging to console
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(message)s')
    
    print("--- Starting Pi-ESP State Machine Test ---")
    mock_controller = MockRobotController()
    
    print("\n--- Sending Trigger to start ANTENNA sequence ---")
    mock_controller.trigger("ANTENNA")
    
    # Run the main loop
    cycles = 0
    try:
        while True:
            mock_controller.update()
            
            # Simulate real-time loop delay
            time.sleep(0.5)
            cycles += 1
            
            # Print state periodically to show what's happening
            if cycles % 2 == 0:
                print(f"[{cycles*0.5:.1f}s] Current Pi State: {mock_controller.current_state.name} | Pi thinks ESP is: {mock_controller.esp_comm.current_esp_state}")
            
            # Test complete condition: returned to idle after doing antenna
            if isinstance(mock_controller.current_state, MasterIdleState) and cycles > 2:
                print("\n--- Returned to Idle! Sequence complete. ---")
                
                # Check ducks transition too
                if mock_controller.esp_comm._target_state == "IDLE" and hasattr(mock_controller, "ducks_done"):
                    break
                elif not hasattr(mock_controller, "ducks_done"):
                    print("\n--- Sending Trigger to start DUCKS sequence ---")
                    mock_controller.trigger("DUCKS")
                    mock_controller.ducks_done = True
                else:
                    break
                
    except KeyboardInterrupt:
        print("\nTest manually interrupted.")

if __name__ == "__main__":
    test_states()
