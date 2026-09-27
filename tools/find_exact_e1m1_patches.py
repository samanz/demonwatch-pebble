import struct

wad_path = "/mnt/c/Users/Sam/.gemini/antigravity/scratch/genesis-DOOM64KB/scripts/doom64.wad"
b = open(wad_path, "rb").read()

is_be = struct.unpack_from(">i", b, 8)[0] < len(b)
numlumps = struct.unpack_from(">h" if is_be else "<h", b, 4)[0]
infoofs = struct.unpack_from(">i" if is_be else "<i", b, 8)[0]

lumps = {}
lump_names = []
for i in range(abs(numlumps)):
    o = infoofs + i * 16
    fp = struct.unpack_from(">i" if is_be else "<i", b, o)[0]
    sz = struct.unpack_from(">H" if is_be else "<H", b, o + 4)[0]
    name = b[o+8:o+16].split(b"\x00")[0]
    lumps[name] = (fp, sz)
    lump_names.append(name)

# Sidedefs in E1M1: 7 bytes each
side_fp, side_sz = lumps[b"SIDEDEFS"]
num_sides = side_sz // 7
used_textures = set()
for i in range(num_sides):
    sd_ofs = side_fp + i * 7
    texoffset, rowoffset, top, bot, mid, sector = struct.unpack_from(">hBbbbB" if is_be else "<hBbbbB", b, sd_ofs)
    for t in (top, bot, mid):
        if t >= 0:
            used_textures.add(t)

print(f"E1M1 uses {len(used_textures)} textures: {sorted(list(used_textures))}")

texp_fp, texp_sz = lumps[b"TEXTUREP"]
texp_bytes = b[texp_fp:texp_fp + texp_sz]

e1m1_patches = set()
for t_idx in used_textures:
    offset = struct.unpack_from(">H" if is_be else "<H", texp_bytes, t_idx * 2)[0]
    widthmask, width, height = struct.unpack_from(">Hhh" if is_be else "<Hhh", texp_bytes, offset)
    overlapped, patchcount = struct.unpack_from("BB", texp_bytes, offset + 6)
    for p in range(patchcount):
        p_ofs = offset + 8 + p * 8
        origx, origy, patch_num, pwidth = struct.unpack_from(">hhhh" if is_be else "<hhhh", texp_bytes, p_ofs)
        if 0 <= patch_num < len(lump_names):
            e1m1_patches.add(lump_names[patch_num])

print(f"\nExact patches used by E1M1 textures: {len(e1m1_patches)}")
total_sz = 0
for p in sorted(list(e1m1_patches)):
    sz = lumps[p][1] if p in lumps else 0
    total_sz += sz
    print(f"  {p.decode('latin1', errors='replace'):<12} size={sz}")

print(f"\nTotal size of patches actually used in E1M1: {total_sz} bytes ({total_sz/1024:.1f} KiB)")
