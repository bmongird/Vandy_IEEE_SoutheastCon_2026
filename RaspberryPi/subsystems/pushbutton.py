import RPi.GPIO as GPIO
import logging
import time

class Pushbutton:
    """
    Subsystem for reading a pushbutton state using RPi.GPIO.
    """
    
    def __init__(self, pin=22):
        """
        Initialize the pushbutton.
        Sets up the GPIO pin as an input with an internal pull-up resistor.
        """
        self.pin = pin
        self.logger = logging.getLogger("Pushbutton")
        
        # Use BCM numbering
        GPIO.setmode(GPIO.BCM)
        # Set up the pin as an input with a pull-up resistor
        GPIO.setup(self.pin, GPIO.IN, pull_up_down=GPIO.PUD_UP)
        self.logger.info(f"Initialized pushbutton on GPIO {self.pin}")

    def is_pressed(self):
        """
        Check if the button is pressed.
        Returns True if the pin is pulled LOW, False otherwise.
        """
        # Active low: button press pulls the pin to GND
        return GPIO.input(self.pin) == GPIO.LOW

    def cleanup(self):
        """Clean up GPIO resources."""
        GPIO.cleanup(self.pin)
        self.logger.info(f"Cleaned up pushbutton on GPIO {self.pin}")

if __name__ == "__main__":
    # Test script if run directly
    print("Testing Pushbutton on GPIO 22...")
    logging.basicConfig(level=logging.INFO)
    button = Pushbutton(pin=22)
    try:
        while True:
            if button.is_pressed():
                print("Button PRESSED!")
            else:
                print("Button released.")
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("Stopped by user.")
    finally:
        button.cleanup()
