#!/usr/bin/env python3
"""
IR Transmitter Test Script for Raspberry Pi
--------------------------------------------
Sends NEC IR frames formatted to match the Arduino receiver code.

Protocol (from receiver):
  Command byte = antenna_code | color_code
  Antenna codes (upper nibble): 0x00, 0x30, 0x50, 0x60
  Color codes   (lower nibble): 0x09=Red, 0x0A=Green, 0x0C=Blue, 0x0F=Purple

Usage:
    sudo python3 ir_transmitter_test.py [--pin GPIO_PIN]

Requirements:
    pip install RPi.GPIO

Default pin: GPIO 17 (BCM).
"""

import time
import argparse
import sys

try:
    import RPi.GPIO as GPIO
except ImportError:
    print("ERROR: RPi.GPIO not found. Install with:  pip install RPi.GPIO")
    sys.exit(1)

# ── Configuration ─────────────────────────────────────────────────────────────
DEFAULT_PIN    = 17
CARRIER_HZ     = 38_000
CARRIER_PERIOD = 1.0 / CARRIER_HZ

# NEC protocol timings
NEC_LEADER_MARK  = 9_000e-6
NEC_LEADER_SPACE = 4_500e-6
NEC_BIT_MARK     = 562.5e-6
NEC_ONE_SPACE    = 1_687.5e-6
NEC_ZERO_SPACE   = 562.5e-6
NEC_FINAL_MARK   = 562.5e-6

# ── Antenna + Color definitions (straight from receiver code) ─────────────────
ANTENNAS = {
    "Ant_1": 0x00,
    "Ant_2": 0x30,
    "Ant_3": 0x50,
    "Ant_4": 0x60,
}

COLORS = {
    "Red":    0x09,
    "Green":  0x0A,
    "Blue":   0x0C,
    "Purple": 0x0F,
}

NEC_ADDRESS = 0x00  # receiver doesn't check address, but we send 0x00


# ── IR low-level helpers ───────────────────────────────────────────────────────
def setup(pin):
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(pin, GPIO.OUT, initial=GPIO.LOW)
    print(f"[+] GPIO {pin} configured as output.")

def teardown():
    GPIO.cleanup()
    print("[+] GPIO cleaned up.")

def mark(pin, duration):
    """38 kHz carrier burst for `duration` seconds."""
    end = time.perf_counter() + duration
    while time.perf_counter() < end:
        GPIO.output(pin, GPIO.HIGH)
        time.sleep(CARRIER_PERIOD / 2)
        GPIO.output(pin, GPIO.LOW)
        time.sleep(CARRIER_PERIOD / 2)

def space(duration):
    time.sleep(duration)

def send_byte(pin, byte):
    """Send one byte LSB-first using NEC encoding."""
    for i in range(8):
        mark(pin, NEC_BIT_MARK)
        space(NEC_ONE_SPACE if (byte & (1 << i)) else NEC_ZERO_SPACE)

def send_nec(pin, address, command):
    """Send a full NEC frame: leader | addr | ~addr | cmd | ~cmd | stop."""
    mark(pin, NEC_LEADER_MARK)
    space(NEC_LEADER_SPACE)
    send_byte(pin, address)
    send_byte(pin, address ^ 0xFF)
    send_byte(pin, command)
    send_byte(pin, command ^ 0xFF)
    mark(pin, NEC_FINAL_MARK)
    space(40e-3)


# ── Test sequences ─────────────────────────────────────────────────────────────
def test_all_combinations(pin):
    """Send every valid antenna+color combination once."""
    print("\n[TEST 1] All antenna/color combinations")
    for ant_name, ant_code in ANTENNAS.items():
        for color_name, color_code in COLORS.items():
            command = ant_code | color_code
            print(f"  {ant_name} + {color_name:6s}  ->  command=0x{command:02X}", end="  ... ", flush=True)
            send_nec(pin, NEC_ADDRESS, command)
            time.sleep(0.1)
            print("sent")

def test_invalid_color(pin):
    """Send a valid antenna with an invalid color code to trigger the wrong-color counter."""
    print("\n[TEST 2] Invalid color (should increment wrong-color counter on LCD)")
    invalid_color = 0x01  # not 0x09/0x0A/0x0C/0x0F
    for ant_name, ant_code in ANTENNAS.items():
        command = ant_code | invalid_color
        print(f"  {ant_name} + INVALID  ->  command=0x{command:02X}", end="  ... ", flush=True)
        send_nec(pin, NEC_ADDRESS, command)
        time.sleep(0.1)
        print("sent")

def test_invalid_antenna(pin):
    """Send an invalid antenna code to trigger the 'Invalid Antenna' branch."""
    print("\n[TEST 3] Invalid antenna code (should set Connected=ON without updating Ant rows)")
    invalid_ant = 0x20  # not 0x00/0x30/0x50/0x60
    command = invalid_ant | 0x09   # pair with Red color code
    print(f"  INVALID_ANT + Red  ->  command=0x{command:02X}", end="  ... ", flush=True)
    send_nec(pin, NEC_ADDRESS, command)
    time.sleep(0.1)
    print("sent")

def test_specific(pin, ant_name, color_name):
    """Send one specific antenna+color pair."""
    ant_code   = ANTENNAS[ant_name]
    color_code = COLORS[color_name]
    command    = ant_code | color_code
    print(f"\n[SINGLE] {ant_name} + {color_name}  ->  command=0x{command:02X}", end="  ... ", flush=True)
    send_nec(pin, NEC_ADDRESS, command)
    print("sent")


# ── Main ───────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="IR TX Test - matched to Arduino receiver")
    parser.add_argument("--pin",   type=int, default=DEFAULT_PIN,
                        help=f"BCM GPIO pin (default: {DEFAULT_PIN})")
    parser.add_argument("--test",  choices=["all", "combos", "invalid_color", "invalid_ant"],
                        default="all", help="Test to run (default: all)")
    parser.add_argument("--ant",   choices=list(ANTENNAS.keys()),
                        help="Send a single antenna (use with --color)")
    parser.add_argument("--color", choices=list(COLORS.keys()),
                        help="Send a single color (use with --ant)")
    args = parser.parse_args()

    print("=" * 52)
    print(" Raspberry Pi -> Arduino IR Transmitter Test")
    print("=" * 52)
    print(f" GPIO pin : {args.pin} (BCM)")
    print(f" Address  : 0x{NEC_ADDRESS:02X}")
    print()

    setup(args.pin)

    try:
        if args.ant and args.color:
            test_specific(args.pin, args.ant, args.color)
        else:
            if args.test in ("all", "combos"):
                test_all_combinations(args.pin)
            if args.test in ("all", "invalid_color"):
                test_invalid_color(args.pin)
            if args.test in ("all", "invalid_ant"):
                test_invalid_antenna(args.pin)

        print("\n[OK] Done.")

    except KeyboardInterrupt:
        print("\n[!] Interrupted.")
    finally:
        teardown()


if __name__ == "__main__":
    main()