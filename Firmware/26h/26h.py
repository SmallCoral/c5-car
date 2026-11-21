from maix import app, camera, err, image, network, time
import json
from pathlib import Path
import socket
import struct
import subprocess


STREAM_PORT = 9000
DISCOVERY_PORT = 37020
DISCOVERY_ADDRESS = "255.255.255.255"

WIDTH = 320
HEIGHT = 240
FPS = 12
JPEG_QUALITY = 70
MAX_CLIENTS = 2

MAGIC = b"MCJ1"
HEADER = struct.Struct("!4sIHH")


def valid_ipv4(ip):
    return bool(
        ip
        and ip != "0.0.0.0"
        and not ip.startswith("127.")
        and not ip.startswith("169.254.")
    )


def wlan_ipv4(wifi):
    """Read wlan0 only, so a USB virtual-network IP is never selected."""
    try:
        output = subprocess.check_output(
            ["ip", "-4", "-o", "addr", "show", "dev", "wlan0"]
        ).decode("utf-8", errors="ignore")
        fields = output.split()
        if "inet" in fields:
            ip = fields[fields.index("inet") + 1].split("/", 1)[0]
            if valid_ipv4(ip):
                return ip
    except Exception:
        pass

    try:
        ip = wifi.get_ip()
        if valid_ipv4(ip):
            return ip
    except Exception:
        pass

    return ""


def ensure_wifi():
    # Keep this object alive for the entire application lifetime.
    wifi = network.wifi.Wifi()
    ip = wlan_ipv4(wifi)
    if ip:
        print("WiFi already ready, wlan0 IP:", ip)
        return wifi, ip

    with Path(__file__).with_suffix(".wifi.json").open(encoding="utf-8") as stream:
        config = json.load(stream)
    ssid, password = config["ssid"], config["password"]

    print("Connecting to WiFi:", ssid)
    try:
        result = wifi.connect(ssid, password, wait=False, timeout=60)
        err.check_raise(result, "start WiFi connection failed")
    except Exception as exc:
        # Settings may already have started an asynchronous connection.
        print("WiFi connect request:", exc)

    for second in range(60):
        ip = wlan_ipv4(wifi)
        if ip:
            print("WiFi ready, wlan0 IP:", ip)
            return wifi, ip
        print("Waiting for WiFi:", second + 1)
        time.sleep(1)

    raise RuntimeError("MaixCAM2 did not obtain a wlan0 IPv4 address")


def make_discovery_socket(local_ip):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.bind((local_ip, 0))
    return sock


def announce(sock, local_ip):
    message = "MCJ1|{}|{}|{}|{}".format(
        local_ip,
        STREAM_PORT,
        WIDTH,
        HEIGHT,
    )
    sock.sendto(
        message.encode("ascii"),
        (DISCOVERY_ADDRESS, DISCOVERY_PORT),
    )


def accept_clients(server, clients):
    while len(clients) < MAX_CLIENTS:
        try:
            conn, address = server.accept()
        except BlockingIOError:
            break
        except OSError:
            break

        conn.settimeout(0.5)
        clients.append(conn)
        print("Client connected:", address, "total:", len(clients))


def send_frame(clients, header, payload):
    failed = []

    for conn in clients:
        try:
            conn.sendall(header)
            conn.sendall(payload)
        except OSError as exc:
            print("Client disconnected:", exc)
            failed.append(conn)

    for conn in failed:
        try:
            conn.close()
        except OSError:
            pass
        clients.remove(conn)


def main():
    wifi, local_ip = ensure_wifi()

    discovery = make_discovery_socket(local_ip)

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((local_ip, STREAM_PORT))
    server.listen(MAX_CLIENTS)
    server.setblocking(False)

    cam = camera.Camera(WIDTH, HEIGHT, image.Format.FMT_RGB888)
    cam.skip_frames(10)

    clients = []
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

            accept_clients(server, clients)

            if not clients:
                time.sleep_ms(20)
                continue

            frame = cam.read()
            jpeg = frame.to_jpeg(quality=JPEG_QUALITY)
            payload = jpeg.to_bytes()
            header = HEADER.pack(MAGIC, len(payload), WIDTH, HEIGHT)
            send_frame(clients, header, payload)

            elapsed = time.ticks_ms() - loop_start
            remaining = frame_interval_ms - elapsed
            if remaining > 0:
                time.sleep_ms(remaining)
    finally:
        for conn in clients:
            try:
                conn.close()
            except OSError:
                pass
        server.close()
        discovery.close()


if __name__ == "__main__":
    main()
