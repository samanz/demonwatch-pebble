"""Assemble resources/pdoom.pbl: maps, textures, wall patches and sprites.

Maps come from tools/levels/e1m*.py (each defines build() -> doommap.Map) and
go through tools/convert_map.py. Art comes from resources/freedoom-art.pbl
(see import_freedoom.py); any lump missing there gets a plain placeholder so
the build never depends on downloading Freedoom.

Usage: python3 tools/build_wad.py [map module ...]   (default: all of MAPS)
Generated geometry and placeholder art may be redistributed under CC0-1.0.
"""
import importlib
import struct as st
import sys

from assets import PATCHES, ROOT, patch, read_lumps, sprite_frames, texture_names, texture_patch
from convert_map import MapError, build as build_map
from doommap import check

OUTPUT = ROOT / 'resources/pdoom.pbl'
MAPS = ['e1m1', 'e1m2', 'e1m3']
RESOURCE_LIMIT = 262144 - 4096   # Emery resource budget, minus headroom
# Per-level zone budget for convert_map's estimate. Measured in the emulator
# with the 30 KB zone: 26.6 KB is free when a new game starts, and E1M1
# (estimate 10.7 KB) really uses 12.7 KB, so real use is ~1.2x the estimate.
# Keep ~2.5 KB spare for projectiles, puffs and blood: 24 KB / 1.2.
ZONE_BUDGET = 20000


def colormap():
    """32 light levels x 64 Pebble colours (src/doom/r_draw.c COLORMAP_STRIDE).
    Level 0 is full bright; each level scales every 2-bit channel by
    (32 - level) / 32 and rounds, so light 160+ is untouched, 128 is dimmer,
    96 is dark and 64 is very dark."""
    out = bytearray()
    for level in range(32):
        f = (32 - level) / 32
        for c in range(64):
            r, g, b = (c >> 4) & 3, (c >> 2) & 3, c & 3
            out += bytes([int(r * f + 0.5) * 16 + int(g * f + 0.5) * 4 + int(b * f + 0.5)])
    return bytes(out)


def placeholder_sprite(name):
    if name[:4] in ('PUNG', 'PISG', 'PISF', 'SHTG', 'SHTF', 'CHGG', 'CHGF'):
        return patch(64, 64, 32, -100, lambda x, y: 42 if abs(x - 32) < 8 + y // 4 else None)
    if name[:4] in ('POSS', 'SPOS', 'TROO', 'SARG'):
        return patch(32, 56, 16, 56, lambda x, y: 18 if 5 < x < 27 and 2 < y < 54 else None)
    return patch(24, 20, 12, 20, lambda x, y: 63 if 3 < x < 21 and 3 < y < 18 else None)


def placeholder_wall(name):
    width = PATCHES[name][1]
    return patch(width, 128, 0, 0, lambda x, y: 1 if x in (0, width - 1) or y % 32 < 2 else 21)


def build(maps=MAPS):
    art_path = ROOT / 'resources/freedoom-art.pbl'
    art = read_lumps(art_path.read_bytes()) if art_path.exists() else {}
    missing = []

    def art_or(name, fallback):
        if name in art:
            return art[name]
        missing.append(name)
        return fallback(name)

    textures = texture_names()
    lumps = [('COLORMAP', colormap())]
    report = []
    sys.path.insert(0, str(ROOT / 'tools/levels'))
    for module_name in maps:
        m = importlib.import_module(module_name).build()
        try:
            route = check(m)
            map_lumps, stats = build_map(m, textures)
        except (MapError, ValueError) as e:
            sys.exit(f'{module_name}: {e}')
        if stats['zone_estimate'] > ZONE_BUDGET:
            sys.exit(f"{module_name}: needs ~{stats['zone_estimate']} B of zone; the budget is {ZONE_BUDGET}")
        stats['route'] = route
        lumps += map_lumps
        report.append((m.name, stats))

    tex1_index = len(lumps)
    lumps += [('TEXTURE1', b''), ('TEXTUREP', b''),
              ('TEXHEIGH', st.pack('<' + 'h' * len(textures), *([128] * len(textures))))]
    lumps.append(('P_START', b''))
    patch_index = {}
    for name in PATCHES:
        patch_index[name] = len(lumps)
        lumps.append((name, art_or(name, placeholder_wall)))
    lumps.append(('P_END', b''))

    tex1 = bytearray(st.pack('<i', len(textures)) + bytes(4 * len(textures)))
    texp = bytearray(2 * len(textures))
    for i, name in enumerate(textures):
        pname = texture_patch(name)
        width = st.unpack_from('<h', lumps[patch_index[pname]][1])[0]
        st.pack_into('<i', tex1, 4 + i * 4, len(tex1))
        tex1.extend(st.pack('<8s', name.encode()))   # the engine reads only the name
        st.pack_into('<H', texp, i * 2, len(texp))
        texp.extend(st.pack('<HhhBBhhhh', width - 1, width, 128, 0, 1, 0, 0, patch_index[pname], width))
    lumps[tex1_index] = ('TEXTURE1', bytes(tex1))
    lumps[tex1_index + 1] = ('TEXTUREP', bytes(texp))

    lumps.append(('S_START', b''))
    for name in sprite_frames():
        lumps.append((name, art_or(name, placeholder_sprite)))
    lumps.append(('S_END', b''))

    out = bytearray(b'IWAD' + st.pack('<ii', len(lumps), 0))
    directory, dedup = [], {}
    for name, data in lumps:
        if data not in dedup:
            out.extend(bytes((-len(out)) % 4))
            dedup[data] = len(out)
            out.extend(data)
        directory.append((dedup[data], len(data), name.encode()))
    out.extend(bytes((-len(out)) % 4))
    st.pack_into('<i', out, 8, len(out))
    for pos, size, name in directory:
        out.extend(st.pack('<ii8s', pos, size, name))
    if len(out) > RESOURCE_LIMIT:
        sys.exit(f'{OUTPUT.name} is {len(out)} bytes; the budget is {RESOURCE_LIMIT}')
    OUTPUT.write_bytes(out)

    print(f'{OUTPUT.name}: {len(out):,} bytes of {RESOURCE_LIMIT:,}; {len(lumps)} lumps, '
          f'{len(textures)} textures')
    for name, s in report:
        print(f"  {name}: {s['lines']} lines, {s['sectors']} sectors, {s['segs']} segs, "
              f"{s['things']} things; {s['lump_bytes']:,} B lumps, ~{s['zone_estimate']:,} B zone\n"
              f"        route: {s['route']}")
    if missing:
        print(f'  placeholder art for {len(missing)} lumps (run import_freedoom.py): {" ".join(missing[:12])}')


if __name__ == '__main__':
    build(sys.argv[1:] or MAPS)
