import time
import math
from subsystems.vision.core import VisionStrategy, VisionTarget

class Pixy2Strategy(VisionStrategy):
    """
    Implementation of VisionStrategy for the Pixy2 Camera.
    """
    def __init__(self):
        self.connected = False
        # self.pixy = None

    def start(self):
        print("Pixy2 Strategy Started (Mocking Connection)")
        # try:
        #     self.pixy = ... connect to pixy ...
        #     self.connected = True
        # except:
        #     self.connected = False
        self.connected = True

    def update(self) -> VisionTarget:
        """
        Reads from the Pixy2 and returns a VisionTarget.
        """
        if not self.connected:
            return VisionTarget()
            
        # Mock Data for now until pyserial/i2c is hooked up to Pixy
        # In reality: blocks = self.pixy.get_blocks()
        
        # Simulate a target moving left to right
        t = time.time()
        offset = math.sin(t * 2) 
        
        return VisionTarget(
            valid=True,
            x_offset=offset,
            distance=1.5, # meters
            timestamp=t
        )

    def stop(self):
        print("Pixy2 Strategy Stopped")
        self.connected = False
