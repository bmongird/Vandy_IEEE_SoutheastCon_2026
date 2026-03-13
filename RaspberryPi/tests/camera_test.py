import sys
import os
import time
import logging

# Add the parent directory to sys.path so we can import from subsystems
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from subsystems.vision.pixy_detect import detect_led_color

def main():
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
    logging.info("Starting infinite camera test loop...")
    logging.info("Press Ctrl+C to stop.")
    
    try:
        while True:
            # We use a shorter timeout for the infinite loop test
            # so it responds more quickly to changes/no-detection
            result = detect_led_color(timeout_seconds=5.0)
            print(f"[{time.strftime('%H:%M:%S')}] Detected Color: {result}")
            
            # Small delay between checks
            time.sleep(0.5)
            
    except KeyboardInterrupt:
        logging.info("Camera test stopped by user.")

if __name__ == "__main__":
    main()
