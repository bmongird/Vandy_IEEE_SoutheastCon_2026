#!/usr/bin/env python3
"""
IR Receiver Test Script for Raspberry Pi
-----------------------------------------
Uses GPIO edge-detection callbacks with timestamps to decode NEC frames.
This is far more reliable than busy-waiting.

Receiver data-out pin: GPIO 24 (physical pin 18)

Usage:
    sudo python3 ir_receiver_test.py
"""

import time
import sys
import threading

try:
    import RPi.GPIO as GPIO
except ImportError:
    print("ERROR: RPi.GPIO not found.  pip install RPi.GPIO")
    sys.exit(1)

# ── Pin ────────────────────────────────────────────────────────────────────────
IR_PIN = 24

# ── Protocol constants ─────────────────────────────────────────────────────────
ANTENNA_MASK = 0xF0
COLOR_MASK   = 0x0F
MAX_WRONG    = 8

ANTENNA_MAP = {
    0x00: "Ant_1",
    0x30: "Ant_2",
    0x50: "Ant_3",
    0x60: "Ant_4",
}

COLOR_MAP = {
    0x09: "RED",
    0x0A: "GRE",
    0x0C: "BLU",
    0x0F: "PUR",
}

# ── NEC timing tolerances (microseconds) ──────────────────────────────────────
LEADER_MARK_MIN  = 8000;  LEADER_MARK_MAX  = 10000
LEADER_SPACE_MIN = 3500;  LEADER_SPACE_MAX =  5500
BIT_MARK_MIN     =  200;  BIT_MARK_MAX     =  1000
ONE_SPACE_MIN    = 1400;  ONE_SPACE_MAX    =  1900
ZERO_SPACE_MIN   =  200;  ZERO_SPACE_MAX   =   800
FRAME_TIMEOUT    = 0.030  # 30ms — if no edge for this long, attempt decode

# ── State ──────────────────────────────────────────────────────────────────────
state = {
    "Ant_1":     {"status": "OFF", "color": "??"},
    "Ant_2":     {"status": "OFF", "color": "??"},
    "Ant_3":     {"status": "OFF", "color": "??"},
    "Ant_4":     {"status": "OFF", "color": "??"},
    "wrong":     0,
    "connected": "OFF",
}

# ── Terminal display ───────────────────────────────────────────────────────────
RESET  = "\033[0m";  BOLD   = "\033[1m"
GREEN  = "\033[92m"; RED    = "\033[91m"
YELLOW = "\033[93m"
color_ansi = {"RED": "\033[91m", "GRE": "\033[92m",
              "BLU": "\033[94m", "PUR": "\033[95m", "??": "\033[90m"}

def print_state():
    print("\n" + "─" * 36)
    print(f" {'Antenna':<12} {'Status':<8} Color")
    print("─" * 36)
    for ant in ("Ant_1", "Ant_2", "Ant_3", "Ant_4"):
        s = state[ant]
        status_str = (GREEN + "ON " + RESET) if s["status"] == "ON" else (YELLOW + "OFF" + RESET)
        col = s["color"]
        color_str = color_ansi.get(col, "") + col + RESET
        print(f" {BOLD}{ant:<12}{RESET} {status_str:<18} {color_str}")
    print("─" * 36)
    wrong_str = (RED + str(state['wrong']) + RESET) if state['wrong'] > 0 else str(state['wrong'])
    conn_str  = (GREEN + "ON" + RESET) if state["connected"] == "ON" else (YELLOW + "OFF" + RESET)
    print(f" {'Wrong Color':<12} {wrong_str}")
    print(f" {'Connected':<12} {conn_str}")
    print("─" * 36 + "\n")

def process_command(command):
    antenna_code = command & ANTENNA_MASK
    color_code   = command & COLOR_MASK

    print(f"[RX] command=0x{command:02X}  antenna=0x{antenna_code:02X}  color=0x{color_code:02X}")

    if antenna_code in ANTENNA_MAP:
        ant_name = ANTENNA_MAP[antenna_code]
        print(f"  Antenna : {ant_name}")
        state[ant_name]["status"] = "ON"
        state["connected"] = "ON"

        if color_code in COLOR_MAP:
            color_str = COLOR_MAP[color_code]
            print(f"  Color   : {color_str}")
            state[ant_name]["color"] = color_str
        else:
            print(f"  Color   : INVALID (0x{color_code:02X})")
            if state["wrong"] < MAX_WRONG:
                state["wrong"] += 1
                print(f"  Wrong count -> {state['wrong']}")
    else:
        print(f"  Antenna : INVALID (0x{antenna_code:02X})")
        state["connected"] = "ON"

    print_state()


# ── Edge-interrupt based NEC decoder ──────────────────────────────────────────
class NECDecoder:
    def __init__(self, pin):
        self.pin      = pin
        self.edges    = []        # list of (timestamp_us, level)
        self.lock     = threading.Lock()
        self.timer    = None

        GPIO.setmode(GPIO.BCM)
        GPIO.setup(pin, GPIO.IN, pull_up_down=GPIO.PUD_UP)
        GPIO.add_event_detect(pin, GPIO.BOTH, callback=self._edge_callback)
        print(f"[+] Edge detection started on GPIO {pin}")

    def _edge_callback(self, channel):
        level = GPIO.input(self.pin)
        ts    = time.perf_counter() * 1_000_000  # microseconds

        with self.lock:
            self.edges.append((ts, level))

        # Reset the frame-timeout timer on every edge
        if self.timer:
            self.timer.cancel()
        self.timer = threading.Timer(FRAME_TIMEOUT, self._attempt_decode)
        self.timer.start()

    def _attempt_decode(self):
        with self.lock:
            edges = list(self.edges)
            self.edges.clear()

        if len(edges) < 4:
            return

        # Debug: print raw edge count and first pulse duration
        first_dur = edges[1][0] - edges[0][0] if len(edges) > 1 else 0
        print(f"[DBG] {len(edges)} edges captured, first pulse ~{first_dur:.0f} µs")

        # Build list of pulse durations paired with the level that started them
        pulses = []
        for i in range(len(edges) - 1):
            dur = edges[i+1][0] - edges[i][0]
            pulses.append((edges[i][1], dur))  # (level_at_start, duration_us)

        result = self._decode_nec(pulses)
        if result is not None:
            process_command(result)
        else:
            print("[DBG] Decode failed — raw pulses (level, µs):")
            for lvl, dur in pulses[:10]:
                print(f"       {'LOW ' if lvl==0 else 'HIGH'} {dur:8.1f} µs")
            if len(pulses) > 10:
                print(f"       ... ({len(pulses)} total)")

    def _decode_nec(self, pulses):
        """
        Decode NEC from a list of (level, duration_us) tuples.
        Active-LOW receiver: burst = LOW, idle = HIGH.
        """
        idx = 0

        def expect(level, min_us, max_us):
            nonlocal idx
            if idx >= len(pulses):
                return False
            lvl, dur = pulses[idx]
            idx += 1
            return lvl == level and min_us <= dur <= max_us

        # Leader mark: LOW ~9ms
        if not expect(GPIO.LOW, LEADER_MARK_MIN, LEADER_MARK_MAX):
            # Try inverted polarity (some receivers output HIGH for burst)
            idx = 0
            if not expect(GPIO.HIGH, LEADER_MARK_MIN, LEADER_MARK_MAX):
                print("[DBG] No leader mark found")
                return None
            # Inverted polarity — flip expected levels
            burst_level = GPIO.HIGH
            space_level = GPIO.LOW
        else:
            burst_level = GPIO.LOW
            space_level = GPIO.HIGH

        # Leader space
        if not expect(space_level, LEADER_SPACE_MIN, LEADER_SPACE_MAX):
            print("[DBG] Leader space invalid")
            return None

        # 32 data bits
        bits = []
        for bit_num in range(32):
            # Bit mark
            if idx >= len(pulses):
                print(f"[DBG] Ran out of pulses at bit {bit_num}")
                return None
            lvl, dur = pulses[idx]; idx += 1
            if lvl != burst_level or not (BIT_MARK_MIN <= dur <= BIT_MARK_MAX):
                print(f"[DBG] Bad bit mark at bit {bit_num}: level={lvl} dur={dur:.1f}µs")
                return None

            # Bit space
            if idx >= len(pulses):
                # Last stop bit may be missing the trailing edge — treat as 0
                bits.append(0)
                continue
            lvl, dur = pulses[idx]; idx += 1
            if lvl != space_level:
                print(f"[DBG] Bad bit space level at bit {bit_num}")
                return None
            if ONE_SPACE_MIN <= dur <= ONE_SPACE_MAX:
                bits.append(1)
            elif ZERO_SPACE_MIN <= dur <= ZERO_SPACE_MAX:
                bits.append(0)
            else:
                print(f"[DBG] Bad bit space duration at bit {bit_num}: {dur:.1f}µs")
                return None

        # Reconstruct bytes (LSB first)
        def b2byte(b): return sum(bit << i for i, bit in enumerate(b))
        addr     = b2byte(bits[0:8])
        addr_inv = b2byte(bits[8:16])
        cmd      = b2byte(bits[16:24])
        cmd_inv  = b2byte(bits[24:32])

        print(f"[DBG] addr=0x{addr:02X} ~addr=0x{addr_inv:02X} "
              f"cmd=0x{cmd:02X} ~cmd=0x{cmd_inv:02X}")

        if (cmd ^ cmd_inv) != 0xFF:
            print(f"[DBG] Command checksum failed")
            return None

        return cmd

    def cleanup(self):
        if self.timer:
            self.timer.cancel()
        GPIO.cleanup()


# ── Main ───────────────────────────────────────────────────────────────────────
def main():
    print("=" * 40)
    print(" Raspberry Pi IR Receiver Test")
    print("=" * 40)
    print(f" GPIO {IR_PIN}  (physical pin 18)")
    print(" Ctrl+C to exit\n")

    decoder = NECDecoder(IR_PIN)
    print("[+] Waiting for IR signal …")
    print_state()

    try:
        while True:
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("\n[!] Exiting.")
    finally:
        decoder.cleanup()


if __name__ == "__main__":
    main()