import sys
import select
import termios
import tty
import atexit
import logging

class KeyboardInput:
    """
    Subsystem for reading non-blocking single character keyboard input.
    Changes terminal mode to cbreak so it doesn't require pressing Enter.
    """
    
    def __init__(self):
        self.logger = logging.getLogger("KeyboardInput")
        self.fd = sys.stdin.fileno()
        try:
            self.old_settings = termios.tcgetattr(self.fd)
            # Set to cbreak mode to read keys immediately without requiring Enter
            tty.setcbreak(self.fd)
            # Ensure cleanup happens even if the script crashes
            atexit.register(self.cleanup)
            self.logger.info("Initialized keyboard input (cbreak mode)")
        except Exception as e:
            self.logger.warning(f"Keyboard input initialization failed (not a terminal?): {e}")
            self.old_settings = None

    def get_key(self):
        """
        Returns the character currently pressed, or None if no key is pressed.
        """
        if self.old_settings is None:
            return None
            
        try:
            # select.select check if there is data waiting on stdin
            if select.select([sys.stdin], [], [], 0) == ([sys.stdin], [], []):
                return sys.stdin.read(1)
        except Exception:
            return None
        return None

    def cleanup(self):
        """Restore terminal settings."""
        if self.old_settings:
            try:
                termios.tcsetattr(self.fd, termios.TCSADRAIN, self.old_settings)
                self.old_settings = None
            except Exception:
                pass

if __name__ == "__main__":
    # Test script if run directly
    print("Testing Keyboard Input... Press 'e' to test. Press 'q' to quit.")
    logging.basicConfig(level=logging.INFO)
    kbd = KeyboardInput()
    try:
        import time
        while True:
            key = kbd.get_key()
            if key:
                print(f"Key pressed: {key}")
                if key.lower() == 'q':
                    break
            time.sleep(0.1)
    finally:
        kbd.cleanup()
