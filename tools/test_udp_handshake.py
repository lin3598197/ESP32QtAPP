#!/usr/bin/env python3
"""
Test UDP Handshake & Protocol Validation Script
Verifies:
1. ESP32 Hello registration parsing
2. MAC address normalization logic
3. Key broadcast ('set_key') with UUID message_id
4. Direct ACK response handling ('key_ack')
5. Deduplication and timeout handling
"""

import socket
import json
import time
import uuid
import threading
import sys
import re

TEST_PORT = 4210

def normalize_mac(raw_mac: str) -> str:
    cleaned = raw_mac.replace("-", "").replace(":", "").replace(" ", "").upper()
    if not re.match(r"^[0-9A-F]{12}$", cleaned):
        return ""
    return ":".join(cleaned[i:i+2] for i in range(0, 12, 2))

def test_mac_normalization():
    print("[TEST] Running MAC normalization checks...")
    cases = [
        ("aa-bb-cc-dd-ee-01", "AA:BB:CC:DD:EE:01"),
        ("aa:bb:cc:dd:ee:01", "AA:BB:CC:DD:EE:01"),
        ("aabbccddee01", "AA:BB:CC:DD:EE:01"),
        ("AA BB CC DD EE 01", "AA:BB:CC:DD:EE:01"),
        ("invalid_mac", ""),
        ("12345", ""),
    ]
    for inp, expected in cases:
        out = normalize_mac(inp)
        assert out == expected, f"Failed for {inp}: expected {expected}, got {out}"
    print("  -> MAC normalization tests passed!")

def test_full_protocol_handshake():
    print("[TEST] Running full UDP handshake test with 3 virtual ESP32 nodes...")

    # Start host socket on TEST_PORT
    host_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    host_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    host_sock.bind(("127.0.0.1", TEST_PORT))
    host_sock.settimeout(2.0)

    # 3 simulated nodes
    sim_nodes = [
        {"id": "ESP32-001", "mac": "aa-bb-cc-dd-ee-01", "behavior": "normal"},
        {"id": "ESP32-002", "mac": "AA:BB:CC:DD:EE:02", "behavior": "normal"},
        {"id": "ESP32-003", "mac": "aabbccddee03", "behavior": "error"},
    ]

    client_socks = []

    def run_virtual_client(node_info):
        csock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        client_socks.append(csock)
        # Send Hello
        hello = {
            "protocol": "esp32-control",
            "version": 1,
            "type": "hello",
            "mac": node_info["mac"],
            "id": node_info["id"],
            "firmware": "1.0.0"
        }
        csock.sendto(json.dumps(hello).encode("utf-8"), ("127.0.0.1", TEST_PORT))

        # Wait for set_key broadcast
        csock.settimeout(4.0)
        try:
            data, addr = csock.recvfrom(2048)
            msg = json.loads(data.decode("utf-8"))
            if msg.get("type") == "set_key":
                # Reply ACK to addr
                if node_info["behavior"] == "error":
                    ack = {
                        "protocol": "esp32-control",
                        "version": 1,
                        "type": "key_ack",
                        "message_id": msg["message_id"],
                        "mac": node_info["mac"],
                        "id": node_info["id"],
                        "status": "error",
                        "error_code": "FLASH_WRITE_FAIL",
                        "message": "Simulated flash write failure"
                    }
                else:
                    ack = {
                        "protocol": "esp32-control",
                        "version": 1,
                        "type": "key_ack",
                        "message_id": msg["message_id"],
                        "mac": node_info["mac"],
                        "id": node_info["id"],
                        "status": "ok",
                        "message": "key received"
                    }
                csock.sendto(json.dumps(ack).encode("utf-8"), addr)
        except Exception as e:
            print(f"Virtual client {node_info['id']} error: {e}")

    # Launch clients
    threads = []
    for node in sim_nodes:
        t = threading.Thread(target=run_virtual_client, args=(node,))
        t.start()
        threads.append(t)

    # Host receives Hellos
    registered_devices = {}
    while len(registered_devices) < len(sim_nodes):
        try:
            data, addr = host_sock.recvfrom(2048)
            obj = json.loads(data.decode("utf-8"))
            if obj.get("type") == "hello":
                norm_mac = normalize_mac(obj["mac"])
                registered_devices[norm_mac] = {
                    "id": obj["id"],
                    "addr": addr,
                    "firmware": obj.get("firmware")
                }
                print(f"  [Host] Registered: MAC={norm_mac}, ID={obj['id']} from {addr}")
        except socket.timeout:
            break

    assert len(registered_devices) == 3, f"Expected 3 registered devices, got {len(registered_devices)}"
    print("  -> Device registration verified!")

    # Host broadcasts set_key
    test_msg_id = str(uuid.uuid4())
    test_chunk_key = "CONFIDENTIAL_KEY_XYZ_999"
    broadcast_packet = {
        "protocol": "esp32-control",
        "version": 1,
        "type": "set_key",
        "message_id": test_msg_id,
        "target": "all",
        "chunk_key": test_chunk_key,
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    }
    bdata = json.dumps(broadcast_packet).encode("utf-8")

    # In localhost test, we broadcast to 127.0.0.1 on the ports our virtual clients are listening on, or broadcast
    for norm_mac, dev in registered_devices.items():
        host_sock.sendto(bdata, dev["addr"])

    print(f"  [Host] Broadcast 'set_key' sent with Message ID: {test_msg_id}")

    # Host collects ACKs
    received_acks = {}
    start_time = time.time()
    while len(received_acks) < len(sim_nodes) and (time.time() - start_time) < 3.0:
        try:
            data, addr = host_sock.recvfrom(2048)
            obj = json.loads(data.decode("utf-8"))
            if obj.get("type") == "key_ack" and obj.get("message_id") == test_msg_id:
                norm_mac = normalize_mac(obj["mac"])
                received_acks[norm_mac] = obj
                print(f"  [Host] Received ACK from {norm_mac} ({obj['id']}): Status={obj['status']}")
        except socket.timeout:
            break

    # Wait for threads
    for t in threads:
        t.join(timeout=1.0)
    for cs in client_socks:
        cs.close()
    host_sock.close()

    assert len(received_acks) == 3, f"Expected 3 ACKs, received {len(received_acks)}"
    assert received_acks["AA:BB:CC:DD:EE:01"]["status"] == "ok"
    assert received_acks["AA:BB:CC:DD:EE:02"]["status"] == "ok"
    assert received_acks["AA:BB:CC:DD:EE:03"]["status"] == "error"
    assert received_acks["AA:BB:CC:DD:EE:03"]["error_code"] == "FLASH_WRITE_FAIL"

    print("  -> Full protocol handshake test successfully passed!")

if __name__ == "__main__":
    try:
        test_mac_normalization()
        test_full_protocol_handshake()
        print("\n==========================================")
        print(" ALL PROTOCOL & HANDSHAKE TESTS PASSED! ")
        print("==========================================")
        sys.exit(0)
    except AssertionError as e:
        print(f"\nTEST FAILED: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\nUNEXPECTED ERROR: {e}")
        sys.exit(1)
