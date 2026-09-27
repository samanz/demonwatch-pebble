import struct

b = open("/mnt/c/Users/Sam/.gemini/antigravity/scratch/pdoom/resources/e1m1.pbl", "rb").read()
numlumps = struct.unpack_from("<i", b, 4)[0]
infoofs = struct.unpack_from("<i", b, 8)[0]

for i in range(numlumps):
    o = infoofs + i * 16
    pos = struct.unpack_from("<i", b, o)[0]
    sz = struct.unpack_from("<i", b, o+4)[0]
    name = b[o+8:o+16].split(b"\x00")[0]
    if name == b"WALL00_1":
        patch_bytes = b[pos:pos+sz]
        w, h, left, top = struct.unpack_from(">hhhh", patch_bytes, 0)
        print(f"BE Patch {name.decode()}: width={w}, height={h}, left={left}, top={top}, size={sz}")
        col0_ofs = struct.unpack_from(">i", patch_bytes, 8)[0]
        print(f"Col 0 offset: {col0_ofs}, bytes at col 0: {[hex(x) for x in patch_bytes[col0_ofs:col0_ofs+10]]}")
        break
