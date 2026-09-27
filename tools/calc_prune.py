import struct

with open("resources/e1m1.pbl", "rb") as f:
    d = f.read()

nl, io = struct.unpack_from("<ii", d, 4)
prune_candidates = []
total_prune_sz = 0

for i in range(nl):
    p, sz = struct.unpack_from("<ii", d, io + i * 16)
    name = d[io + i * 16 + 8 : io + i * 16 + 16].split(b"\x00")[0].decode("latin1")
    # Check if name is diagonal angle: 2A8, 2B8, 4A6, 4B6, etc.
    # Name format: POSSA2A8, POSSA4A6, etc.
    if len(name) >= 7 and (("2" in name[5:] and "8" in name[5:]) or ("4" in name[5:] and "6" in name[5:])):
        prune_candidates.append((name, sz))
        total_prune_sz += sz

print(f"Number of diagonal rotation sprites: {len(prune_candidates)}")
print(f"Total size of diagonal rotation sprites: {total_prune_sz:,} bytes ({total_prune_sz/1024:.2f} KiB)")
print(f"Bundle size if pruned: {len(d) - total_prune_sz:,} bytes ({(len(d) - total_prune_sz)/1024:.2f} KiB)")
