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
import shutil
from pathlib import Path

# ===== CONFIG =====
UART_BAUD    = 115200
# Keep read timeout short so per-frame retries stay within MCU's 10s upload timeout.
UART_TIMEOUT = 0.25      # seconds read timeout
BOOT_WAIT    = 3         # seconds to wait after firmware upload
AUDIO_DIR    = './out'   # default PCM output directory

SOF    = 0xAA
CHUNK  = 256
ACK    = b'\x06'
NACK   = b'\x15'


def find_stm32_programmer_cli() -> str:
    """Locate STM32_Programmer_CLI executable if installed."""
    candidates = [
        os.environ.get('STM32_PROGRAMMER_CLI', ''),
        shutil.which('STM32_Programmer_CLI'),
        shutil.which('STM32_Programmer_CLI.exe'),
        r"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
        r"C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
    ]

    for path in candidates:
        if path and Path(path).exists():
            return str(path)
    return ""


def find_local_cubeprog_installer() -> str:
    """Find STM32CubeProgrammer installer in local ./app folder."""
    app_dir = Path(__file__).parent / 'app'
    if not app_dir.exists():
        return ""

    patterns = [
        'SetupSTM32CubeProgrammer*.exe',
        '*CubeProgrammer*.exe',
    ]
    for pattern in patterns:
        matches = sorted(app_dir.glob(pattern))
        if matches:
            return str(matches[0])
    return ""


def ensure_stm32_programmer_cli() -> str:
    """Ensure STM32_Programmer_CLI exists; try local installer, then winget on Windows."""
    cli = find_stm32_programmer_cli()
    if cli:
        return cli

    if os.name != 'nt':
        return ""

    installer = find_local_cubeprog_installer()
    if installer:
        print("  - STM32CubeProgrammer CLI missing, trying local installer...")
        print(f"    Installer: {installer}")
        install_attempts = [
            [installer, '/S'],
            [installer, '/silent'],
            [installer],
        ]
        for cmd in install_attempts:
            print(f"    -> {' '.join(cmd)}")
            result = subprocess.run(cmd, cwd=str(Path(__file__).parent), text=True)
            if result.returncode == 0:
                cli = find_stm32_programmer_cli()
                if cli:
                    print("  ✓ STM32CubeProgrammer installed from local installer")
                    return cli
    else:
        print("  - No local STM32CubeProgrammer installer found in ./app")

    winget_path = shutil.which('winget')
    if winget_path:
        print("  - Trying to install STM32CubeProgrammer via winget...")
        result = subprocess.run(
            [
                winget_path,
                'install',
                '--id', 'STMicroelectronics.STM32CubeProgrammer',
                '--source', 'winget',
                '--accept-source-agreements',
                '--accept-package-agreements',
                '--silent',
            ],
            text=True
        )
        if result.returncode == 0:
            cli = find_stm32_programmer_cli()
            if cli:
                print("  ✓ STM32CubeProgrammer installed via winget")
                return cli

    return ""


def looks_like_locked_chip(output_text: str) -> bool:
    """Best-effort detection for flash failures caused by readout protection/lock."""
    text = (output_text or "").lower()
    keywords = [
        'read protection',
        'readout protection',
        'rdp',
        'is protected',
        'option bytes',
        'flash loader cannot be loaded',
        'failed to erase memory',
        'memory is not writable',
        'device is locked',
    ]
    return any(k in text for k in keywords)


def try_unlock_chip() -> bool:
    """Attempt to unlock STM32 chip (RDP) using STM32CubeProgrammer CLI."""
    cli = ensure_stm32_programmer_cli()
    if not cli:
        print("✗ Chip appears locked but STM32_Programmer_CLI was not found")
        print("  Install STM32CubeProgrammer to enable auto-unlock")
        print("  https://www.st.com/en/development-tools/stm32cubeprog.html")
        return False

    print("\n[*] Detected possible chip lock. Attempting auto-unlock...")
    unlock_commands = [
        [cli, '-c', 'port=SWD', '-unlockrdp1'],
        [cli, '-c', 'port=SWD', '-ob', 'RDP=0xAA'],
    ]

    for cmd in unlock_commands:
        print(f"  -> {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True)
        combined = (result.stdout or '') + "\n" + (result.stderr or '')
        if combined.strip():
            print(combined.strip())

        if result.returncode == 0:
            print("✓ Unlock command succeeded")
            time.sleep(1.0)
            return True

    print("✗ Auto-unlock failed")
    return False

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


# ===== REQUIREMENTS CHECK =====
def ensure_package(module_name: str, package_name: str = None) -> bool:
    """Ensure a package is installed for the current Python interpreter."""
    package = package_name or module_name

    result = subprocess.run(
        [sys.executable, '-c', f'import {module_name}'],
        capture_output=True,
        text=True
    )
    if result.returncode == 0:
        print(f"  ✓ {package} OK")
        return True

    print(f"  - Missing {package}, installing...")
    install_result = subprocess.run(
        [sys.executable, '-m', 'pip', 'install', '--upgrade', package],
        text=True
    )
    if install_result.returncode != 0:
        print(f"✗ Failed to install {package}")
        return False

    verify_result = subprocess.run(
        [sys.executable, '-c', f'import {module_name}'],
        capture_output=True,
        text=True
    )
    if verify_result.returncode != 0:
        print(f"✗ {package} installed but import still failed")
        return False

    print(f"  ✓ {package} installed")
    return True


def check_requirements() -> bool:
    """Verify all required tools are installed (auto-install if missing)."""
    print("\n[*] Checking requirements...")

    if not ensure_package('platformio'):
        return False

    # Verify PlatformIO command works after installation.
    result = subprocess.run([sys.executable, '-m', 'platformio', '--version'], capture_output=True, text=True)
    if result.returncode != 0:
        print("✗ platformio command is still unavailable")
        return False
    print("  ✓ platformio command OK")

    if not ensure_package('serial', 'pyserial'):
        return False
    
    return True


# ===== STEP 1: Build + Upload firmware =====
def flash_firmware(motors: int) -> bool:
    env = f"nucleo_g070rb_{motors}motor"
    print(f"\n{'='*60}")
    print(f"[1/2] Building + uploading firmware: {env}")
    print(f"{'='*60}")

    cmd = [sys.executable, '-m', 'platformio', 'run', '-e', env, '--target', 'upload']
    result = subprocess.run(cmd, cwd=str(Path(__file__).parent), capture_output=True, text=True)
    combined = (result.stdout or '') + "\n" + (result.stderr or '')
    if combined.strip():
        print(combined)

    if result.returncode != 0:
        print(f"\n✗ Firmware upload FAILED (env: {env})")

        if looks_like_locked_chip(combined):
            if not try_unlock_chip():
                return False

            print("\n[*] Retrying firmware upload after unlock...")
            retry = subprocess.run(cmd, cwd=str(Path(__file__).parent), capture_output=True, text=True)
            retry_output = (retry.stdout or '') + "\n" + (retry.stderr or '')
            if retry_output.strip():
                print(retry_output)
            if retry.returncode != 0:
                print("✗ Upload still failed after unlock attempt")
                return False
        else:
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
    time.sleep(0.2)

    # Send STORE command, retry a few times to recover from a noisy CLI state.
    cmd = f"store {name} {len(data)}\n"
    got_ready = False
    for cmd_try in range(1, 4):
        ser.write(b"\n")
        ser.flush()
        time.sleep(0.05)

        ser.write(cmd.encode())
        ser.flush()
        print(f"  → {cmd.strip()} (try {cmd_try}/3)")

        if wait_text(ser, "READY", timeout=5):
            got_ready = True
            break

    if not got_ready:
        print("  ✗ MCU did not respond READY")
        return False

    # Drain any leftover bytes (e.g. "> " prompt after READY)
    time.sleep(0.3)
    ser.reset_input_buffer()

    # Send binary frames
    seq    = 0
    offset = 0

    while offset < len(data):
        chunk  = data[offset:offset + CHUNK]
        length = len(chunk)
        crc    = crc16(chunk)

        frame = bytes([SOF, seq, length & 0xFF, (length >> 8) & 0xFF]) \
              + chunk \
              + crc.to_bytes(2, 'little')

        # Send frame with retries per frame
        frame_retries = 0
        max_retries = 10

        while frame_retries < max_retries:
            try:
                ser.write(frame)
                ser.flush()
                time.sleep(0.05)  # Give MCU time to process
                
                ack = ser.read(1)

                if ack == ACK:
                    break
                elif ack == NACK:
                    frame_retries += 1
                    print(f"    NACK seq={seq}, retry {frame_retries}/{max_retries}")
                    time.sleep(0.1)
                elif len(ack) == 0:
                    frame_retries += 1
                    print(f"    Timeout seq={seq}, retry {frame_retries}/{max_retries}")
                    time.sleep(0.1)
                else:
                    frame_retries += 1
                    print(f"    Bad response seq={seq}: {ack.hex()}, retry {frame_retries}/{max_retries}")
                    time.sleep(0.1)

            except Exception as e:
                frame_retries += 1
                print(f"    Error seq={seq}: {e}, retry {frame_retries}/{max_retries}")
                time.sleep(0.1)

        if frame_retries >= max_retries:
            print(f"  ✗ Frame seq={seq} failed after {max_retries} retries")
            return False

        offset += length
        seq = (seq + 1) & 0xFF

    # Wait for DONE
    if not wait_text(ser, "DONE", timeout=10):
        print("  ✗ MCU did not respond DONE")
        return False

    time.sleep(0.3)  # Wait after DONE before next file
    return True


def upload_audio(port: str, audio_dir: str) -> bool:
    try:
        import serial
    except ImportError:
        print("✗ pyserial not installed. Run: pip install pyserial")
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

    ser = None
    try:
        ser = serial.Serial(port, UART_BAUD, timeout=UART_TIMEOUT, write_timeout=2)
    except Exception as e:
        print(f"✗ Cannot open serial port {port}: {e}")
        print(f"  Check: Device Manager → Ports (COM & LPT)")
        return False

    try:
        time.sleep(1.0)

        success = 0
        for pcm_file in pcm_files:
            data = pcm_file.read_bytes()
            size_kb = len(data) / 1024
            duration = len(data) / 8000
            print(f"\n  [{success+1}/{len(pcm_files)}] {pcm_file.name} ({size_kb:.1f}KB, {duration:.1f}s)")

            file_ok = False
            for file_try in range(1, 4):
                if upload_one(ser, pcm_file.name, data):
                    file_ok = True
                    break
                print(f"  ! Retry file {pcm_file.name} ({file_try}/3)")
                time.sleep(0.2)

            if file_ok:
                print(f"  ✓ {pcm_file.name} OK")
                success += 1
            else:
                print(f"  ✗ {pcm_file.name} FAILED")

        print(f"\n{'='*60}")
        print(f"Audio upload: {success}/{len(pcm_files)} files OK")
        print(f"{'='*60}")
        return success == len(pcm_files)
    finally:
        if ser:
            try:
                ser.close()
            except Exception:
                pass


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

    # Check requirements first
    if not check_requirements():
        sys.exit(1)

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
