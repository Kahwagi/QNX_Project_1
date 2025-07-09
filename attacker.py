import socket
import time
import struct
from Crypto.Cipher import AES
from Crypto.Hash import CMAC
from Crypto.Random import get_random_bytes

# ---------------- CONFIG ---------------- #
SERVER_IP = "192.168.25.29"
SERVER_PORT = 8000
SEND_INTERVAL = 3  # seconds

AES_KEY = bytes([
    0x00,0x01,0x02,0x03, 0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B, 0x0C,0x0D,0x0E,0x0F
])

AES_IV = bytes([
    0x10,0x11,0x12,0x13, 0x14,0x15,0x16,0x17,
    0x18,0x19,0x1A,0x1B, 0x1C,0x1D,0x1E,0x1F
])
# ---------------------------------------- #

def pack_sensor_data(temp, speed, lat, lon):
    return struct.pack("ffff", temp, speed, lat, lon)

def encrypt_sensor_data(plaintext: bytes) -> bytes:
    cipher = AES.new(AES_KEY, AES.MODE_CBC, AES_IV)
    pad_len = AES.block_size - len(plaintext) % AES.block_size
    padded = plaintext + bytes([pad_len] * pad_len)
    return cipher.encrypt(padded)

def compute_cmac(data: bytes) -> bytes:
    c = CMAC.new(AES_KEY, ciphermod=AES)
    c.update(data)
    return c.digest()

def corrupt_cmac(cmac: bytes) -> bytes:
    tampered = bytearray(cmac)
    tampered[0] ^= 0xFF  # Flip the first byte
    return bytes(tampered)

def send_payload(payload: bytes):
    try:
        with socket.create_connection((SERVER_IP, SERVER_PORT)) as sock:
            sock.sendall(payload)
            print(f"[+] Sent {len(payload)} bytes with tampered CMAC to tcp_receiver:{SERVER_PORT}")
    except Exception as e:
        print(f"[X] Connection failed: {e}")

def attacker_loop():
    while True:
        sensor_bytes = pack_sensor_data(55.0, 80.5, 30.1234, 31.5678)
        ciphertext = encrypt_sensor_data(sensor_bytes)
        good_cmac = compute_cmac(ciphertext)
        bad_cmac = corrupt_cmac(good_cmac)

        final_payload = ciphertext + bad_cmac
        send_payload(final_payload)
        time.sleep(SEND_INTERVAL)

if __name__ == "__main__":
    print("[!] Starting replay/tamper attack simulation with binary key/IV...")
    attacker_loop()
