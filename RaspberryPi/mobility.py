import pigpio
import time

# BCM pin numbers for each ESC
ESC_1_PIN = 17  # Left front
ESC_2_PIN = 18  # Left rear
ESC_3_PIN = 22  # Right front
ESC_4_PIN = 23  # Right rear

pi = pigpio.pi()

SIGNAL_MIN = 1050
SIGNAL_MAX = 1950

def set_esc_power(power_lf, power_lr, power_rf, power_rr):
    """Set power for four ESCs (-100 to 100)."""
    powers = [power_lf, power_lr, power_rf, power_rr]
    powers = [max(-100, min(100, p)) for p in powers]
    pulses = [int((p + 100) * (SIGNAL_MAX - SIGNAL_MIN) / 200 + SIGNAL_MIN) for p in powers]

    pi.set_servo_pulsewidth(ESC_1_PIN, pulses[0])
    pi.set_servo_pulsewidth(ESC_2_PIN, pulses[1])
    pi.set_servo_pulsewidth(ESC_3_PIN, pulses[2])
    pi.set_servo_pulsewidth(ESC_4_PIN, pulses[3])

# Movement mapping: direction -> (lf, lr, rf, rr)
DIRECTION_MAP = {
    "forward":      (1, 1, -1, -1),
    "backward":     (-1, -1, 1, 1),
    "left":         (-1, 1, -1, 1),
    "right":        (1, -1, 1, -1),
    "diagonal_NW":  (0, -1, 1, 0),
    "diagonal_NE":  (1, 0, 0, 1),
    "diagonal_SW":  (0, -1, -1, 0),
    "diagonal_SE":  (-1, 0, 0, -1),
    "spin_left":    (-1, -1, 1, 1),
    "spin_right":   (1, 1, -1, -1),
    "stop":         (0, 0, 0, 0)   # added stop command
}

def move(direction, power=10):
    """Move robot in a given direction with specified power."""
    if direction not in DIRECTION_MAP:
        raise ValueError(f"Invalid direction: {direction}")
    
    multipliers = DIRECTION_MAP[direction]
    set_esc_power(*(m * power for m in multipliers))

# Initialize ESCs to stop
for pin in [ESC_1_PIN, ESC_2_PIN, ESC_3_PIN, ESC_4_PIN]:
    pi.set_servo_pulsewidth(pin, 0)
time.sleep(2)  # Allow ESCs to arm

# Example usage
try:
    for dir_name in DIRECTION_MAP.keys():
        print(f"Moving {dir_name}")
        move(dir_name, 20)
        time.sleep(1)

except KeyboardInterrupt:
    print("Stopping motors...")
    move("stop")
    pi.stop()
