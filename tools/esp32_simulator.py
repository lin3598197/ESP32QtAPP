#!/usr/bin/env python3
"""
ESP32 Multi-Device UDP Simulator
Simulates multiple ESP32 devices connected to the PC:
- Sends periodic "hello" packets with MAC, ID, and firmware version.
- Listens for "set_key" broadcasts on port 4210 (SO_REUSEADDR).
- Replies with "key_ack" directly to the sender's IP and port.
- Supports simulating timeout (no ACK) or error responses for specific nodes.
"""

import socket
import json
import time
import threading
import argparse
import sys

DEFAULT_PORT = 4210
TARGET_HOST = "127.0.0.1"

class SimulatedEsp32:
    def __init__(self, index: int, target_host: str, target_port: int, behavior: str = "normal"):
        self.index = index
        self.target_host = target_host
        self.target_port = target_port
        self.behavior = behavior  # 'normal', 'timeout', 'error'

        self.mac = f"AA:BB:CC:11:22:{index:02X}"
        self.dev_id = f"ESP32-{index:03d}"
        self.firmware = "1.0.0"
        self.stored_key = ""
        self.last_msg_id = ""
        self.running = False

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    def start(self):
        self.running = True
        self.thread_hello = threading.Thread(target=self._hello_loop, daemon=True)
        self.thread_hello.start()

    def stop(self):
        self.running = False
        try:
            self.sock.close()
        except:
            pass

    def send_hello(self):
        hello_packet = {
            "protocol": "esp32-control",
            "version": 1,
            "type": "hello",
            "mac": self.mac,
            "id": self.dev_id,
            "firmware": self.firmware
        }
        data = json.dumps(hello_packet).encode("utf-8")
        try:
            # Send to localhost and local subnet/broadcast
            self.sock.sendto(data, ("127.0.0.1", self.target_port))
            if self.target_host != "127.0.0.1":
                self.sock.sendto(data, (self.target_host, self.target_port))
            print(f"[{self.dev_id}] Sent Hello -> MAC: {self.mac}")
        except Exception as e:
            print(f"[{self.dev_id}] Failed to send hello: {e}")

    def _hello_loop(self):
        self.send_hello()
        while self.running:
            time.sleep(5)
            if self.running:
                self.send_hello()

    def handle_broadcast(self, obj: dict, addr):
        if obj.get("type") != "set_key":
            return

        msg_id = obj.get("message_id")
        key = obj.get("chunk_key")
        target = obj.get("target", "all")

        if target != "all" and target != self.dev_id:
            return

        print(f"[{self.dev_id}] Recv set_key from {addr}: msg_id={msg_id}, key={key}")

        if self.behavior == "timeout":
            print(f"[{self.dev_id}] [SIMULATING TIMEOUT] Dropping broadcast, no ACK will be sent.")
            return

        if self.behavior == "error":
            print(f"[{self.dev_id}] [SIMULATING ERROR] Sending error ACK.")
            ack = {
                "protocol": "esp32-control",
                "version": 1,
                "type": "key_ack",
                "message_id": msg_id,
                "mac": self.mac,
                "id": self.dev_id,
                "status": "error",
                "error_code": "FLASH_WRITE_FAIL",
                "message": "Simulated hardware flash write error"
            }
        else:
            self.stored_key = key
            self.last_msg_id = msg_id
            ack = {
                "protocol": "esp32-control",
                "version": 1,
                "type": "key_ack",
                "message_id": msg_id,
                "mac": self.mac,
                "id": self.dev_id,
                "status": "ok",
                "message": "key received"
            }

        ack_data = json.dumps(ack).encode("utf-8")
        try:
            self.sock.sendto(ack_data, addr)
            print(f"[{self.dev_id}] Sent ACK to {addr} (Status: {ack['status']})")
        except Exception as e:
            print(f"[{self.dev_id}] Error sending ACK: {e}")


class SimulatorManager:
    def __init__(self, count: int, host: str, port: int, timeout_node: int = -1, error_node: int = -1):
        self.count = count
        self.host = host
        self.port = port
        self.timeout_node = timeout_node
        self.error_node = error_node
        self.devices = []
        self.running = False
        self.listen_sock = None

    def start(self):
        self.running = True

        # Bind shared broadcast listener on port 4210
        try:
            self.listen_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.listen_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.listen_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            self.listen_sock.bind(("", self.port))
            self.listen_sock.settimeout(0.5)
            print(f"[SimulatorManager] Bound shared broadcast listener to 0.0.0.0:{self.port}")
        except Exception as e:
            print(f"[SimulatorManager] Warning: Could not bind to port {self.port}: {e}")

        # Start simulated devices
        for i in range(1, self.count + 1):
            behavior = "normal"
            if i == self.timeout_node:
                behavior = "timeout"
            elif i == self.error_node:
                behavior = "error"

            dev = SimulatedEsp32(i, self.host, self.port, behavior)
            self.devices.append(dev)
            dev.start()
            print(f"Started simulated device {dev.dev_id} ({dev.mac}) [Behavior: {behavior}]")

        self.listen_thread = threading.Thread(target=self._listen_loop, daemon=True)
        self.listen_thread.start()

    def _listen_loop(self):
        while self.running and self.listen_sock:
            try:
                data, addr = self.listen_sock.recvfrom(2048)
                try:
                    obj = json.loads(data.decode("utf-8"))
                except Exception:
                    continue

                if obj.get("type") == "set_key":
                    for dev in self.devices:
                        dev.handle_broadcast(obj, addr)
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"[SimulatorManager] Recv error: {e}")
                break

    def stop(self):
        self.running = False
        if self.listen_sock:
            try:
                self.listen_sock.close()
            except:
                pass
        for d in self.devices:
            d.stop()


def main():
    parser = argparse.ArgumentParser(description="ESP32 Multi-Node UDP Simulator")
    parser.add_argument("--count", type=int, default=3, help="Number of ESP32 devices to simulate (default: 3)")
    parser.add_argument("--host", type=str, default="127.0.0.1", help="Target host/IP for Hello (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=4210, help="Target UDP port (default: 4210)")
    parser.add_argument("--timeout-node", type=int, default=-1, help="Node index (1-based) to simulate timeout")
    parser.add_argument("--error-node", type=int, default=-1, help="Node index (1-based) to simulate error ACK")

    args = parser.parse_args()

    print(f"=== Starting ESP32 Simulator with {args.count} devices ===")
    mgr = SimulatorManager(args.count, args.host, args.port, args.timeout_node, args.error_node)
    mgr.start()

    print("\nSimulators are running! Press Ctrl+C to stop.\n")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopping simulated devices...")
        mgr.stop()
        print("Done.")

if __name__ == "__main__":
    main()
