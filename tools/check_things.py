import struct

with open("resources/e1m1.pbl", "rb") as f:
    d = f.read()

nl, io = struct.unpack_from("<ii", d, 4)
things_data = b""
for i in range(nl):
    p, sz = struct.unpack_from("<ii", d, io + i * 16)
    name = d[io + i * 16 + 8 : io + i * 16 + 16].split(b"\x00")[0]
    if name == b"THINGS":
        things_data = d[p : p + sz]
        break

# In Doom, mapthings_t is 10 bytes: x, y, angle, type, options
# In Doom64KB, let's check size
print(f"THINGS lump length: {len(things_data)}")
# Let's inspect unique types
# Doom standard thing is 10 bytes: (int16 x, int16 y, int16 angle, int16 type, int16 flags)
types = set()
for i in range(0, len(things_data), 8):
    if i + 8 <= len(things_data):
        x, y, t, ang, opt = struct.unpack_from(">hhhbb", things_data, i)
        types.add(t)

print("Unique thing types in E1M1 (8-byte big-endian):", sorted(list(types)))


