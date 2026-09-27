import socket

s = socket.socket()
try:
    s.connect(("localhost", 57753))
    s.settimeout(2.0)
    prompt = s.recv(1024)
    print("Monitor prompt:", prompt.decode("latin1", errors="ignore"))
    s.sendall(b"info status\r\n")
    import time
    time.sleep(0.5)
    print("Status:", s.recv(4096).decode("latin1", errors="ignore"))
    s.sendall(b"x/8i 0x000c46e0\r\n")
    time.sleep(0.5)
    print("Disassembly:\n", s.recv(4096).decode("latin1", errors="ignore"))


except Exception as e:
    print("Monitor error:", e)
finally:
    s.close()
