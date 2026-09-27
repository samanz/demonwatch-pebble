import struct

with open("resources/e1m1.pbl", "rb") as f:
    d = f.read()

nl, io = struct.unpack_from("<ii", d, 4)
lumps = []
for i in range(nl):
    p, sz = struct.unpack_from("<ii", d, io + i * 16)
    name = d[io + i * 16 + 8 : io + i * 16 + 16].split(b"\x00")[0].decode("latin1")
    lumps.append((name, sz))

# Let's inspect HUD / Weapon sprites
for prefix in ("SHTG", "PISG", "PUNG"):
    weap_lumps = [(n, s) for n, s in lumps if n.startswith(prefix)]
    print(f"\n{prefix} lumps ({sum(s for _, s in weap_lumps)} bytes):")
    for n, s in weap_lumps:
        print(f"  {n:8}: {s} bytes")
