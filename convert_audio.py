#!/usr/bin/env python3
"""
Audio Converter Script for STM32G0 Battery Management System

Converts audio files (MP3, WAV, etc.) to 8-bit signed PCM format for flash storage.
Compatible with NS8002 Class D amplifier + STM32G070 audio_dac.cpp

Conversion specs (fixed for STM32G070):
    - Codec: 8-bit signed PCM (pcm_s8)
    - Sample rate: 8000 Hz (matches AUDIO_SAMPLE_RATE in audio_dac.h)
    - Channels: Mono (1)
    - File size: 8000 bytes/second = ~4 seconds per 32KB

Usage:
    python3 convert_audio.py --input_dir ./audio_input --output_dir ./audio_output

Requirements:
    - ffmpeg (install: sudo apt install ffmpeg)
    - Python 3.6+
"""

import os
import sys
import argparse
import subprocess
from pathlib import Path

# Audio specifications for STM32G070 (FIXED)
SAMPLE_RATE = 16000  # Hz - must match AUDIO_SAMPLE_RATE in audio_dac.h
BIT_DEPTH = 8  # bits
CHANNELS = 1  # mono
CODEC = 'pcm_s8'  # 8-bit signed
FORMAT = 's8'

# Flash limits (STM32G070 typical)
MAX_FLASH_SIZE = 32 * 1024  # 32KB is common for W25Q32
MAX_FILE_SIZE = 24 * 1024  # Leave 8KB for filesystem header


def check_ffmpeg():
    """Check if ffmpeg is installed."""
    try:
        subprocess.run(['ffmpeg', '-version'], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def convert_audio_to_pcm(input_file, output_file):
    """
    Convert any audio file to 8-bit PCM for STM32.
    
    Args:
        input_file: Path to input audio file (MP3, WAV, etc.)
        output_file: Path to output PCM file
    
    Returns:
        tuple: (success: bool, duration_seconds: float, file_size: int)
    """
    cmd = [
        'ffmpeg',
        '-y',  # Overwrite without asking
        '-loglevel', 'error',  # Suppress verbose output
        '-i', input_file,
        '-acodec', CODEC,  # 8-bit signed PCM
        '-f', FORMAT,  # raw format
        '-ac', str(CHANNELS),  # mono
        '-ar', str(SAMPLE_RATE),  # 8000 Hz
        output_file
    ]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        
        # Get output file size
        file_size = os.path.getsize(output_file)
        
        # Calculate duration from file size
        # 8-bit samples at 8kHz = 8000 bytes/second
        duration_seconds = file_size / SAMPLE_RATE
        
        return True, duration_seconds, file_size
        
    except subprocess.CalledProcessError as e:
        print(f"  ✗ ffmpeg error: {e.stderr}")
        return False, 0, 0
    except Exception as e:
        print(f"  ✗ Error: {e}")
        return False, 0, 0


def format_filesize(size_bytes):
    """Format bytes to human readable."""
    for unit in ['B', 'KB', 'MB']:
        if size_bytes < 1024:
            return f"{size_bytes:.1f}{unit}"
        size_bytes /= 1024
    return f"{size_bytes:.1f}GB"


def main():
    parser = argparse.ArgumentParser(
        description='Convert audio to STM32G070-compatible PCM format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 convert_audio.py --input_dir ./audio_input --output_dir ./audio_output
  
Audio file size calculator:
  - 4 seconds @ 8kHz = 32,000 bytes (fits in 32KB flash sector)
  - 8 seconds @ 8kHz = 64,000 bytes (requires 2 sectors)
        """
    )
    
    parser.add_argument('--input_dir', required=True, 
                        help='Directory with audio files (MP3, WAV, OGG, etc.)')
    parser.add_argument('--output_dir', required=True,
                        help='Output directory for PCM files')
    parser.add_argument('--ext', default='*.mp3',
                        help='File pattern (default: *.mp3), e.g. *.wav, *.ogg')

    args = parser.parse_args()

    # Verify ffmpeg
    if not check_ffmpeg():
        print("✗ ffmpeg not found. Install with: sudo apt install ffmpeg")
        sys.exit(1)

    input_dir = Path(args.input_dir)
    output_dir = Path(args.output_dir)

    # Check input directory
    if not input_dir.exists():
        print(f"✗ Input directory not found: {input_dir}")
        sys.exit(1)

    # Create output directory
    output_dir.mkdir(parents=True, exist_ok=True)

    # Find audio files (case-insensitive)
    pattern = args.ext.lower()
    audio_files = list(input_dir.glob(pattern))
    audio_files.extend(list(input_dir.glob(pattern.upper())))
    audio_files = list(set(audio_files))  # Remove duplicates

    if not audio_files:
        print(f"✗ No files matching '{pattern}' in {input_dir}")
        sys.exit(1)

    print(f"\n{'='*60}")
    print(f"Audio Converter for STM32G070 (NS8002)")
    print(f"{'='*60}")
    print(f"Input:  {input_dir}")
    print(f"Output: {output_dir}")
    print(f"Format: {BIT_DEPTH}-bit PCM, {SAMPLE_RATE}Hz, {CHANNELS} channel")
    print(f"Max file size: {format_filesize(MAX_FILE_SIZE)} (fits in 24KB)")
    print(f"Found {len(audio_files)} file(s)\n")

    success_count = 0
    failed_count = 0
    total_size = 0

    for audio_file in sorted(audio_files):
        output_file = output_dir / (audio_file.stem + '.pcm')
        print(f"Converting: {audio_file.name}")
        
        success, duration, file_size = convert_audio_to_pcm(str(audio_file), str(output_file))
        
        if success:
            total_size += file_size
            
            # Check file size warning
            if file_size > MAX_FILE_SIZE:
                print(f"  ⚠ Warning: File {format_filesize(file_size)} exceeds 24KB limit!")
                print(f"  ℹ Playback time: {duration:.1f} seconds")
            else:
                print(f"  ✓ {format_filesize(file_size)} ({duration:.1f}s playback)")
            
            success_count += 1
        else:
            failed_count += 1

    print(f"\n{'='*60}")
    print(f"Results: {success_count} succeeded, {failed_count} failed")
    print(f"Total output size: {format_filesize(total_size)}")
    print(f"{'='*60}\n")

    if success_count > 0:
        print(f"✓ PCM files ready in: {output_dir}")
        print(f"✓ Upload with: batch_upload_audio.py")
    else:
        print("✗ No files converted successfully")
        sys.exit(1)


if __name__ == '__main__':
    main()

if __name__ == '__main__':
    main()