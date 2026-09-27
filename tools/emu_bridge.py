import os
import sys
import json
import time
import subprocess
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

PORT = 8088
PEBBLE_TOOL = "/home/sam/.local/share/uv/tools/pebble-tool/bin/pebble"
ENV = os.environ.copy()
ENV["PATH"] = "/home/sam/.local/bin:" + ENV.get("PATH", "")
ENV["LD_LIBRARY_PATH"] = "/home/sam/.local/usr/lib/x86_64-linux-gnu:/home/sam/.local/usr/lib/x86_64-linux-gnu/pulseaudio:" + ENV.get("LD_LIBRARY_PATH", "")

SCREENSHOT_PATH = "/tmp/pebble_screen.png"

class BridgeHandler(BaseHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(200)
        self.end_headers()

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == '/status':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            status = {
                "platform": "emery",
                "firmware": "4.33.2",
                "vnc_port": 5901,
                "ws_port": 6080,
                "app": "pDOOM (E1M1)",
                "uuid": "d00364e1-0e11-4000-8000-000000000001",
                "time": time.time()
            }
            self.wfile.write(json.dumps(status).encode('utf-8'))
        elif parsed.path == '/screenshot':
            # Capture screenshot via pebble tool
            subprocess.call([PEBBLE_TOOL, "screenshot", "--emulator", "emery", "--no-open", SCREENSHOT_PATH], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if os.path.exists(SCREENSHOT_PATH):
                with open(SCREENSHOT_PATH, "rb") as f:
                    img_data = f.read()
                self.send_response(200)
                self.send_header('Content-Type', 'image/png')
                self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
                self.end_headers()
                self.wfile.write(img_data)
            else:
                self.send_response(500)
                self.end_headers()
        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        parsed = urlparse(self.path)
        qs = parse_qs(parsed.query)
        if parsed.path == '/button':
            btn = qs.get('b', ['select'])[0]
            action = qs.get('a', ['click'])[0]
            if btn in ('select', 'up', 'down', 'back'):
                subprocess.call([PEBBLE_TOOL, "emu-button", "--emulator", "emery", action, btn], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({"success": True, "button": btn, "action": action}).encode('utf-8'))
                return
        elif parsed.path == '/tap':
            subprocess.call([PEBBLE_TOOL, "emu-tap", "--emulator", "emery"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({"success": True, "action": "tap"}).encode('utf-8'))
            return

        self.send_response(400)
        self.end_headers()

def run():
    server = HTTPServer(('0.0.0.0', PORT), BridgeHandler)
    print(f"Pebble Emulator Bridge Server running on http://localhost:{PORT}")
    server.serve_forever()

if __name__ == '__main__':
    run()
