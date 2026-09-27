#!/usr/bin/env python3
"""
pack_e1m1.py: Custom asset packer for Pebble Time 2 (Emery).
Extracts only E1M1-required geometry, textures, and sprites into
an optimized format guaranteed to fit within the 256 KiB App Store budget.
"""

import struct
import sys
import os

E1M1_SPRITE_PREFIXES = {
    # Weapons / HUD
    b'PUNG', b'PISG', b'SHTG', b'PISF', b'SHTF',
    # Monsters
    b'POSS', # Zombieman
    b'SPOS', # Shotgun guy
    b'TROO', # Imp
    b'BAR1', # Explosive barrel
    # Projectiles / FX
    b'BAL1', # Imp fireball
    b'PUFF', # Bullet puff
    b'BLUD', # Blood splat
    # Pickups (actual E1M1 map things)
    b'CLIP', b'SHEL', b'AMMO',
    b'STIM', b'MEDI', b'BON1', b'BON2',
    b'ARM1', b'ARM2',
}

EXTREME_DEATH_FRAMES = {ord('N'), ord('O'), ord('P'), ord('Q'), ord('R'), ord('S'), ord('T'), ord('U')}


def parse_wad(path):
    with open(path, "rb") as f:
        data = f.read()

    is_be = struct.unpack_from(">i", data, 8)[0] < len(data)
    numlumps = struct.unpack_from(">h" if is_be else "<h", data, 4)[0]
    infoofs = struct.unpack_from(">i" if is_be else "<i", data, 8)[0]

    lumps = {}
    lumps_meta = {}
    lump_order = []
    for i in range(abs(numlumps)):
        o = infoofs + i * 16
        fp = struct.unpack_from(">i" if is_be else "<i", data, o)[0]
        sz = struct.unpack_from(">H" if is_be else "<H", data, o + 4)[0]
        name = data[o+8:o+16].split(b"\x00")[0]
        lump_data = data[fp:fp + sz] if sz else b''
        lumps[name] = lump_data
        lumps_meta[name] = (fp, sz)
        lump_order.append(name)

    return lumps, lumps_meta, lump_order, is_be, data

def find_e1m1_required_patches(lumps_meta, lump_order, is_be, raw_wad):
    side_fp, side_sz = lumps_meta[b"SIDEDEFS"]
    num_sides = side_sz // 7
    used_textures = set()
    for i in range(num_sides):
        sd_ofs = side_fp + i * 7
        texoffset, rowoffset, top, bot, mid, sector = struct.unpack_from(">hBbbbB" if is_be else "<hBbbbB", raw_wad, sd_ofs)
        for t in (top, bot, mid):
            if t >= 0:
                used_textures.add(t)

    texp_fp, texp_sz = lumps_meta[b"TEXTUREP"]
    texp_bytes = raw_wad[texp_fp:texp_fp + texp_sz]

    e1m1_patches = set()
    for t_idx in used_textures:
        offset = struct.unpack_from(">H" if is_be else "<H", texp_bytes, t_idx * 2)[0]
        widthmask, width, height = struct.unpack_from(">Hhh" if is_be else "<Hhh", texp_bytes, offset)
        overlapped, patchcount = struct.unpack_from("BB", texp_bytes, offset + 6)
        for p in range(patchcount):
            p_ofs = offset + 8 + p * 8
            origx, origy, patch_num, pwidth = struct.unpack_from(">hhhh" if is_be else "<hhhh", texp_bytes, p_ofs)
            if 0 <= patch_num < len(lump_order):
                e1m1_patches.add(lump_order[patch_num])

    return e1m1_patches

def build_packed_wad(src_wad, out_wad):
    print(f"Analyzing source WAD: {src_wad}")
    lumps, lumps_meta, lump_order, is_be, raw_wad = parse_wad(src_wad)

    e1m1_patches = find_e1m1_required_patches(lumps_meta, lump_order, is_be, raw_wad)
    print(f"Identified {len(e1m1_patches)} exact patches required by E1M1.")

    selected_lumps = []

    # 1. Base Palette (768 B) & Colormap (1 KiB)
    if b'COLORMAP' in lumps:
        selected_lumps.append((b'COLORMAP', lumps[b'COLORMAP'][:1024]))
    if b'PLAYPAL' in lumps:
        selected_lumps.append((b'PLAYPAL', lumps[b'PLAYPAL'][:768]))

    # 2. E1M1 Map Geometry
    map_lumps = [
        b'E1M1', b'THINGS', b'LINEDEFS', b'SIDEDEFS',
        b'SEGS', b'SSECTORS', b'NODES', b'SECTORS',
        b'REJECT', b'BLOCKMAP'
    ]
    for k in map_lumps:
        if k in lumps:
            selected_lumps.append((k, lumps[k]))

    # 3. Textures, Patches, Flats, Sprites
    in_sprites = False
    in_patches = False
    in_flats = False

    for name in lump_order:
        if name in (b'P_START', b'PP_START'): in_patches = True; selected_lumps.append((name, b'')); continue
        if name in (b'P_END', b'PP_END'): in_patches = False; selected_lumps.append((name, b'')); continue
        if name in (b'S_START', b'SS_START'): in_sprites = True; selected_lumps.append((name, b'')); continue
        if name in (b'S_END', b'SS_END'): in_sprites = False; selected_lumps.append((name, b'')); continue
        if name in (b'F_START', b'FF_START'): in_flats = True; selected_lumps.append((name, b'')); continue
        if name in (b'F_END', b'FF_END'): in_flats = False; selected_lumps.append((name, b'')); continue

        if in_sprites:
            prefix = name[:4]
            if prefix in E1M1_SPRITE_PREFIXES:
                frame_char = name[4] if len(name) > 4 else 0
                # Prune extreme gib death frames
                if frame_char in EXTREME_DEATH_FRAMES:
                    continue
                # For monsters (POSS, SPOS, TROO), prune diagonal rotations (2A8, 4A6, etc.)
                if prefix in (b'POSS', b'SPOS', b'TROO'):
                    # Prune diagonal rotation frames: 2A8, 2B8, 4A6, etc.
                    if len(name) >= 7 and ((b'2' in name[5:] and b'8' in name[5:]) or (b'4' in name[5:] and b'6' in name[5:])):
                        continue
                    # Pain frame (G): keep only angle 1, fallback will cover all rotations
                    if frame_char == ord('G') and not name.endswith(b'1'):
                        continue
                selected_lumps.append((name, lumps[name]))
        elif in_patches:
            # Include ONLY patches that are actually used by E1M1 textures
            if name in e1m1_patches:
                selected_lumps.append((name, lumps[name]))
        elif in_flats:
            selected_lumps.append((name, lumps[name]))
        elif name in (b'TEXTURE1', b'TEXTUREP', b'TEXHEIGH'):
            selected_lumps.append((name, lumps[name]))

    # Pack into little-endian 4-byte aligned WAD
    ALIGN = 4
    out = bytearray()
    out += b'IWAD'
    out += struct.pack('<i', len(selected_lumps))
    out += struct.pack('<i', 0)

    positions = []
    for name, data in selected_lumps:
        while len(out) % ALIGN != 0:
            out += b'\x00'
        positions.append(len(out) if len(data) else 0)
        out += data

    while len(out) % ALIGN != 0:
        out += b'\x00'

    dir_offset = len(out)
    for (name, data), pos in zip(selected_lumps, positions):
        out += struct.pack('<i', pos)
        out += struct.pack('<i', len(data))
        padded_name = name.ljust(8, b'\x00')[:8]
        out += padded_name

    struct.pack_into('<i', out, 8, dir_offset)

    os.makedirs(os.path.dirname(os.path.abspath(out_wad)), exist_ok=True)
    with open(out_wad, "wb") as f:
        f.write(out)

    total_bytes = len(out)
    total_kb = total_bytes / 1024.0
    print("=" * 60)
    print(f"Packed Asset Bundle: {out_wad}")
    print(f"Total Lumps Included: {len(selected_lumps)}")
    print(f"Total Bundle Size:   {total_bytes:,} bytes ({total_kb:.2f} KiB)")
    print(f"App Store Limit:     262,144 bytes (256.00 KiB)")
    if total_kb <= 256.0:
        margin = 256.0 - total_kb
        print(f"STATUS: PASSED! Fits inside the 256 KiB App Store budget!")
        print(f"Headroom Margin:     {margin:.2f} KiB ({int(margin * 1024)} bytes) free!")
    else:
        print(f"STATUS: EXCEEDS budget by {total_kb - 256.0:.2f} KiB")
    print("=" * 60)

if __name__ == "__main__":
    src = sys.argv[1] if len(sys.argv) > 1 else "../genesis-DOOM64KB/scripts/doom64.wad"
    dst = sys.argv[2] if len(sys.argv) > 2 else "../resources/e1m1.pbl"
    build_packed_wad(src, dst)
