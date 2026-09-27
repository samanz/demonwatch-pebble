"""Build nodes for a vanilla Doom map and convert it to Doom64KB lumps.

Pipeline: doommap.Map -> vanilla PWAD -> zdbsp (nodes, blockmap, zero reject)
-> Doom64KB little-endian lumps as read by src/doom/p_setup.c:

  THINGS    8 B  x, y, type, angle/45 (int8), options (int8)
  LINEDEFS 31 B  v1, v2 inline, lineno, dx, dy, sidenum[2], bbox
                 (top, bottom, left, right), tag, flags (uint8), slopetype,
                 special (int8)
  SIDEDEFS  7 B  x offset, y offset (uint8), top/bottom/mid texture index
                 (int8, 0 = none), sector (uint8); identical sides shared
  SEGS     18 B  v1, v2 inline, offset, angle + 90 degrees (BAM16), sidenum,
                 linenum, front sector, back sector (255 = none)
  SSECTORS, NODES, REJECT, BLOCKMAP: vanilla layout
  SECTORS  12 B  floor, ceiling, floor colour, ceiling colour, light (uint8),
                 special (int8), tag

Colours are 6-bit Pebble codes; -2 is sky and -3 animated nukage.
The seg-angle and offset conventions were checked against Doom64KB's own
converted E1M1 (693/721 segs exact, the rest within rounding).

Set ZDBSP to override the node builder path.
"""
import os
import struct
import subprocess
import tempfile
from pathlib import Path

from doommap import read_wad, wad

ZDBSP = os.environ.get('ZDBSP', str(Path.home() / '.local/zdbsp/usr/bin/zdbsp'))
ORDER = ['THINGS', 'LINEDEFS', 'SIDEDEFS', 'SEGS', 'SSECTORS', 'NODES', 'SECTORS', 'REJECT', 'BLOCKMAP']

# Rough per-level zone cost of engine-side structures (ARM, bytes), used for
# the memory estimate. Calibrate against the "zone" figure in the app log.
SIZEOF = {'sector_t': 48, 'side_t': 12, 'linedata_t': 8, 'subsector_t': 4, 'mobj_t': 72, 'blocklink': 4}
ZONE_BLOCK_OVERHEAD = 16


class MapError(Exception):
    pass


def records(data, fmt):
    size = struct.calcsize(fmt)
    if len(data) % size:
        raise MapError(f'lump size {len(data)} is not a multiple of {size}')
    return [struct.unpack_from(fmt, data, i) for i in range(0, len(data), size)]


def run_nodebuilder(vanilla, name):
    with tempfile.TemporaryDirectory() as tmp:
        src, dst = Path(tmp, 'in.wad'), Path(tmp, 'out.wad')
        src.write_bytes(vanilla)
        # -R zero reject, -q keep indices stable (no pruning), -s split cost.
        result = subprocess.run([ZDBSP, '-R', '-q', '-t', '-s', '16', '-o', str(dst), str(src)],
                                capture_output=True, text=True)
        if result.returncode or not dst.exists():
            raise MapError(f'{name}: zdbsp failed:\n{result.stdout}{result.stderr}')
        return dict(read_wad(dst.read_bytes()))


def flat_code(name):
    name = name.rstrip(b'\0').decode('latin1').upper()
    if name == 'F_SKY1':
        return -2
    if name.startswith('NUKAGE'):
        return -3
    if name.startswith('C') and name[1:].isdigit() and 0 <= int(name[1:]) < 64:
        return int(name[1:])
    raise MapError(f'unknown flat {name!r}: use a colour code 0-63, SKY or NUKAGE')


def convert(vanilla, name, textures):
    """Return (lumps, stats). textures: list of texture names; index 0 = none."""
    tindex = {t.upper(): i for i, t in enumerate(textures)}
    L = run_nodebuilder(vanilla, name)

    def tex(raw):
        n = raw.rstrip(b'\0').decode('latin1').upper()
        if n in ('-', ''):
            return 0
        if n not in tindex:
            raise MapError(f'{name}: texture {n!r} is not in the texture list')
        return tindex[n]

    verts = records(L['VERTEXES'], '<hh')
    vlines = records(L['LINEDEFS'], '<HHHHHHH')
    vsides = records(L['SIDEDEFS'], '<hh8s8s8sh')
    vsegs = records(L['SEGS'], '<HHHHhh')
    vsectors = records(L['SECTORS'], '<hh8s8shhh')
    vthings = records(L['THINGS'], '<hhhhh')

    if len(vsectors) > 255:
        raise MapError(f'{name}: {len(vsectors)} sectors; the format allows 255')
    if len(textures) > 127:
        raise MapError(f'{name}: {len(textures)} textures; the format allows 127')

    # Sidedefs: convert, then share identical ones.
    side_map, packed_sides, side_key = [], [], {}
    for xo, yo, up, lo, mid, sec in vsides:
        key = struct.pack('<hBbbbB', xo, yo % 128, tex(up), tex(lo), tex(mid), sec)
        if key not in side_key:
            side_key[key] = len(packed_sides)
            packed_sides.append(key)
        side_map.append(side_key[key])

    def side(n):
        return 0xFFFF if n == 0xFFFF else side_map[n]

    lines = bytearray()
    for i, (a, b, flags, special, tag, s0, s1) in enumerate(vlines):
        (x1, y1), (x2, y2) = verts[a], verts[b]
        dx, dy = x2 - x1, y2 - y1
        slope = 1 if dx == 0 else 0 if dy == 0 else (2 if (dx > 0) == (dy > 0) else 3)
        if special > 127:
            raise MapError(f'{name}: line {i} special {special} does not fit int8')
        lines += struct.pack('<hhhhHhhHHhhhhhBbb', x1, y1, x2, y2, i, dx, dy, side(s0), side(s1),
                             max(y1, y2), min(y1, y2), min(x1, x2), max(x1, x2),
                             tag, flags & 0xFF, slope, special)

    segs = bytearray()
    for a, b, angle, linenum, direction, offset in vsegs:
        line = vlines[linenum]
        mine, other = line[5 + direction], line[6 - direction]
        front = vsides[mine][5]
        back = vsides[other][5] if other != 0xFFFF else 255
        segs += struct.pack('<hhhhhHHHBB', *verts[a], *verts[b], offset, (angle + 0x4000) & 0xFFFF,
                            side_map[mine], linenum, front, back)

    sectors = b''.join(struct.pack('<hhhhBbh', f, c, flat_code(fp), flat_code(cp), min(255, max(0, light)), special, tag)
                       for f, c, fp, cp, light, special, tag in vsectors)
    things = b''.join(struct.pack('<hhhbb', x, y, kind, (angle % 360) // 45, flags & 0xFF)
                      for x, y, angle, kind, flags in vthings)
    if not any(t[3] == 1 for t in vthings):
        raise MapError(f'{name}: no player 1 start')

    out = {'THINGS': things, 'LINEDEFS': bytes(lines), 'SIDEDEFS': b''.join(packed_sides),
           'SEGS': bytes(segs), 'SSECTORS': L['SSECTORS'], 'NODES': L['NODES'],
           'SECTORS': sectors, 'REJECT': L['REJECT'], 'BLOCKMAP': L['BLOCKMAP']}
    for k, data in out.items():
        if len(data) > 65535:
            raise MapError(f'{name}: {k} is {len(data)} bytes; lumps are limited to 65535')

    bw, bh = struct.unpack_from('<hh', L['BLOCKMAP'], 4)
    structs = (len(vsectors) * SIZEOF['sector_t'] + len(vsides) * SIZEOF['side_t']
               + len(vlines) * SIZEOF['linedata_t'] + len(L['SSECTORS']) // 4 * SIZEOF['subsector_t']
               + len(vthings) * SIZEOF['mobj_t'] + bw * bh * SIZEOF['blocklink'])
    lump_bytes = sum(len(d) for d in out.values())
    stats = {'lines': len(vlines), 'sides': len(packed_sides), 'segs': len(vsegs),
             'subsectors': len(L['SSECTORS']) // 4, 'nodes': len(L['NODES']) // 28,
             'sectors': len(vsectors), 'things': len(vthings), 'lump_bytes': lump_bytes,
             'zone_estimate': lump_bytes + structs + 20 * ZONE_BLOCK_OVERHEAD}
    return [(name, b'')] + [(k, out[k]) for k in ORDER], stats


def build(m, textures):
    return convert(m.write_wad(), m.name, textures)
