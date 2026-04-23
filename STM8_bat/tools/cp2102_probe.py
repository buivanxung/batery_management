import argparse
import time
from typing import Optional

import serial


SOF = 0xA5
CMD_GET_STATUS = 0x01
CMD_SET_PW_KEY = 0x02
CMD_SET_PW_TIMER = 0x03
CMD_GET_DATA = 0x04
CMD_GET_DIAG = 0x05
CMD_ACK_MASK = 0x80
CMD_NACK = 0x7F

ERR_MAP = {
    0x01: "BAD_CMD",
    0x02: "BAD_LEN",
    0x03: "BAD_VALUE",
    0x04: "LOCKED",
    0x05: "BAD_CRC",
}

CMD_NAME_TO_ID = {
    "GET_STATUS": CMD_GET_STATUS,
    "SET_PW_KEY": CMD_SET_PW_KEY,
    "SET_PW_TIMER": CMD_SET_PW_TIMER,
    "GET_DATA": CMD_GET_DATA,
    "GET_DIAG": CMD_GET_DIAG,
}

CMD_ID_TO_NAME = {v: k for k, v in CMD_NAME_TO_ID.items()}


def frame_crc(frame_wo_crc: bytes) -> int:
    crc = 0
    for b in frame_wo_crc:
        crc ^= b
    return crc


def build_frame(cmd: int, payload: bytes = b"") -> bytes:
    header = bytes([SOF, cmd, len(payload)]) + payload
    return header + bytes([frame_crc(header)])


def build_request(cmd_name: str, pw: int, minutes: int) -> tuple[int, bytes]:
    cmd_name = cmd_name.upper()
    if cmd_name not in CMD_NAME_TO_ID:
        raise ValueError(f"Unsupported cmd: {cmd_name}")

    cmd_id = CMD_NAME_TO_ID[cmd_name]
    payload = b""

    if cmd_id == CMD_SET_PW_KEY:
        if pw not in (0, 1):
            raise ValueError("--pw must be 0 or 1 for SET_PW_KEY")
        payload = bytes([pw])

    if cmd_id == CMD_SET_PW_TIMER:
        if minutes < 0 or minutes > 0xFFFF:
            raise ValueError("--minutes must be in range 0..65535")
        payload = bytes([(minutes >> 8) & 0xFF, minutes & 0xFF])

    return cmd_id, build_frame(cmd_id, payload)


def parse_frames(raw: bytes) -> list[bytes]:
    frames: list[bytes] = []
    i = 0
    n = len(raw)

    while i < n:
        if raw[i] != SOF:
            i += 1
            continue

        if i + 3 > n:
            break

        length = raw[i + 2]
        frame_len = 4 + length
        end = i + frame_len
        if end > n:
            break

        frame = raw[i:end]
        frames.append(frame)
        i = end

    return frames


def frame_summary(frame: bytes) -> str:
    if len(frame) < 4:
        return "short-frame"

    cmd = frame[1]
    length = frame[2]
    payload = frame[3:-1]
    crc = frame[-1]
    calc = frame_crc(frame[:-1])
    crc_valid = crc == calc

    if cmd == 0x06 and length == 1:
        return f"HEARTBEAT seq={payload[0]} crc_ok={crc_valid}"

    if cmd == (CMD_GET_STATUS | CMD_ACK_MASK) and length == 4:
        pw_locked = payload[0]
        real_on = payload[1]
        bat_mv = (payload[2] << 8) | payload[3]
        return (
            f"GET_STATUS ack pw_locked={pw_locked} real_on={real_on} "
            f"bat_mv={bat_mv} crc_ok={crc_valid}"
        )

    if cmd == (CMD_GET_DATA | CMD_ACK_MASK) and length == 3:
        bat_mv = (payload[0] << 8) | payload[1]
        return f"GET_DATA ack bat_mv={bat_mv} locked={payload[2]} crc_ok={crc_valid}"

    if cmd == (CMD_GET_DIAG | CMD_ACK_MASK) and length == 8:
        fields = [
            "rx_ok",
            "crc_err",
            "timeout_err",
            "bad_len_err",
            "bad_cmd_err",
            "framing_err",
            "false_start_err",
            "last_cmd",
        ]
        diag = ", ".join(f"{k}={v}" for k, v in zip(fields, payload))
        return f"GET_DIAG ack {diag} crc_ok={crc_valid}"

    if cmd == CMD_NACK and length == 2:
        req = payload[0]
        err = payload[1]
        err_name = ERR_MAP.get(err, "UNKNOWN")
        req_name = CMD_ID_TO_NAME.get(req, "UNKNOWN")
        return f"NACK req_cmd=0x{req:02X}({req_name}) err=0x{err:02X}({err_name}) crc_ok={crc_valid}"

    cmd_base = cmd & 0x7F
    cmd_name = CMD_ID_TO_NAME.get(cmd_base, f"0x{cmd_base:02X}")
    if cmd & CMD_ACK_MASK:
        return f"{cmd_name} ack len={length} crc_ok={crc_valid}"
    return f"cmd=0x{cmd:02X}({cmd_name}) len={length} crc_ok={crc_valid}"


def crc_ok(frame: bytes) -> bool:
    return len(frame) >= 4 and frame[-1] == frame_crc(frame[:-1])


def pick_response(frames: list, req_cmd_id: int) -> Optional[bytes]:
    expected_ack = req_cmd_id | CMD_ACK_MASK
    valid = [f for f in frames if crc_ok(f)]

    for frame in valid:
        if frame[1] == expected_ack:
            return frame

    for frame in valid:
        if frame[1] == CMD_NACK and len(frame) >= 6 and frame[3] == req_cmd_id:
            return frame

    # Fallback: first valid frame that is not just echoed request cmd.
    for frame in valid:
        if frame[1] != req_cmd_id:
            return frame

    return None


def send_and_receive(
    ser: serial.Serial,
    req_cmd_id: int,
    req_frame: bytes,
    timeout_s: float,
    retries: int,
    tx_delay_s: float = 0.0,
) -> tuple:
    tries = max(1, retries + 1)
    raw = b""
    frames: list[bytes] = []

    def set_probe_low(ser, hold_ms=200):
        # Tạm bỏ để debug - uncomment khi cần
        # ser.dtr = False
        # time.sleep(hold_ms / 1000.0)
        # ser.dtr = True
        pass

    for _ in range(tries):
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        # set_probe_low(ser, 200)  # Tạm bỏ
        ser.write(req_frame)
        ser.flush()

        # Wait for STM8 to process and respond
        # Tăng delay để STM8 có đủ thời gian xử lý
        if tx_delay_s > 0:
            time.sleep(tx_delay_s)
        else:
            time.sleep(0.1)  # Mặc định 100ms để STM8 xử lý và gửi phản hồi

        deadline = time.time() + timeout_s
        chunks: list[bytes] = []
        while time.time() < deadline:
            data = ser.read(64)
            if data:
                chunks.append(data)

        raw = b"".join(chunks)
        frames = parse_frames(raw)
        response = pick_response(frames, req_cmd_id)
        if response is not None:
            return raw, frames, response

    return raw, frames, None


def print_frames(frames: list[bytes]) -> None:
    if not frames:
        print("No frame parsed")
        return

    for idx, frame in enumerate(frames, start=1):
        print(f"FRAME{idx}:", frame.hex(" "), "=>", frame_summary(frame))


def run_listen(ser: serial.Serial, listen_seconds: float) -> None:
    ser.reset_input_buffer()
    end_time = time.time() + listen_seconds
    chunks: list[bytes] = []

    while time.time() < end_time:
        data = ser.read(64)
        if data:
            chunks.append(data)

    raw = b"".join(chunks)
    print("RAW:", raw.hex(" ") if raw else "(none)")
    print_frames(parse_frames(raw))


def run_request(
    ser: serial.Serial,
    cmd: str,
    pw: int,
    minutes: int,
    timeout_s: float,
    retries: int,
    tx_delay_s: float = 0.0,
) -> None:
    req_cmd_id, req = build_request(cmd, pw, minutes)
    print("TX:", req.hex(" "))

    raw, frames, response = send_and_receive(ser, req_cmd_id, req, timeout_s, retries, tx_delay_s)
    print("RAW:", raw.hex(" ") if raw else "(none)")
    print_frames(frames)

    if response is None:
        print("RESULT: timeout/no valid ACK-NACK")
    else:
        print("RESULT:", frame_summary(response))


def run_sweep(ser: serial.Serial, timeout_s: float, retries: int, tx_delay_s: float = 0.0) -> None:
    for name in ["GET_STATUS", "GET_DATA", "GET_DIAG"]:
        print("=" * 62)
        print("CMD:", name)
        run_request(ser, name, pw=0, minutes=0, timeout_s=timeout_s, retries=retries, tx_delay_s=tx_delay_s)


def run_stress(
    ser: serial.Serial,
    cmd: str,
    pw: int,
    minutes: int,
    timeout_s: float,
    retries: int,
    count: int,
    interval_s: float,
    tx_delay_s: float = 0.0,
) -> None:
    req_cmd_id, req = build_request(cmd, pw, minutes)
    ok_count = 0
    nack_count = 0
    timeout_count = 0

    for idx in range(1, count + 1):
        raw, _, response = send_and_receive(ser, req_cmd_id, req, timeout_s, retries, tx_delay_s)
        if response is None:
            timeout_count += 1
            print(f"[{idx}/{count}] TIMEOUT raw={raw.hex(' ') if raw else '(none)'}")
        elif response[1] == CMD_NACK:
            nack_count += 1
            print(f"[{idx}/{count}] NACK {frame_summary(response)}")
        else:
            ok_count += 1
            print(f"[{idx}/{count}] ACK  {frame_summary(response)}")

        if idx < count and interval_s > 0:
            time.sleep(interval_s)

    print("=" * 62)
    print(
        "STATS:",
        f"ok={ok_count}",
        f"nack={nack_count}",
        f"timeout={timeout_count}",
        f"total={count}",
    )


def positive_float(value: str) -> float:
    v = float(value)
    if v <= 0:
        raise argparse.ArgumentTypeError("must be > 0")
    return v


def non_negative_float(value: str) -> float:
    v = float(value)
    if v < 0:
        raise argparse.ArgumentTypeError("must be >= 0")
    return v


def positive_int(value: str) -> int:
    v = int(value)
    if v <= 0:
        raise argparse.ArgumentTypeError("must be > 0")
    return v


def non_negative_int(value: str) -> int:
    v = int(value)
    if v < 0:
        raise argparse.ArgumentTypeError("must be >= 0")
    return v


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Probe/test STM8 SIGNAL_OUT via CP2102")
    parser.add_argument("--port", default="COM6", help="Serial port, default COM6")
    parser.add_argument("--baud", type=positive_int, default=9600, help="Baud rate, default 9600")
    parser.add_argument(
        "--mode",
        default="request",
        choices=["request", "listen", "sweep", "stress"],
        help="request=one command, listen=sniff, sweep=3 read commands, stress=repeat command",
    )
    parser.add_argument(
        "--cmd",
        default="GET_DATA",
        choices=list(CMD_NAME_TO_ID.keys()),
        help="Command for request/stress mode",
    )
    parser.add_argument("--pw", type=int, default=1, help="SET_PW_KEY payload (0/1)")
    parser.add_argument("--minutes", type=non_negative_int, default=1, help="SET_PW_TIMER payload (0..65535)")
    parser.add_argument("--timeout", type=positive_float, default=1.0, help="Read window in seconds, default 1.0 (increased from 0.25)")
    parser.add_argument("--retries", type=non_negative_int, default=1, help="Extra retries when no valid ACK/NACK")
    parser.add_argument("--listen-seconds", type=positive_float, default=4.0, help="Listen window for mode=listen")
    parser.add_argument("--count", type=positive_int, default=30, help="Number of cycles for mode=stress")
    parser.add_argument("--interval", type=non_negative_float, default=0.05, help="Delay between stress cycles")
    parser.add_argument("--tx-delay", type=non_negative_float, default=0.0, help="Delay in seconds after TX before reading response")
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    print("PORT:", args.port)
    print("BAUD:", args.baud)

    with serial.Serial(args.port, args.baud, timeout=0.03) as ser:
        if args.mode == "listen":
            run_listen(ser, args.listen_seconds)
            return

        if args.mode == "sweep":
            run_sweep(ser, args.timeout, args.retries, args.tx_delay)
            return

        if args.mode == "stress":
            run_stress(
                ser,
                args.cmd,
                args.pw,
                args.minutes,
                args.timeout,
                args.retries,
                args.count,
                args.interval,
                args.tx_delay,
            )
            return

        run_request(ser, args.cmd, args.pw, args.minutes, args.timeout, args.retries, args.tx_delay)


if __name__ == "__main__":
    main()
