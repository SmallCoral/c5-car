# 相机端的网络预览入口。读取本地 Wi-Fi 设置，在局域网公布地址后逐帧发送视频。

from maix import app, camera, err, image, network, time
import json
from pathlib import Path
import socket
import struct


STREAM_PORT = 9000
DISCOVERY_PORT = 37020
DISCOVERY_ADDRESS = "255.255.255.255"

WIDTH = 320
HEIGHT = 240
FPS = 20
JPEG_QUALITY = 70

MAGIC = b"MCJ1"
HEADER = struct.Struct("!4sIHH")


# 优先复用设备已有的 Wi-Fi 地址，必要时读取本地热点配置。
def ensure_wifi():
    # 在整个应用运行期间保留无线接口对象。
    wifi = network.wifi.Wifi()
    ip = wifi.get_ip()

    if ip:
        print("WiFi already ready, IP:", ip)
        return wifi, ip

    with Path(__file__).with_suffix(".wifi.json").open(encoding="utf-8") as stream:
        config = json.load(stream)
    ssid, password = config["ssid"], config["password"]

    if wifi.is_connected():
        try:
            err.check_raise(wifi.disconnect(), "disconnect old WiFi failed")
            time.sleep(1)
        except Exception as exc:
            print("Disconnect old WiFi:", exc)

    print("Connecting to Windows hotspot:", ssid)
    result = wifi.connect(ssid, password, wait=True, timeout=60)
    err.check_raise(result, "connect Windows hotspot failed")

    ip = wifi.get_ip()
    if not ip:
        raise RuntimeError("WiFi connected but no IPv4 address")

    print("WiFi ready, IP:", ip)
    return wifi, ip


# 建立用于局域网发现的广播套接字。
def make_discovery_socket(local_ip):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.bind((local_ip, 0))
    return sock


# 广播可供电脑连接的视频地址。
def announce(discovery, local_ip):
    message = "MCJ1|{}|{}|{}|{}".format(
        local_ip,
        STREAM_PORT,
        WIDTH,
        HEIGHT,
    )
    discovery.sendto(
        message.encode("ascii"),
        (DISCOVERY_ADDRESS, DISCOVERY_PORT),
    )


# 等待并接入电脑端图传连接。
def accept_client(server):
    try:
        conn, address = server.accept()
    except BlockingIOError:
        return None
    except OSError:
        return None

    conn.settimeout(0.5)
    print("PC connected:", address)
    return conn


# 封装并送出一帧图像的头部和像素数据。
def send_frame(conn, header, payload):
    try:
        conn.sendall(header)
        conn.sendall(payload)
        return True
    except OSError as exc:
        print("PC disconnected:", exc)
        try:
            conn.close()
        except OSError:
            pass
        return False


# 初始化相机与网络服务，向已连接电脑持续发送图像。
def main():
    wifi, local_ip = ensure_wifi()

    discovery = make_discovery_socket(local_ip)

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((local_ip, STREAM_PORT))
    server.listen(1)
    server.setblocking(False)

    cam = camera.Camera(WIDTH, HEIGHT, image.Format.FMT_RGB888)
    cam.skip_frames(10)

    client = None
    last_announce = 0
    frame_interval_ms = max(1, 1000 // FPS)

    print("JPEG stream server: {}:{}".format(local_ip, STREAM_PORT))
    print("Resolution: {}x{} @ {} FPS".format(WIDTH, HEIGHT, FPS))

    try:
        while not app.need_exit():
            loop_start = time.ticks_ms()

            if loop_start - last_announce >= 1000:
                try:
                    announce(discovery, local_ip)
                except OSError as exc:
                    print("Discovery broadcast failed:", exc)
                last_announce = loop_start

            if client is None:
                client = accept_client(server)
                time.sleep_ms(20)
                continue

            frame = cam.read()
            jpeg = frame.to_jpeg(quality=JPEG_QUALITY)
            payload = jpeg.to_bytes()
            header = HEADER.pack(MAGIC, len(payload), WIDTH, HEIGHT)

            if not send_frame(client, header, payload):
                client = None
                continue

            elapsed = time.ticks_ms() - loop_start
            remaining = frame_interval_ms - elapsed
            if remaining > 0:
                time.sleep_ms(remaining)
    finally:
        if client is not None:
            try:
                client.close()
            except OSError:
                pass
        server.close()
        discovery.close()


if __name__ == "__main__":
    main()
