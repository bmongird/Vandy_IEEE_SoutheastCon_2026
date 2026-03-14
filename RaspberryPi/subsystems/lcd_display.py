#!/usr/bin/env python3
"""
lcd_antenna.py

Displays the scanning status of 4 antennas on a 2-line I2C LCD.
The display cycles between antennas 1–2 and 3–4 every 5 seconds.

    ANTENNA 1: R
    ANTENNA 2: G

Each antenna colour is one of R, G, B, or P.

Public API
----------
    set_antenna(antenna: int, color: str) -> None
        Set the colour for antenna 1–4.  colour must be "R", "G", "B", or "P".

    get_antenna(antenna: int) -> str
        Return the current colour string for the given antenna.

Usage as a standalone script (demo):
    python3 lcd_antenna.py

Usage as an importable module:
    from lcd_antenna import AntennaDisplay

    display = AntennaDisplay()
    display.start()                    # begins the 5 s cycling in background
    display.set_antenna(1, "R")
    display.set_antenna(2, "G")
    display.set_antenna(3, "B")
    display.set_antenna(4, "P")
    ...
    display.stop()                     # clears LCD and stops background thread

Wiring (40-pin Raspberry Pi header):
    Pin 2  (5 V)               -> LCD VCC
    Pin 6  (GND)               -> LCD GND
    Pin 3  (GPIO2 / SDA1)     -> LCD SDA
    Pin 5  (GPIO3 / SCL1)     -> LCD SCL
"""

from time import sleep, monotonic
from threading import Thread, Event, Lock
import logging
import signal
import sys

try:
    import smbus2 as smbus
except ImportError:
    import smbus

# ---------------------------------------------------------------------------
# I2C / LCD constants
# ---------------------------------------------------------------------------
LCD_CLEARDISPLAY   = 0x01
LCD_ENTRYMODESET   = 0x04
LCD_DISPLAYCONTROL = 0x08
LCD_FUNCTIONSET    = 0x20
LCD_SETDDRAMADDR   = 0x80

LCD_ENTRYLEFT           = 0x02
LCD_ENTRYSHIFTDECREMENT = 0x00
LCD_DISPLAYON  = 0x04
LCD_CURSOROFF  = 0x00
LCD_BLINKOFF   = 0x00
LCD_2LINE   = 0x08
LCD_5x8DOTS = 0x00

LCD_BACKLIGHT = 0x08
ENABLE        = 0b00000100

VALID_COLORS = {"R", "G", "B", "P", "F"}
CYCLE_INTERVAL = 3  # seconds


# ---------------------------------------------------------------------------
# Low-level LCD driver (same proven helpers from lcd_clock.py)
# ---------------------------------------------------------------------------

class _LCDDriver:
    """Thin wrapper around the HD44780 I2C backpack."""

    def __init__(self, bus_num: int = 1, address: int | None = None):
        self.bus = smbus.SMBus(bus_num)
        self.addr = address or self._find_address()
        self._init_hw()

    # -- address detection --------------------------------------------------

    def _find_address(self) -> int:
        for addr in (0x27, 0x3F):
            try:
                self.bus.write_byte(addr, 0)
                return addr
            except OSError:
                continue
        raise RuntimeError(
            "No I2C device at 0x27 or 0x3F — check wiring / run 'i2cdetect -y 1'"
        )

    # -- low-level I2C helpers ----------------------------------------------

    def _strobe(self, data: int) -> None:
        self.bus.write_byte(self.addr, data | ENABLE)
        sleep(0.0005)
        self.bus.write_byte(self.addr, data & ~ENABLE)
        sleep(0.001)

    def _write_nibble(self, nibble: int) -> None:
        data = (nibble & 0xF0) | LCD_BACKLIGHT
        self.bus.write_byte(self.addr, data)
        self._strobe(data)

    def _write_byte(self, bits: int, mode: int) -> None:
        high = mode | (bits & 0xF0) | LCD_BACKLIGHT
        low  = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT
        self.bus.write_byte(self.addr, high)
        self._strobe(high)
        self.bus.write_byte(self.addr, low)
        self._strobe(low)

    # -- public LCD primitives ----------------------------------------------

    def command(self, cmd: int) -> None:
        self._write_byte(cmd, 0)

    def write_char(self, ch: str) -> None:
        self._write_byte(ord(ch), 1)

    def clear(self) -> None:
        self.command(LCD_CLEARDISPLAY)
        sleep(0.002)

    def write_string(self, message: str, line: int) -> None:
        addr = 0x00 if line == 1 else 0x40
        self.command(LCD_SETDDRAMADDR | addr)
        for ch in message.ljust(16)[:16]:
            self.write_char(ch)

    # -- init sequence ------------------------------------------------------

    def _init_hw(self) -> None:
        sleep(0.05)
        for _ in range(3):
            self._write_nibble(0x30)
            sleep(0.005)
        self._write_nibble(0x20)
        sleep(0.005)

        self.command(LCD_FUNCTIONSET | LCD_2LINE | LCD_5x8DOTS)
        self.command(LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF)
        self.clear()
        self.command(LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT)


# ---------------------------------------------------------------------------
# Public antenna display class
# ---------------------------------------------------------------------------

class AntennaDisplay:
    """Manages 4 antenna colour states and cycles them on a 2-line LCD.

    The display alternates between showing antennas 1–2 and 3–4 every
    ``CYCLE_INTERVAL`` seconds.  Updating an antenna colour via
    ``set_antenna()`` is thread-safe and takes effect on the next refresh.
    """

    def __init__(self, bus_num: int = 1, address: int | None = None):
        self._lcd = _LCDDriver(bus_num, address)
        self._lock = Lock()
        self._stop_event = Event()
        self._thread: Thread | None = None

        # Default all antennas to R
        self._colors = {1: "F", 2: "F", 3: "F", 4: "F"}

        logging.info(f"LCD ready at 0x{self._lcd.addr:02X}")

    # -- public API ---------------------------------------------------------

    def set_antenna(self, antenna: int, color: str) -> None:
        """Set the colour for *antenna* (1–4).

        Parameters
        ----------
        antenna : int
            Antenna number, 1 through 4.
        color : str
            One of ``"R"``, ``"G"``, ``"B"``, ``"P"``, or ``"F"`` (case-insensitive).

        Raises
        ------
        ValueError
            If *antenna* or *color* is out of range.
        """
        color = color.upper()
        if antenna not in (1, 2, 3, 4):
            raise ValueError(f"antenna must be 1–4, got {antenna}")
        if color not in VALID_COLORS:
            raise ValueError(f"color must be one of {VALID_COLORS}, got '{color}'")
        with self._lock:
            self._colors[antenna] = color

    def get_antenna(self, antenna: int) -> str:
        """Return the current colour for *antenna* (1–4)."""
        if antenna not in (1, 2, 3, 4):
            raise ValueError(f"antenna must be 1–4, got {antenna}")
        with self._lock:
            return self._colors[antenna]

    def start(self) -> None:
        """Start the background display loop."""
        if self._thread and self._thread.is_alive():
            return
        self._stop_event.clear()
        self._thread = Thread(target=self._loop, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        """Stop the background loop and clear the LCD."""
        self._stop_event.set()
        if self._thread:
            self._thread.join(timeout=3)
        try:
            self._lcd.clear()
        except Exception:
            pass

    # -- internal -----------------------------------------------------------

    def _format_line(self, antenna: int) -> str:
        """Return a 16-char display line like 'ANTENNA 1: RED  '."""
        with self._lock:
            color = self._colors[antenna]
            
        color_map = {
            "R": "RED",
            "G": "GRE",
            "B": "BLU",
            "P": "PUR",
            "F": "OFF"
        }
        display_color = color_map.get(color, color)
        return f"ANTENNA {antenna}: {display_color}"

    def _loop(self) -> None:
        """Cycle between pages of two antennas every CYCLE_INTERVAL seconds."""
        pages = [(1, 2), (3, 4)]
        page_idx = 0

        while not self._stop_event.is_set():
            a, b = pages[page_idx]
            self._lcd.write_string(self._format_line(a), 1)
            self._lcd.write_string(self._format_line(b), 2)

            # Wait for CYCLE_INTERVAL but check stop flag frequently
            deadline = monotonic() + CYCLE_INTERVAL
            while monotonic() < deadline:
                if self._stop_event.is_set():
                    return
                sleep(0.1)

            page_idx = (page_idx + 1) % len(pages)


# ---------------------------------------------------------------------------
# Standalone demo
# ---------------------------------------------------------------------------

def main() -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s  %(message)s",
        datefmt="%H:%M:%S",
    )

    display = AntennaDisplay()

    def cleanup(*_args):
        logging.info("Stopping display")
        display.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, cleanup)
    signal.signal(signal.SIGTERM, cleanup)

    # Set some example colours
    display.set_antenna(1, "N")
    display.set_antenna(2, "N")
    display.set_antenna(3, "N")
    display.set_antenna(4, "N")

    display.start()
    logging.info("Antenna display running — Ctrl+C to stop")

    # Keep main thread alive; in your own code you'd do other work here
    while True:
        sleep(1)


if __name__ == "__main__":
    main()