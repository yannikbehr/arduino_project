#!/usr/bin/env python3
"""Capture serial output from the device for a fixed duration, then exit."""
import serial
import sys
import time
import glob

PORT = None
BAUD = 115200
DURATION = int(sys.argv[1]) if len(sys.argv) > 1 else 90


def find_port():
    candidates = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/cu.SLAB*")
    if not candidates:
        raise RuntimeError("No USB serial port found")
    return candidates[0]


port = find_port()
print(f"--- Capturing {DURATION}s from {port} at {BAUD} baud ---", flush=True)

with serial.Serial(port, BAUD, timeout=0.1) as s:
    deadline = time.time() + DURATION
    while time.time() < deadline:
        data = s.read(4096)
        if data:
            sys.stdout.buffer.write(data)
            sys.stdout.flush()
