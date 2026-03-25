import serial
import sys
import time

SOF = 0xAA
CHUNK = 256

ACK = b'\x06'
NACK = b'\x15'


def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc


def wait_text(ser, keyword):
    """Chỉ dùng cho giai đoạn command (text mode)"""
    while True:
        line = ser.readline().decode(errors="ignore").strip()
        if line:
            print("MCU:", line)
        if keyword in line:
            break


def main():
    port = sys.argv[1]
    name = sys.argv[2]
    path = sys.argv[3]

    data = open(path, "rb").read()

    ser = serial.Serial(port, 115200, timeout=1)
    time.sleep(2)

    ser.reset_input_buffer()
    ser.reset_output_buffer()

    # ===== TEXT MODE =====
    ser.write(f"store {name} {len(data)}\n".encode())

    wait_text(ser, "READY")

    print("Start sending...")

    # ===== BINARY MODE =====
    seq = 0
    offset = 0

    while offset < len(data):
        chunk = data[offset:offset + CHUNK]
        length = len(chunk)

        crc = crc16(chunk)

        frame = bytes([
            SOF,
            seq,
            length & 0xFF,
            (length >> 8) & 0xFF
        ]) + chunk + crc.to_bytes(2, 'little')

        while True:
            ser.write(frame)

            ack = ser.read(1)

            if ack == ACK:
                print(f"ACK {seq}")
                break

            elif ack == NACK:
                print(f"NACK {seq} → resend")

            else:
                print("Timeout / Unknown → resend")

        offset += length
        seq += 1

    # ===== DONE =====
    print("Waiting DONE...")
    wait_text(ser, "DONE")

    print("UPLOAD SUCCESS")


if __name__ == "__main__":
    main()