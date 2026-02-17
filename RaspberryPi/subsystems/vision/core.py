from abc import ABC, abstractmethod
from dataclasses import dataclass

@dataclass
class VisionTarget:
    """
    Data structure to hold vision target information.
    """
    valid: bool = False
    x_offset: float = 0.0  # -1.0 (left) to 1.0 (right)
    distance: float = 0.0
    timestamp: float = 0.0

class VisionStrategy(ABC):
    """
    Abstract base class for different vision implementations (Mock, Pixy2, OpenCV).
    """
    
    @abstractmethod
    def start(self):
        """
        Start the camera/sensor.
        """
        pass

    @abstractmethod
    def update(self) -> VisionTarget:
        """
        Called in the vision loop to get the latest data.
        Returns a VisionTarget object.
        """
        pass

    @abstractmethod
    def stop(self):
        """
        Release resources.
        """
        pass
