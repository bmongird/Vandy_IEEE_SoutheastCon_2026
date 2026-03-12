import spidev
import time

# Initialize SPI
spi = spidev.SpiDev()
# Open SPI bus 0, device (CS) 0. This uses GPIO8 as CS0 on Raspberry Pi
spi.open(0, 0)

# Set SPI speed and mode
spi.max_speed_hz = 1000000 # 1 MHz
spi.mode = 0 # ESP32 slave is configured for mode 0

print("Starting SPI communication with ESP32...")
buf_size = 32

try:
    count = 0
    while True:
        # Prepare data to send (must be exactly 32 bytes to match ESP32 buffer)
        msg = f"Hello from Pi {count}"
        count += 1
        
        # Convert string to list of ASCII values
        send_data = [ord(c) for c in msg]
        
        # Pad with zeros to exactly 32 bytes
        send_data.extend([0] * (buf_size - len(send_data)))

        # Perform SPI transaction (send and receive simultaneously)
        # xfer2 keeps CS active between blocks
        recv_data = spi.xfer2(send_data)
        
        # Convert received bytes to string, ignoring null terminators
        recv_msg = "".join([chr(b) for b in recv_data if b != 0])
        
        print(f"Sent: {msg}")
        print(f"Received: {recv_msg}")
        print("-" * 20)
        
        time.sleep(1)

except KeyboardInterrupt:
    print("Stopping...")
finally:
    spi.close()
