import logging
import multiprocessing
import time
from .core import VisionTarget, VisionStrategy
from .pixy2 import Pixy2Strategy

class VisionSystem:
    """
    Subsystem for the vision system.
    Manages the separate vision process and uses a pluggable strategy.
    """
    def __init__(self, strategy_name="pixy2"):
        self.logger = logging.getLogger("VisionSystem")
        self.manager = multiprocessing.Manager()
        self.target_data = self.manager.Namespace()
        self.target_data.valid = False
        self.target_data.x_offset = 0.0
        self.target_data.distance = 0.0
        self.target_data.timestamp = 0.0
        self.strategy_name = strategy_name
        
        self.process = multiprocessing.Process(
            target=self._run_vision_loop, 
            args=(self.target_data, self.strategy_name),
            daemon=True
        )
        self.process.start()
        self.logger.info(f"Vision system initialized with strategy: {strategy_name}")

    @staticmethod
    def _run_vision_loop(shared_data, strategy_name):
        """
        Static method to act as the entry point for the process.
        """
        # Factory logic for strategy
        strategy = None
        if strategy_name == "pixy2":
            strategy = Pixy2Strategy()
        else:
            print(f"Unknown strategy {strategy_name}, defaulting to mock")
            strategy = Pixy2Strategy() # Fallback

        strategy.start()
        print(f"Vision Process Running with {strategy.__class__.__name__}...")
        
        try:
            while True:
                # Get data from strategy
                target = strategy.update()
                
                # Update shared memory
                shared_data.timestamp = target.timestamp
                shared_data.valid = target.valid
                shared_data.x_offset = target.x_offset
                shared_data.distance = target.distance
                
                # Avoid busy loop if strategy is fast
                time.sleep(0.01) 
                
        except KeyboardInterrupt:
            pass
        finally:
            strategy.stop()

    def get_latest_target(self) -> VisionTarget:
        """
        Returns the most recent vision target data.
        """
        return VisionTarget(
            valid=self.target_data.valid,
            x_offset=self.target_data.x_offset,
            distance=self.target_data.distance,
            timestamp=self.target_data.timestamp
        )

    def stop(self):
        """
        Stops the vision process.
        """
        if self.process.is_alive():
            self.process.terminate()
            self.process.join()
