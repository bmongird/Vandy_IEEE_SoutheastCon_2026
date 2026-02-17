from core.state_machine import StateMachine
from subsystems.drivetrain import DriveTrain
from subsystems.vision import VisionSystem

class RobotController(StateMachine):
    """
    Top-level controller for the robot, inheriting from StateMachine.
    Manages subsystems (DriveTrain, Vision).
    """
    def __init__(self):
        super().__init__()
        # Initialize hardware components
        self.drive = DriveTrain()
        self.vision = VisionSystem()
        self.initialize_hardware()
        
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

if __name__ == "__main__":
    robot = RobotController()
    robot.run()
