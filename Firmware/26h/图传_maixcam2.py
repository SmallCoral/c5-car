# 图传接收端。可通过局域网广播发现发送器，也可指定地址，按协议头取出每帧图像。

import argparse
import socket
import struct
import time

import cv2
import numpy as np


DISCOVERY_PORT = 37020
DEFAULT_STREAM_PORT = 9000
MAGIC = b"MCJ1"
HEADER = struct.Struct("!4sIHH")
MAX_FRAME_SIZE = 2 * 1024 * 1024


# 按给定长度读完整段 TCP 数据，区分正常帧与断连。
def recv_exact(sock, size):
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("stream closed")
        data.extend(chunk)
    return bytes(data)


# 从 UDP 发现广播取得发送端地址。
def discover_sender():
    discovery = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    discovery.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    discovery.bind(("0.0.0.0", DISCOVERY_PORT))
    discovery.settimeout(2)

    print("Waiting for MaixCAM2 discovery...")
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

            print("Found MaixCAM2:", address[0], port)
            return address[0], port
    finally:
        discovery.close()


# 从 TCP 连接读取完整视频帧并解码显示。
def show_stream(host, port):
    stream = socket.create_connection((host, port), timeout=5)
    stream.settimeout(5)
    print("Connected. Press S to save, Q or Esc to exit.")

    try:
        while True:
            raw_header = recv_exact(stream, HEADER.size)
            magic, frame_size, width, height = HEADER.unpack(raw_header)

            if magic != MAGIC:
                raise RuntimeError("invalid stream magic")
            if frame_size <= 0 or frame_size > MAX_FRAME_SIZE:
                raise RuntimeError("invalid frame size: {}".format(frame_size))
            if width <= 0 or height <= 0:
                raise RuntimeError("invalid frame dimensions")

            payload = recv_exact(stream, frame_size)
            encoded = np.frombuffer(payload, dtype=np.uint8)
            frame = cv2.imdecode(encoded, cv2.IMREAD_COLOR)

            if frame is None:
                print("JPEG decode failed")
                continue

            cv2.imshow("MaixCAM2 Wireless Video", frame)
            key = cv2.waitKey(1) & 0xFF

            if key in (ord("q"), 27):
                return False
            if key == ord("s"):
                filename = time.strftime("maixcam2_%Y%m%d_%H%M%S.jpg")
                cv2.imwrite(filename, frame)
                print("Saved:", filename)
    finally:
        stream.close()
        cv2.destroyAllWindows()


# 解析地址与显示参数，启动发现和视频接收。
def main():
    parser = argparse.ArgumentParser(
        description="Receive the MaixCAM2 framed-JPEG stream"
    )
    parser.add_argument(
        "--host",
        help="MaixCAM2 wlan0 IP; omit to use UDP discovery",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=DEFAULT_STREAM_PORT,
    )
    args = parser.parse_args()

    while True:
        sender = (args.host, args.port) if args.host else discover_sender()

        try:
            keep_running = show_stream(sender[0], sender[1])
            if not keep_running:
                break
        except (ConnectionError, OSError, RuntimeError) as exc:
            print("Stream error, retrying:", exc)
            time.sleep(2)


if __name__ == "__main__":
    main()
