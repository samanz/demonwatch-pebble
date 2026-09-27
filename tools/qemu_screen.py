import socket
import json
import time
import os

def get_monitor_port():
    try:
        with open('/tmp/pb-emulator.json') as f:
            data = json.load(f)
            return data['emery']['4.33.1']['qemu']['monitor']
    except Exception:
        return 52153

def qemu_cmd(command):
    port = get_monitor_port()
    s = socket.socket()
    s.settimeout(2.0)
    try:
        s.connect(('127.0.0.1', port))
        greeting = s.recv(1024)
        s.sendall((command.strip() + '\n').encode('utf-8'))
        time.sleep(0.1)
        res = s.recv(4096)
        return res.decode('utf-8', errors='replace')
    finally:
        s.close()

def dump_screen(output_path="/tmp/qemu_screen.ppm"):
    qemu_cmd(f"screendump {output_path}")

def press_key(key):
    print(f"Pressing {key}...")
    qemu_cmd(f"sendkey {key}")
    time.sleep(0.3)

if __name__ == '__main__':
    dump_screen()
    if os.path.exists("/tmp/qemu_screen.ppm"):
        from PIL import Image
        im = Image.open('/tmp/qemu_screen.ppm')
        colors = set(im.getdata())
        print('Image dimensions:', im.size, 'Unique colors:', len(colors))
        im.save('/tmp/qemu_screen.png')
        print("Saved /tmp/qemu_screen.png successfully!")
