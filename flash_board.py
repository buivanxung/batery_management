#!/usr/bin/env python3
"""
flash_board.py - Flash firmware + upload audio to STM32G070

Steps:
  1. Build + upload firmware (ST-Link) for selected motor config
  2. Wait for board to boot
  3. Upload all PCM audio files via UART

Usage:
    python3 flash_board.py --motors 6 --port /dev/ttyUSB0
    python3 flash_board.py --motors 8 --port /dev/ttyACM0
    python3 flash_board.py --motors 6 --port /dev/ttyUSB0 --audio-dir ./out
    python3 flash_board.py --motors 6 --port /dev/ttyUSB0 --skip-firmware
    python3 flash_board.py --motors 6 --port /dev/ttyUSB0 --skip-audio

Requirements:
    - platformio installed (pio command)
    - ST-Link connected
    - UART connected to PA9/PA10 (Serial1) at 115200
    - PCM files in ./out (or specify --audio-dir)
"""

import os
import sys
import time
import argparse
import subprocess
from pathlib import Path

# ===== CONFIG =====
UART_BAUD    = 115200
UART_TIMEOUT = 2         # seconds read timeout
BOOT_WAIT    = 3         # seconds to wait after firmware upload
AUDIO_DIR    = './out'   # default PCM output directory

SOF    = 0xAA
CHUNK  = 256
ACK    = b'\x06'
NACK   = b'\x15'

# ===== CRC16 =====
def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc


# ===== STEP 1: Build + Upload firmware =====
def flash_firmware(motors: int) -> bool:
    env = f"nucleo_g070rb_{motors}motor"
    print(f"\n{'='*60}")
    print(f"[1/2] Building + uploading firmware: {env}")
    print(f"{'='*60}")

    cmd = ['platformio', 'run', '-e', env, '--target', 'upload']
    result = subprocess.run(cmd, cwd=str(Path(__file__).parent))

    if result.returncode != 0:
        print(f"\n✗ Firmware upload FAILED (env: {env})")
        return False

    print(f"\n✓ Firmware uploaded OK ({motors} motors)")
    print(f"  Waiting {BOOT_WAIT}s for board to boot...")
    time.sleep(BOOT_WAIT)
    return True


# ===== STEP 2: Upload audio via UART =====
def wait_text(ser, keyword: str, timeout: float = 10.0) -> bool:
    """Read lines until keyword found or timeout."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            line = ser.readline().decode(errors='ignore').strip()
        except Exception:
            break
        if line:
            print(f"  MCU: {line}")
        if keyword in line:
            return True
    return False


def upload_one(ser, name: str, data: bytes) -> bool:
    """Upload a single PCM file to flash via UART protocol."""
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    # Send STORE command
    cmd = f"store {name} {len(data)}\n"
    ser.write(cmd.encode())
    print(f"  → {cmd.strip()}")

    if not wait_text(ser, "READY", timeout=5):
        print("  ✗ MCU did not respond READY")
        return False

    # Send binary frames
    seq    = 0
    offset = 0
    retries = 0

    while offset < len(data):
        chunk  = data[offset:offset + CHUNK]
        length = len(chunk)
        crc    = crc16(chunk)

        frame = bytes([SOF, seq, length & 0xFF, (length >> 8) & 0xFF]) \
              + chunk \
              + crc.to_bytes(2, 'little')

        while True:
            ser.write(frame)
            ack = ser.read(1)

            if ack == ACK:
                break
            elif ack == NACK:
                print(f"    NACK seq={seq}, retrying...")
                retries += 1
            else:
                print(f"    Timeout seq={seq}, retrying...")
                retries += 1

            if retries > 5:
                print("  ✗ Too many retries, aborting")
                return False

        offset += length
        seq = (seq + 1) & 0xFF

    # Wait for DONE
    if not wait_text(ser, "DONE", timeout=10):
        print("  ✗ MCU did not respond DONE")
        return False

    return True


def upload_audio(port: str, audio_dir: str) -> bool:
    try:
        import serial
    except ImportError:
        print("✗ pyserial not installed. Run: pip3 install pyserial")
        return False

    pcm_dir = Path(audio_dir)
    if not pcm_dir.exists():
        print(f"✗ Audio directory not found: {pcm_dir}")
        return False

    pcm_files = sorted(pcm_dir.glob('*.pcm')) + sorted(pcm_dir.glob('*.PCM'))
    pcm_files = list(dict.fromkeys(pcm_files))  # deduplicate

    if not pcm_files:
        print(f"✗ No PCM files in {pcm_dir}")
        return False

    print(f"\n{'='*60}")
    print(f"[2/2] Uploading {len(pcm_files)} audio file(s) via UART")
    print(f"      Port: {port} @ {UART_BAUD} baud")
    print(f"{'='*60}")

    try:
        ser = serial.Serial(port, UART_BAUD, timeout=UART_TIMEOUT)
    except Exception as e:
        print(f"✗ Cannot open serial port {port}: {e}")
        return False

    time.sleep(1)  # Let UART settle

    success = 0
    for pcm_file in pcm_files:
        data = pcm_file.read_bytes()
        size_kb = len(data) / 1024
        duration = len(data) / 8000
        print(f"\n  [{success+1}/{len(pcm_files)}] {pcm_file.name} ({size_kb:.1f}KB, {duration:.1f}s)")

        if upload_one(ser, pcm_file.name, data):
            print(f"  ✓ {pcm_file.name} OK")
            success += 1
        else:
            print(f"  ✗ {pcm_file.name} FAILED")

    ser.close()

    print(f"\n{'='*60}")
    print(f"Audio upload: {success}/{len(pcm_files)} files OK")
    print(f"{'='*60}")
    return success == len(pcm_files)


# ===== MAIN =====
def main():
    parser = argparse.ArgumentParser(
        description='Flash STM32G070 firmware + upload audio',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 flash_board.py --motors 6 --port /dev/ttyUSB0
  python3 flash_board.py --motors 8 --port /dev/ttyACM0 --audio-dir ./out
  python3 flash_board.py --motors 6 --port /dev/ttyUSB0 --skip-firmware
  python3 flash_board.py --motors 6 --port /dev/ttyUSB0 --skip-audio
        """
    )
    parser.add_argument('--motors',        type=int, choices=[6, 8], required=True,
                        help='Motor count: 6 or 8')
    parser.add_argument('--port',          type=str, required=True,
                        help='UART port, e.g. /dev/ttyUSB0')
    parser.add_argument('--audio-dir',     type=str, default=AUDIO_DIR,
                        help=f'Directory with PCM files (default: {AUDIO_DIR})')
    parser.add_argument('--skip-firmware', action='store_true',
                        help='Skip firmware flash, only upload audio')
    parser.add_argument('--skip-audio',    action='store_true',
                        help='Skip audio upload, only flash firmware')

    args = parser.parse_args()

    print(f"\nSTM32G070 Flash Tool")
    print(f"  Motors    : {args.motors}")
    print(f"  UART port : {args.port}")
    print(f"  Audio dir : {args.audio_dir}")

    fw_ok    = True
    audio_ok = True

    if not args.skip_firmware:
        fw_ok = flash_firmware(args.motors)
        if not fw_ok:
            print("\n✗ Stopping: firmware upload failed")
            sys.exit(1)

    if not args.skip_audio:
        audio_ok = upload_audio(args.port, args.audio_dir)

    print(f"\n{'='*60}")
    if fw_ok and audio_ok:
        print("✓ All done! Board is ready.")
    else:
        print("✗ Completed with errors. Check output above.")
    print(f"{'='*60}\n")

    sys.exit(0 if (fw_ok and audio_ok) else 1)


if __name__ == '__main__':
    main()
