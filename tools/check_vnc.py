import socket

s = socket.socket()
s.connect(("127.0.0.1", 5901))
s.settimeout(2.0)
banner = s.recv(1024)
print("VNC Banner:", banner)
s.close()
