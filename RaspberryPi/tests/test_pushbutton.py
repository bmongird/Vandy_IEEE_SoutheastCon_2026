import sys
import os
import time

# Add the parent directory to sys.path so we can import from subsystems
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from subsystems.pushbutton import Pushbutton

def test_pushbutton_read(pin=22, duration=10.0, poll_interval=0.1):
    """
    Live hardware test for the Pushbutton subsystem.
    
    Polls the button state for `duration` seconds and prints transitions.
    Press the button a few times while the test runs to verify detection.
    
    Args:
        pin:          GPIO BCM pin the button is wired to (default 22).
        duration:     How long to run the test in seconds (default 10).
        poll_interval: How often to poll the pin in seconds (default 0.1).
    """
    print("=" * 50)
    print(f" Pushbutton Test  —  GPIO {pin}")
    print(f" Press the button within the next {duration}s")
    print("=" * 50)

    button = Pushbutton(pin=pin)

    try:
        presses = 0
        last_state = button.is_pressed()
        deadline = time.time() + duration

        while time.time() < deadline:
            state = button.is_pressed()

            if state and not last_state:
                presses += 1
                print(f"  [DOWN]  press #{presses}  (t={time.time():.2f})")
            elif not state and last_state:
                print(f"  [UP]    release      (t={time.time():.2f})")

            last_state = state
            time.sleep(poll_interval)

    except KeyboardInterrupt:
        print("\n[INTERRUPTED] Test stopped by user.")
    finally:
        button.cleanup()

    print("-" * 50)
    if presses > 0:
        print(f"[PASS] Detected {presses} button press(es).")
    else:
        print("[WARN] No presses detected — is the button wired correctly?")
    print("=" * 50)

if __name__ == "__main__":
    test_pushbutton_read()
