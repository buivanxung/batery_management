#!/usr/bin/env python3
"""Upload a raw 8-bit PCM file to the MCU over Serial1 using the STORE protocol.

Usage:
  python3 upload_audio.py /dev/ttyUSB0 beep beep.raw

This script sends the command:
  STORE <name> <length>\n
then sends the raw bytes immediately after.

The MCU must be running the firmware with the "STORE" handler (as in src/main.cpp).
"""

import sys
import serial


def main():
    if len(sys.argv) != 4:
        print("Usage: python3 upload_audio.py <serial-port> <name> <file>")
        sys.exit(1)

    port = sys.argv[1]
    name = sys.argv[2]
    path = sys.argv[3]

    data = open(path, "rb").read()
    length = len(data)

    ser = serial.Serial(port, 115200, timeout=1)

    cmd = f"STORE {name} {length}\n"
    ser.write(cmd.encode())

    # Read the response line that asks for the data.
    line = ser.readline().decode(errors="ignore").strip()
    print(f"MCU: {line}")

    if not line.startswith("Send"):
        print("Unexpected response, aborting")
        ser.close()
        sys.exit(1)

    ser.write(data)

    # Read final status.
    line = ser.readline().decode(errors="ignore").strip()
    print(f"MCU: {line}")

    ser.close()


if __name__ == "__main__":
    main()
