import socket
import sys

port = int(sys.argv[1]) if len(sys.argv) > 1 else 56997
s = socket.socket()
try:
    s.connect(("localhost", port))
    s.settimeout(2.0)
    data = s.recv(4096)
    print("Serial output:", data.decode("latin1", errors="ignore"))
except Exception as e:
    print("Error:", e)
