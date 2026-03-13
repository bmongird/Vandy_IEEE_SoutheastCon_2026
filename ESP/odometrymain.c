#!/usr/bin/env python3
"""
REV Through Bore Encoder (REV-11-1271) — Raspberry Pi Test Script
==================================================================
Tests both quadrature (ABI) and absolute duty-cycle outputs of the
Broadcom AEAT-8800-Q24-based encoder using gpiozero.

Encoder specs (from datasheet):
  - Quadrature:  2048 CPR  →  8192 counts/rev with 4× decoding
  - Index pulse: once per revolution, 90°e wide (default)
  - Absolute:    duty-cycle output, period ≈ 1025 µs (≈ 975.6 Hz)
                 pulse 1 µs → 0°,  1024 µs → 360°,  10-bit resolution
  - Logic level: 3.3 V  (safe for Raspberry Pi GPIO)

Wiring (using the included JST-PH 6-pin → 4×3-pin breakout cable):
  ┌──────────────────────────────────────────────────────┐
  │  Encoder Wire    Signal       Pi Pin (BCM)           │
  │  ─────────────   ──────────   ──────────────         │
  │  Red             VCC (3.3V)   Pin 1  (3V3 Power)     │
  │  Black           GND          Pin 6  (Ground)        │
  │  Yellow          A  (quad)    GPIO 17  (Pin 11)      │
  │  Green           B  (quad)    GPIO 27  (Pin 13)      │
  │  Blue            Index        GPIO 22  (Pin 15)      │
  │  White           Abs / PWM    GPIO 23  (Pin 16)      │
  └──────────────────────────────────────────────────────┘
  Adjust the GPIO numbers below if your wiring differs.

Requirements:
  pip install gpiozero           # usually pre-installed on Raspberry Pi OS

  For best edge-timing accuracy, install pigpio as the *pin factory*:
    sudo apt-get install pigpio
    sudo systemctl start pigpiod
    export GPIOZERO_PIN_FACTORY=pigpio

  gpiozero will fall back to RPi.GPIO automatically if pigpio isn't running.

Usage:
  python3 test_through_bore_encoder.py
"""

import time
import sys
import signal
from collections import deque

from gpiozero import RotaryEncoder, Button, DigitalInputDevice

# ─── Configuration ──────────────────────────────────────────────────────────

# GPIO pins (BCM numbering) — change these to match your wiring
PIN_A     = 17   # Quadrature channel A
PIN_B     = 27   # Quadrature channel B
PIN_INDEX = 22   # Index pulse (once per revolution)
PIN_ABS   = 23   # Absolute duty-cycle output

# Encoder specs
# gpiozero's RotaryEncoder reports in half-step increments, giving
# 2 counts per full quadrature cycle → 2048 CPR × 2 = 4096 steps/rev.
STEPS_PER_REV  = 4096
ABS_MIN_PULSE  = 1         # Minimum pulse width (0°) in µs
ABS_MAX_PULSE  = 1024      # Maximum pulse width (360°) in µs

# Velocity measurement
VEL_WINDOW_SEC = 0.2       # Rolling window for speed calculation

# ─── Globals ────────────────────────────────────────────────────────────────

index_count    = 0
abs_angle_deg  = 0.0        # Latest absolute angle in degrees
abs_high_us    = 0           # Latest high-pulse width in µs
_abs_rise_time = 0.0         # Timestamp of last rising edge (seconds)
abs_edge_count = 0           # Edge counter for diagnostics
step_history   = deque()     # (timestamp, steps) for velocity calc
running        = True

# ─── Absolute duty-cycle reader (manual edge timing) ───────────────────────
# gpiozero doesn't have a PWM-input class, so we time edges ourselves.

def _on_abs_rise():
    global _abs_rise_time, abs_edge_count
    _abs_rise_time = time.monotonic()
    abs_edge_count += 1

def _on_abs_fall():
    global abs_high_us, abs_angle_deg, abs_edge_count
    if _abs_rise_time == 0.0:
        return
    high_sec = time.monotonic() - _abs_rise_time
    high_us  = high_sec * 1_000_000
    abs_high_us = int(high_us)
    # Map pulse width to 0°–360° (spec: 1 µs = 0°, 1024 µs = 360°)
    fraction = max(0.0, min(1.0, (high_us - ABS_MIN_PULSE) / (ABS_MAX_PULSE - ABS_MIN_PULSE)))
    abs_angle_deg = fraction * 360.0
    abs_edge_count += 1

# ─── Index pulse counter ───────────────────────────────────────────────────

def _on_index_rise():
    global index_count
    index_count += 1

# ─── Helper functions ───────────────────────────────────────────────────────

def steps_to_degrees(steps):
    return (steps % STEPS_PER_REV) / STEPS_PER_REV * 360.0

def steps_to_revolutions(steps):
    return steps / STEPS_PER_REV

def compute_velocity_rpm(encoder):
    """Compute RPM from recent step history using a rolling window."""
    now = time.monotonic()
    step_history.append((now, encoder.steps))

    # Purge old entries
    while step_history and (now - step_history[0][0]) > VEL_WINDOW_SEC * 2:
        step_history.popleft()
    if len(step_history) < 2:
        return 0.0

    t0, s0 = step_history[0]
    t1, s1 = step_history[-1]
    dt = t1 - t0
    if dt < 1e-6:
        return 0.0
    steps_per_sec = (s1 - s0) / dt
    return (steps_per_sec / STEPS_PER_REV) * 60.0

# ─── Diagnostics ────────────────────────────────────────────────────────────

def run_diagnostics(encoder, index_btn, abs_input):
    """Quick check that signals are actually toggling."""
    print("\n╔══════════════════════════════════════════════════════╗")
    print("║   REV Through Bore Encoder — Diagnostics            ║")
    print("╚══════════════════════════════════════════════════════╝\n")

    print(f"  Pin factory: {encoder.pin_factory.__class__.__name__}")
    print(f"  Index pin level: {index_btn.value}    Abs pin level: {abs_input.value}")

    print("\n  Watching for signal activity (3 seconds — rotate the shaft)...")

    start_steps = encoder.steps
    start_index = index_count
    start_abs_edges = abs_edge_count
    time.sleep(3)
    end_steps = encoder.steps
    end_index = index_count
    end_abs_edges = abs_edge_count

    quad_moved = abs(end_steps - start_steps)
    idx_pulses = end_index - start_index
    abs_edges  = end_abs_edges - start_abs_edges

    print(f"  Quadrature steps:  {quad_moved}")
    print(f"  Index pulses:      {idx_pulses}")
    print(f"  Abs signal edges:  {abs_edges}")

    ok = True
    if quad_moved == 0:
        print(f"\n  ⚠  No quadrature movement detected!")
        print(f"     → Check wiring to GPIO {PIN_A} (A) and GPIO {PIN_B} (B)")
        print(f"     → Confirm encoder is powered (3.3V on VCC, GND connected)")
        ok = False

    if abs_edges == 0:
        print(f"\n  ⚠  No absolute duty-cycle edges detected!")
        print(f"     → Check wiring to GPIO {PIN_ABS}")
        print(f"     → The abs signal should toggle even with no rotation (~976 Hz)")
        ok = False
    else:
        approx_freq = abs_edges / 2 / 3  # edges/2 = cycles, over 3 sec
        print(f"\n  Absolute signal frequency: ~{approx_freq:.0f} Hz  (expected ~976 Hz)")
        if approx_freq < 500 or approx_freq > 1500:
            print("  ⚠  Frequency outside expected range — check wiring or power")
            ok = False

    if ok:
        print("\n  ✓ All signals look good!\n")
    else:
        print("\n  ✗ Some signals missing — check wiring before continuing.\n")
    return ok

# ─── Main test loop ─────────────────────────────────────────────────────────

def run_live_test(encoder):
    """Continuously display encoder readings."""
    global running

    print("\n╔══════════════════════════════════════════════════════╗")
    print("║   Live Encoder Test  (Ctrl+C to stop)               ║")
    print("╚══════════════════════════════════════════════════════╝\n")
    print("  Rotate the encoder shaft and watch the values update.\n")

    try:
        while running:
            steps    = encoder.steps
            rpm      = compute_velocity_rpm(encoder)
            quad_deg = steps_to_degrees(steps)
            quad_rev = steps_to_revolutions(steps)

            line = (
                f"\r  Quad: {steps:>8d} steps | "
                f"{quad_deg:>7.2f}° | "
                f"{quad_rev:>+9.3f} rev | "
                f"{rpm:>+8.1f} RPM | "
                f"Abs: {abs_angle_deg:>6.1f}° ({abs_high_us:>5d} µs) | "
                f"Idx: {index_count}"
            )
            sys.stdout.write(line)
            sys.stdout.flush()
            time.sleep(0.05)
    except KeyboardInterrupt:
        pass

    print("\n\n  Test stopped.")

def run_agreement_test(encoder):
    """Compare quadrature and absolute readings over slow rotation."""
    global running

    print("\n╔══════════════════════════════════════════════════════╗")
    print("║   Quadrature vs Absolute Agreement Test             ║")
    print("╚══════════════════════════════════════════════════════╝\n")
    print("  Slowly rotate the shaft one full turn.")
    print("  This checks that both outputs track the same angle.\n")

    samples = []
    start = time.monotonic()
    duration = 10  # seconds

    print(f"  Sampling for {duration} seconds...\n")
    running = True
    try:
        while running and (time.monotonic() - start) < duration:
            q_deg = steps_to_degrees(encoder.steps)
            a_deg = abs_angle_deg
            samples.append((q_deg, a_deg))
            time.sleep(0.05)
    except KeyboardInterrupt:
        pass

    if len(samples) < 10:
        print("  Not enough samples collected.\n")
        return

    # Compute angular difference (handle wraparound)
    diffs = []
    for q, a in samples:
        diff = q - a
        # Normalize to -180..+180
        diff = (diff + 180) % 360 - 180
        diffs.append(abs(diff))

    avg_err = sum(diffs) / len(diffs)
    max_err = max(diffs)

    print(f"  Samples collected: {len(samples)}")
    print(f"  Avg |quad - abs| error:  {avg_err:.2f}°")
    print(f"  Max |quad - abs| error:  {max_err:.2f}°")

    if max_err < 5.0:
        print("  ✓ Good agreement — both outputs are consistent.\n")
    elif max_err < 15.0:
        print("  ~ Moderate disagreement — check magnet alignment.\n")
    else:
        print("  ✗ Large disagreement — possible wiring issue or magnet problem.\n")

# ─── Entry point ────────────────────────────────────────────────────────────

def cleanup(*args):
    global running
    running = False

def main():
    global running

    signal.signal(signal.SIGINT,  cleanup)
    signal.signal(signal.SIGTERM, cleanup)

    print("\n  Initialising gpiozero devices...\n")

    # ── Quadrature encoder ──
    # max_steps=0 disables the step limit so it counts freely (needed for odometry).
    encoder = RotaryEncoder(
        a=PIN_A,
        b=PIN_B,
        max_steps=0,
        bounce_time=None,   # disable software debounce for speed
    )

    # ── Index pulse ──
    index_btn = Button(PIN_INDEX, pull_up=False, bounce_time=0.001)
    index_btn.when_pressed = _on_index_rise

    # ── Absolute duty-cycle input ──
    abs_input = DigitalInputDevice(PIN_ABS, pull_up=False, bounce_time=None)
    abs_input.when_activated   = _on_abs_rise
    abs_input.when_deactivated = _on_abs_fall

    print(f"  Encoder on GPIO {PIN_A} (A), {PIN_B} (B)")
    print(f"  Index  on GPIO {PIN_INDEX}")
    print(f"  Abs    on GPIO {PIN_ABS}")

    # ── Step 1: Diagnostics ──
    run_diagnostics(encoder, index_btn, abs_input)

    # ── Step 2: Live test ──
    running = True
    run_live_test(encoder)

    # ── Step 3: Agreement test ──
    running = True
    run_agreement_test(encoder)

    # ── Cleanup ──
    encoder.close()
    index_btn.close()
    abs_input.close()
    print("  Done. GPIO cleaned up.\n")

if __name__ == "__main__":
    main()