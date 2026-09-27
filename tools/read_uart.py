import socket
import sys

import json

def get_uart_port():
    try:
        with open('/tmp/pb-emulator.json') as f:
            data = json.load(f)
            return data['emery']['4.33.1']['qemu']['serial']
    except Exception:
        return 52009

def main():
    port = get_uart_port()
    s = socket.socket()
    s.settimeout(2.0)
    try:
        s.connect(('127.0.0.1', port))
        print(f"Connected to debug UART ({port})...")
        while True:
            try:
                data = s.recv(4096)
                if not data:
                    break
                sys.stdout.write(data.decode('utf-8', errors='replace'))
                sys.stdout.flush()
            except socket.timeout:
                break
    except Exception as e:
        print("Error:", e)
    finally:
        s.close()

if __name__ == '__main__':
    main()
