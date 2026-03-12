import pigpio
import time

LED_PIN = 18  # BCM pin number

pi = pigpio.pi()
if not pi.connected:
    print("Failed to connect to pigpio daemon")
    exit()

# Set the pin as output
pi.set_mode(LED_PIN, pigpio.OUTPUT)

try:
    while True:
        pi.write(LED_PIN, 1)  # LED ON
        print("LED ON")
        time.sleep(1)
        
        pi.write(LED_PIN, 0)  # LED OFF
        print("LED OFF")
        time.sleep(1)

except KeyboardInterrupt:
    pass

# Cleanup
pi.write(LED_PIN, 0)
pi.stop()
