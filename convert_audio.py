#!/usr/bin/env python3
"""
Audio Converter Script for Battery Management System

This script converts MP3 audio files to 8-bit signed PCM format suitable for
flash storage on the STM32 MCU. It uses ffmpeg for conversion.

Usage:
    python convert_audio.py --input_dir <input_directory> --output_dir <output_directory>

Example:
    python convert_audio.py --input_dir ./audio_mp3 --output_dir ./audio_pcm

Requirements:
    - ffmpeg must be installed on your system
    - Input directory should contain MP3 files
    - Output directory will be created if it doesn't exist

Conversion parameters:
    - Audio codec: pcm_s8le (8-bit signed PCM)
    - Channels: 1 (mono)
    - Sample rate: 8000 Hz
"""

import os
import sys
import argparse
import subprocess
from pathlib import Path

def convert_mp3_to_pcm(input_file, output_file):
    """
    Convert a single MP3 file to PCM using ffmpeg.

    Args:
        input_file (str): Path to input MP3 file
        output_file (str): Path to output PCM file
    """
    cmd = [
        'ffmpeg',
        '-y',  # Overwrite output files without asking
        '-i', input_file,  # Input file
        '-acodec', 'pcm_s8',  # Audio codec: 8-bit signed PCM
        '-f', 's8',  # Format: raw 8-bit signed
        '-ac', '1',  # Audio channels: mono
        '-ar', '8000',  # Audio sample rate: 8000 Hz
        output_file  # Output file
    ]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        print(f"Converted: {input_file} -> {output_file}")
    except subprocess.CalledProcessError as e:
        print(f"Error converting {input_file}: {e}")
        print(f"ffmpeg stderr: {e.stderr}")
        return False
    return True

def main():
    parser = argparse.ArgumentParser(description='Convert MP3 files to PCM format for STM32 flash storage')
    parser.add_argument('--input_dir', required=True, help='Directory containing MP3 files')
    parser.add_argument('--output_dir', required=True, help='Directory to save PCM files')

    args = parser.parse_args()

    input_dir = Path(args.input_dir)
    output_dir = Path(args.output_dir)

    if not input_dir.exists():
        print(f"Error: Input directory '{input_dir}' does not exist")
        sys.exit(1)

    # Create output directory if it doesn't exist
    output_dir.mkdir(parents=True, exist_ok=True)

    # Find all MP3 files in input directory
    mp3_files = list(input_dir.glob('*.mp3'))
    mp3_files.extend(list(input_dir.glob('*.MP3')))  # Also check for uppercase extension

    if not mp3_files:
        print(f"No MP3 files found in '{input_dir}'")
        sys.exit(1)

    print(f"Found {len(mp3_files)} MP3 file(s) to convert")

    success_count = 0
    for mp3_file in mp3_files:
        # Generate output filename by replacing extension
        output_file = output_dir / (mp3_file.stem + '.pcm')

        if convert_mp3_to_pcm(str(mp3_file), str(output_file)):
            success_count += 1

    print(f"\nConversion complete: {success_count}/{len(mp3_files)} files converted successfully")

    if success_count == len(mp3_files):
        print(f"PCM files saved to: {output_dir}")
        print("You can now use batch_upload_audio.py to upload these files to the MCU flash.")
    else:
        print("Some files failed to convert. Check the errors above.")

if __name__ == '__main__':
    main()