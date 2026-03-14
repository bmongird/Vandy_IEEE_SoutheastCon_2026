from core.state_machine import StateMachine
from subsystems.drivetrain import DriveTrain
from subsystems.lcd_subsystem import LCDSubsystem
from subsystems.esp_comm import ESPCommunicator
from subsystems.keyboard_input import KeyboardInput
from subsystems.pushbutton import Pushbutton
import time

class RobotController(StateMachine):
    """
    Top-level controller for the robot, inheriting from StateMachine.
    Manages subsystems (DriveTrain, Vision, ESP, Keyboard, Pushbutton).
    """
    def __init__(self, debug=False):
        super().__init__()
        self.debug = debug
        # Initialize hardware components
        self.drive = DriveTrain()
        self.lcd = LCDSubsystem()
        self.esp_comm = ESPCommunicator()
        self.keyboard = KeyboardInput()
        self.pushbutton = Pushbutton(pin=22)
        self.initialize_hardware()

        # Start in the Master Idle state
        from states.master_idle import MasterIdleState
        self.change_state(MasterIdleState(self))
        
    def initialize_hardware(self):
        """
        Setup hardware interfaces.
        """
        print("Initializing Robot Hardware...")
        # Any specific hardware setup can go here
        pass

    def run(self):
        """
        Main loop for the robot controller.
        """
        cycles = 0
        try:
            while True:
                self.update()
                time.sleep(0.05)
                cycles += 1
                
                # In debug mode, print state periodically (every 0.5s = 10 loops at 0.05s)
                if self.debug and cycles % 10 == 0:
                    current_state_name = getattr(self.current_state, "name", type(self.current_state).__name__)
                    esp_state = getattr(self.esp_comm, "current_esp_state", "Unknown")
                    print(f"[{cycles*0.05:.1f}s] Current Pi State: {current_state_name} | Pi thinks ESP is: {esp_state}")


        except KeyboardInterrupt:
            print("Robot stopped by user.")
        finally:
            self.cleanup()

    def cleanup(self):
        """
        Clean up resources.
        """
        print("Cleaning up resources...")
        self.drive.stop()
        self.lcd.stop()
        self.esp_comm.close()

if __name__ == "__main__":
    robot = RobotController()
    robot.run()
