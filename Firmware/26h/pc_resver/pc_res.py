import argparse
import socket
import struct
import time
from datetime import datetime

import cv2
import numpy as np


DISCOVERY_PORT = 37020
DEFAULT_STREAM_PORT = 9000
RECORD_FPS = 12.0

MAGIC = b"MCJ1"
HEADER = struct.Struct("!4sIHH")
MAX_FRAME_SIZE = 2 * 1024 * 1024


def recv_exact(sock, size):
    data = bytearray()

    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("stream closed")
        data.extend(chunk)

    return bytes(data)


def discover_sender():
    discovery = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    discovery.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    discovery.bind(("0.0.0.0", DISCOVERY_PORT))
    discovery.settimeout(2)

    print("Waiting for MaixCAM discovery...")

    try:
        while True:
            try:
                data, address = discovery.recvfrom(256)
            except socket.timeout:
                print("Still waiting; check WiFi and Windows firewall")
                continue

            try:
                parts = data.decode("ascii").strip().split("|")

                if len(parts) != 5 or parts[0] != "MCJ1":
                    continue

                port = int(parts[2])

            except (UnicodeError, ValueError):
                continue

            print("Found MaixCAM:", address[0], port)
            return address[0], port

    finally:
        discovery.close()


def new_file_stem():
    return datetime.now().strftime(
        "maixcam_%Y%m%d_%H%M%S_%f"
    )


def show_stream(host, port):
    stream = socket.create_connection(
        (host, port),
        timeout=5,
    )
    stream.settimeout(5)

    writer = None
    video_path = new_file_stem() + ".mp4"

    print("Connected to MaixCAM:", host, port)
    print("Recording to:", video_path)
    print("Press S to save image, Q or Esc to exit")

    try:
        while True:
            raw_header = recv_exact(
                stream,
                HEADER.size,
            )

            magic, frame_size, width, height = (
                HEADER.unpack(raw_header)
            )

            if magic != MAGIC:
                raise RuntimeError(
                    "invalid stream magic"
                )

            if frame_size <= 0:
                raise RuntimeError(
                    "invalid frame size"
                )

            if frame_size > MAX_FRAME_SIZE:
                raise RuntimeError(
                    "frame is too large"
                )

            if width <= 0 or height <= 0:
                raise RuntimeError(
                    "invalid frame dimensions"
                )

            payload = recv_exact(
                stream,
                frame_size,
            )

            encoded = np.frombuffer(
                payload,
                dtype=np.uint8,
            )

            frame = cv2.imdecode(
                encoded,
                cv2.IMREAD_COLOR,
            )

            if frame is None:
                print("JPEG decode failed")
                continue

            if writer is None:
                frame_height, frame_width = (
                    frame.shape[:2]
                )

                fourcc = cv2.VideoWriter_fourcc(
                    *"mp4v"
                )

                writer = cv2.VideoWriter(
                    video_path,
                    fourcc,
                    RECORD_FPS,
                    (frame_width, frame_height),
                )

                if not writer.isOpened():
                    writer.release()
                    writer = None

                    raise RuntimeError(
                        "cannot create MP4 video file"
                    )

            # 将当前帧写入录像
            writer.write(frame)

            # 显示当前画面
            cv2.imshow(
                "MaixCAM Wireless Video",
                frame,
            )

            key = cv2.waitKey(1) & 0xFF

            # q、Q 或 Esc 退出
            if key in (
                ord("q"),
                ord("Q"),
                27,
            ):
                return False

            # s 或 S 保存当前图片
            if key in (
                ord("s"),
                ord("S"),
            ):
                image_path = (
                    new_file_stem() + ".jpg"
                )

                cv2.imwrite(
                    image_path,
                    frame,
                )

                print(
                    "Image saved:",
                    image_path,
                )

    finally:
        stream.close()

        if writer is not None:
            writer.release()
            print(
                "Video saved:",
                video_path,
            )

        cv2.destroyAllWindows()


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Receive and record "
            "the MaixCAM JPEG stream"
        )
    )

    parser.add_argument(
        "--host",
        help=(
            "MaixCAM WiFi IP; "
            "omit to use UDP discovery"
        ),
    )

    parser.add_argument(
        "--port",
        type=int,
        default=DEFAULT_STREAM_PORT,
    )

    args = parser.parse_args()

    try:
        while True:
            if args.host:
                sender = (
                    args.host,
                    args.port,
                )
            else:
                sender = discover_sender()

            try:
                keep_running = show_stream(
                    sender[0],
                    sender[1],
                )

                if not keep_running:
                    break

            except (
                ConnectionError,
                OSError,
                RuntimeError,
            ) as exc:
                print(
                    "Stream error, retrying:",
                    exc,
                )
                time.sleep(2)

    except KeyboardInterrupt:
        print("Stopped by Ctrl+C")


if __name__ == "__main__":
    main()