import os
import argparse

# ── Antenna + Color definitions (straight from Arduino receiver code) ─────────
ANTENNAS = {
    1: 0x00,
    2: 0x30,
    3: 0x50,
    4: 0x60,
}

COLORS = {
    'R': 0x09,  # Red
    'G': 0x0A,  # Green
    'B': 0x0C,  # Blue
    'P': 0x0F,  # Purple
}

def send_IR(antenna: int, color: str, lirc_device: str = "/dev/lirc0") -> bool:
    """
    Sends an IR command to the secrobot receiver using ir-ctl.
    
    Args:
        antenna: Integer 1, 2, 3, or 4
        color: String 'R', 'G', 'B', or 'P'
        lirc_device: Path to the lirc device (default /dev/lirc0)
    """
    color = color.upper()
    
    if antenna not in ANTENNAS:
        print(f"Error: Invalid antenna ({antenna}). Must be 1, 2, 3, or 4.")
        return False
        
    if color not in COLORS:
        print(f"Error: Invalid color ({color}). Must be 'R', 'G', 'B', or 'P'.")
        return False
        
    # Construct the 8-bit command byte expected by the Arduino receiver
    # Upper nibble = Antenna code, Lower nibble = Color code
    cmd_byte = ANTENNAS[antenna] | COLORS[color]
    
    # ir-ctl -S allows sending a raw protocol scancode directly 
    cmd = f"sudo ir-ctl -S nec:{hex(cmd_byte)} -d {lirc_device}"
    print(f"Sending: Antenna {antenna} + Color {color} -> Scancode: {hex(cmd_byte)}")
    
    # Execute the command in the shell
    return os.system(cmd) == 0

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="IR Blaster using ir-ctl")
    parser.add_argument("--ant", type=int, choices=[1, 2, 3, 4], required=True, help="Antenna number (1-4)")
    parser.add_argument("--color", type=str, choices=['R', 'G', 'B', 'P', 'r', 'g', 'b', 'p'], required=True, help="Color code (R, G, B, P)")
    parser.add_argument("--device", default="/dev/lirc0", help="LIRC device file (default: /dev/lirc0)")
    
    args = parser.parse_args()
    send_IR(args.ant, args.color, args.device)
