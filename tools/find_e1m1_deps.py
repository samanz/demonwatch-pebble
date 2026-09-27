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
    name = b[o+8:o+16].split(b"\x00")[0].decode("latin1", errors="replace")
    lumps[name] = (fp, sz)
    lump_names.append(name)

# Read E1M1 SIDEDEFS to find texture numbers
side_fp, side_sz = lumps["SIDEDEFS"]
num_sides = side_sz // 30
used_textures = set()
for i in range(num_sides):
    sd_ofs = side_fp + i * 30
    top = struct.unpack_from(">h" if is_be else "<h", b, sd_ofs + 2)[0]
    bot = struct.unpack_from(">h" if is_be else "<h", b, sd_ofs + 10)[0]
    mid = struct.unpack_from(">h" if is_be else "<h", b, sd_ofs + 18)[0]
    for t in (top, bot, mid):
        if t > 0:
            used_textures.add(t)

print(f"E1M1 uses {len(used_textures)} unique texture indices.")

# Inspect TEXTUREP
tp_fp, tp_sz = lumps["TEXTUREP"]
# TEXTUREP contains patch numbers for each texture
num_entries = tp_sz // 2
patches_used = set()
for t_idx in used_textures:
    if t_idx < num_entries:
        p_num = struct.unpack_from(">h" if is_be else "<h", b, tp_fp + t_idx * 2)[0]
        if p_num > 0 and p_num < len(lump_names):
            patches_used.add(lump_names[p_num])

print(f"E1M1 uses {len(patches_used)} unique wall patches via TEXTUREP.")
for p in sorted(list(patches_used)):
    sz = lumps[p][1] if p in lumps else 0
    print(f"  Patch: {p:<10} size={sz}")

total_used_patch_bytes = sum(lumps[p][1] for p in patches_used if p in lumps)
print(f"Total size of patches actually used in E1M1: {total_used_patch_bytes} bytes ({total_used_patch_bytes/1024:.1f} KiB)")
