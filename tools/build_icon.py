"""Original pixel portal icon; CC0. No external graphics dependencies."""
import struct,zlib
from pathlib import Path
w,h=25,25
pixels=bytearray(w*h*4)
for py in range(h):
    for px in range(w):
        x,y=px*32//w,py*28//h
        portal=(5<=x<=26 and 2<=y<=25 and (x<9 or x>22 or y<6))
        floor=(2<=x<=29 and 24<=y<=26)
        cross=(14<=x<=17 and 10<=y<=21) or (10<=x<=21 and 14<=y<=17)
        if portal or floor or cross:
            i=4*(py*w+px);pixels[i:i+4]=bytes([255]*4)
def chunk(name,data):
    return struct.pack('>I',len(data))+name+data+struct.pack('>I',zlib.crc32(name+data)&0xffffffff)
raw=b''.join(b'\0'+pixels[y*w*4:(y+1)*w*4] for y in range(h))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
(Path(__file__).resolve().parents[1]/'resources/menu_icon.png').write_bytes(png)
