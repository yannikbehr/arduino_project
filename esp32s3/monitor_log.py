#!/usr/bin/env python3
"""Capture serial output from the device for a fixed duration, then exit.

Handles deep-sleep disconnect/reconnect cycles — the ESP32-S3 USB-Serial/JTAG
port disappears during sleep and reappears on wake.
"""
import serial
import sys
import time
import glob

BAUD = 115200
DURATION = int(sys.argv[1]) if len(sys.argv) > 1 else 90


def find_port(timeout=15):
    deadline = time.time() + timeout
    while time.time() < deadline:
        candidates = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/cu.SLAB*")
        if candidates:
            return candidates[0]
        time.sleep(0.2)
    raise RuntimeError("No USB serial port found after waiting")


deadline = time.time() + DURATION

port = find_port()
print(f"--- Capturing {DURATION}s from {port} at {BAUD} baud ---", flush=True)

while time.time() < deadline:
    try:
        with serial.Serial(port, BAUD, timeout=0.1) as s:
            while time.time() < deadline:
                data = s.read(4096)
                if data:
                    sys.stdout.buffer.write(data)
                    sys.stdout.flush()
    except serial.SerialException:
        # Port disappeared (device entered deep sleep) — wait for it to come back
        sys.stdout.flush()
        try:
            port = find_port(timeout=30)
        except RuntimeError:
            break  # device didn't wake within 30s, give up
