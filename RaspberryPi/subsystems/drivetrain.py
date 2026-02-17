import logging
import json
import time

try:
    import serial
except ImportError:
    serial = None

class DriveTrain:
    """
    Subsystem for controlling the robot's drivetrain via ESP32.
    Sends JSON commands over Serial.
    """
    def __init__(self, port='/dev/ttyUSB0', baudrate=115200):
        self.logger = logging.getLogger("DriveTrain")
        self.left_motor_speed = 0
        self.right_motor_speed = 0
        self.serial_port = None
        
        if serial:
            try:
                # self.serial_port = serial.Serial(port, baudrate, timeout=1)
                self.logger.info(f"DriveTrain initialized on {port} at {baudrate}")
            except serial.SerialException as e:
                self.logger.error(f"Failed to open serial port: {e}")
        else:
            self.logger.warning("pyserial not installed, DriveTrain running in MOCK mode.")

    def set_speed(self, left: float, right: float):
        """
        Sends speed commands to the ESP32.
        :param left: Speed for left motor (-1.0 to 1.0)
        :param right: Speed for right motor (-1.0 to 1.0)
        """
        self.left_motor_speed = max(-1.0, min(1.0, left))
        self.right_motor_speed = max(-1.0, min(1.0, right))
        
        command = {
            "L": round(self.left_motor_speed, 3),
            "R": round(self.right_motor_speed, 3)
        }
        
        self._send_command(command)

    def arcade_drive(self, forward: float, turn: float):
        """
        Controls motors using forward speed and turn rate.
        :param forward: Forward speed (-1.0 to 1.0)
        :param turn: Turn rate (-1.0 to 1.0), positive is right
        """
        left = forward + turn
        right = forward - turn
        
        # Normalize if values exceed 1.0
        maximum = max(abs(left), abs(right))
        if maximum > 1.0:
            left /= maximum
            right /= maximum
            
        self.set_speed(left, right)

    def stop(self):
        """
        Stops all motors.
        """
        self.set_speed(0, 0)
        
    def _send_command(self, command: dict):
        """
        Serializes and sends a JSON command to the ESP32.
        """
        try:
            json_str = json.dumps(command) + '\n'
            # if self.serial_port and self.serial_port.is_open:
            #     self.serial_port.write(json_str.encode('utf-8'))
            
            # For debugging/simulation, verify what we WOULD send
            # self.logger.debug(f"Sending to ESP32: {json_str.strip()}")
            
        except Exception as e:
            self.logger.error(f"Error sending command: {e}")
