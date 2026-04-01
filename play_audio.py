#!/usr/bin/env python3
"""
Simple script to play audio on STM32G070 via UART
"""

import sys
import time
import serial

def play_audio(port: str, filename: str):
    """Send PLAY command to board"""
    try:
        ser = serial.Serial(port, 115200, timeout=2)
    except Exception as e:
        print(f"✗ Cannot open port {port}: {e}")
        return False
    
    try:
        time.sleep(0.5)
        
        # Send PLAY command
        cmd = f"play {filename}\n"
        ser.write(cmd.encode())
        ser.flush()
        print(f"→ Sent: {cmd.strip()}")
        
        # Read response
        print("\nMCU responses:")
        deadline = time.time() + 10
        while time.time() < deadline:
            try:
                line = ser.readline().decode(errors='ignore').strip()
                if line:
                    print(f"  {line}")
            except:
                break
        
        return True
    finally:
        ser.close()

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python play_audio.py <filename> [port]")
        print("Example: python play_audio.py khay1.pcm COM6")
        sys.exit(1)
    
    filename = sys.argv[1]
    port = sys.argv[2] if len(sys.argv) > 2 else 'COM6'
    
    print(f"Playing: {filename} on {port}")
    play_audio(port, filename)
