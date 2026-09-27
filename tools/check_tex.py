import struct

with open("resources/e1m1.pbl", "rb") as f:
    d = f.read()

nl, io = struct.unpack_from("<ii", d, 4)
for i in range(nl):
    p, sz = struct.unpack_from("<ii", d, io + i * 16)
    name = d[io + i * 16 + 8 : io + i * 16 + 16].split(b"\x00")[0]
    if name == b"TEXTUREP":
        t_data = d[p : p + sz]
        first_ofs = struct.unpack_from("<H", t_data, 0)[0]
        num_tex = first_ofs // 2
        print(f"TEXTUREP size: {sz} bytes, num textures defined: {num_tex}")
    if name == b"TEXTURE1":
        print(f"TEXTURE1 size: {sz} bytes")
