from core.state_machine import StateMachine
from subsystems.drivetrain import DriveTrain
from subsystems.vision import VisionSystem
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
    def __init__(self):
        super().__init__()
        # Initialize hardware components
        self.drive = DriveTrain()
        self.vision = VisionSystem()
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
        try:
            while True:
                self.update()
                time.sleep(0.05)
                # Example global check: Print vision status periodically
                # target = self.vision.get_latest_target()
                # if target.valid:
                #    print(f"Vision Target: {target.x_offset:.2f}")

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
        self.vision.stop()
        self.lcd.stop()
        self.esp_comm.close()

if __name__ == "__main__":
    robot = RobotController()
    robot.run()
