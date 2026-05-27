#!/usr/bin/env python3
"""Lightweight serial monitor that streams device logs to stdout."""
import sys

import serial


def main(port: str = "/dev/cu.usbserial-120", baud: int = 115200) -> int:
    ser = serial.Serial(port, baud, timeout=1)
    print("SERIAL_CONNECTED", flush=True)
    while True:
        line = ser.readline()
        if line:
            sys.stdout.write(line.decode("utf-8", "ignore"))
            sys.stdout.flush()


if __name__ == "__main__":
    sys.exit(main())
