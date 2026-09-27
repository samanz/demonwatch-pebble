"""Print a size breakdown of a .pbl resource.

Usage: python3 tools/inspect_pbl.py [resources/arena.pbl]
"""
import struct
import sys

with open(sys.argv[1] if len(sys.argv) > 1 else "resources/arena.pbl", "rb") as f:
    data = f.read()

numlumps, infoofs = struct.unpack_from("<ii", data, 4)
cats = {}
details = []

for i in range(numlumps):
    pos, sz = struct.unpack_from("<ii", data, infoofs + i * 16)
    name = data[infoofs + i * 16 + 8 : infoofs + i * 16 + 16].split(b"\x00")[0].decode("latin1")
    prefix = name[:4]
    cats[prefix] = cats.get(prefix, 0) + sz
    details.append((name, sz))

print(f"Total size: {len(data):,} bytes ({len(data)/1024:.2f} KiB)")
print("\n--- By 4-char prefix ---")
for k, v in sorted(cats.items(), key=lambda x: -x[1])[:20]:
    print(f"{k:8}: {v:7,} bytes ({v/1024:5.1f} KiB)")

section_sizes = {"MAP": 0, "FLATS": 0, "PATCHES": 0, "SPRITES": 0, "OTHER": 0}
cur_sec = "MAP"
for name, sz in details:
    if name in ("P_START", "PP_START"): cur_sec = "PATCHES"
    elif name in ("P_END", "PP_END"): cur_sec = "OTHER"
    elif name in ("S_START", "SS_START"): cur_sec = "SPRITES"
    elif name in ("S_END", "SS_END"): cur_sec = "OTHER"
    elif name in ("F_START", "FF_START"): cur_sec = "FLATS"
    elif name in ("F_END", "FF_END"): cur_sec = "OTHER"
    elif name in ("E1M1", "THINGS", "LINEDEFS", "SIDEDEFS", "SEGS", "SSECTORS", "NODES", "SECTORS", "REJECT", "BLOCKMAP"):
        section_sizes["MAP"] += sz
    else:
        section_sizes[cur_sec] += sz

print("\n--- Sprite breakdown by actor ---")
sprite_actors = {}
for name, sz in details:
    if name.startswith(("POSS", "SPOS", "TROO", "SHTG", "PISG", "PUNG", "BAL1", "BAR1", "BLUD", "PUFF", "CLIP", "SHEL", "AMMO", "STIM", "MEDI", "BON1", "BON2", "ARM1", "ARM2", "BKEY", "YKEY", "RKEY", "SOUL", "SUIT")):
        act = name[:4]
        sprite_actors[act] = sprite_actors.get(act, 0) + sz

for act, sz in sorted(sprite_actors.items(), key=lambda x: -x[1]):
    print(f"{act:8}: {sz:6,} bytes ({sz/1024:4.1f} KiB)")

print("\n--- Patches breakdown ---")
patch_total = sum(sz for name, sz in details if name in [d[0] for d in details] and name not in sprite_actors and not name in ("E1M1", "THINGS", "LINEDEFS", "SIDEDEFS", "SEGS", "SSECTORS", "NODES", "SECTORS", "REJECT", "BLOCKMAP", "COLORMAP", "PLAYPAL", "TEXTURE1", "TEXTUREP", "TEXHEIGH"))
print(f"Total patch bytes: {patch_total:,} bytes ({patch_total/1024:.1f} KiB)")




