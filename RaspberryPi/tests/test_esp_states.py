import logging
import time
import sys
import os

# Add Raspberry Pi root to sys.path so we can import modules
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from robot_controller import RobotController
from states.master_idle import MasterIdleState

def test_states():
    # Set up basic logging to console
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(message)s')
    
    print("--- Starting Pi-ESP State Machine Test ---")
    robot = RobotController()
    
    print("\n--- Sending Trigger to start ANTENNA sequence ---")
    if isinstance(robot.current_state, MasterIdleState):
        robot.current_state.trigger_transition("ANTENNA")
    
    # Run the main loop
    cycles = 0
    try:
        while True:
            robot.update()
            
            # Simulate real-time loop delay
            time.sleep(0.5)
            cycles += 1
            
            # Print state periodically to show what's happening
            if cycles % 2 == 0:
                print(f"[{cycles*0.5:.1f}s] Current Pi State: {robot.current_state.name} | Pi thinks ESP is: {robot.esp_comm.current_esp_state}")
            
            # Test complete condition: returned to idle after doing antenna
            if isinstance(robot.current_state, MasterIdleState) and cycles > 2:
                print("\n--- Returned to Idle! Sequence complete. ---")
                
                # Check ducks transition too
                if robot.esp_comm._target_state == "IDLE" and hasattr(robot, "ducks_done"):
                    break
                elif not hasattr(robot, "ducks_done"):
                    print("\n--- Sending Trigger to start DUCKS sequence ---")
                    robot.current_state.trigger_transition("DUCKS")
                    robot.ducks_done = True
                else:
                    break
                
    except KeyboardInterrupt:
        print("\nTest manually interrupted.")
    finally:
        robot.cleanup()

if __name__ == "__main__":
    test_states()
