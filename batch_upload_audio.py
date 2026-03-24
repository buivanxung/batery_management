#!/usr/bin/env python3
"""
Batch Audio Upload Script for Battery Management System

This script automatically uploads all PCM files from a directory to the
STM32 MCU's flash memory via UART. It calls upload_audio.py for each file.

Usage:
    python batch_upload_audio.py --dir <directory_with_pcm_files>

Example:
    python batch_upload_audio.py --dir ./audio/audio8

Requirements:
    - upload_audio.py in the same directory
    - STM32 connected via UART
    - PCM files in the specified directory
"""

import os
import sys
import argparse
import subprocess
from pathlib import Path

def upload_file(pcm_file):
    """
    Upload a single PCM file using upload_audio.py

    Args:
        pcm_file (str): Path to PCM file

    Returns:
        bool: True if successful, False otherwise
    """
    try:
        print(f"Uploading: {pcm_file}")
        result = subprocess.run(
            [sys.executable, 'upload_audio.py', pcm_file],
            capture_output=True,
            text=True,
            timeout=30  # 30 second timeout per file
        )

        if result.returncode == 0:
            print(f"✓ Success: {pcm_file}")
            return True
        else:
            print(f"✗ Failed: {pcm_file}")
            print(f"Error: {result.stderr}")
            return False

    except subprocess.TimeoutExpired:
        print(f"✗ Timeout: {pcm_file}")
        return False
    except Exception as e:
        print(f"✗ Error uploading {pcm_file}: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Batch upload PCM files to STM32 flash')
    parser.add_argument('--dir', required=True, help='Directory containing PCM files')

    args = parser.parse_args()

    pcm_dir = Path(args.dir)

    if not pcm_dir.exists():
        print(f"Error: Directory '{pcm_dir}' does not exist")
        sys.exit(1)

    if not pcm_dir.is_dir():
        print(f"Error: '{pcm_dir}' is not a directory")
        sys.exit(1)

    # Find all PCM files
    pcm_files = list(pcm_dir.glob('*.pcm'))
    pcm_files.extend(list(pcm_dir.glob('*.PCM')))  # Also check uppercase

    if not pcm_files:
        print(f"No PCM files found in '{pcm_dir}'")
        sys.exit(1)

    print(f"Found {len(pcm_files)} PCM file(s) to upload")
    print("Make sure your STM32 is connected and ready to receive uploads.")
    print("Starting batch upload...\n")

    success_count = 0
    for pcm_file in sorted(pcm_files):  # Sort for consistent order
        if upload_file(str(pcm_file)):
            success_count += 1

    print(f"\nBatch upload complete: {success_count}/{len(pcm_files)} files uploaded successfully")

    if success_count == len(pcm_files):
        print("All files uploaded successfully!")
        print("Use 'list' command in UART CLI to verify files in flash.")
    else:
        print(f"{len(pcm_files) - success_count} files failed to upload.")
        print("Check UART connection and try again.")

if __name__ == '__main__':
    main()