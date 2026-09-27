"""Convert art from the pinned Freedoom release into resources/freedoom-art.pbl.

Usage: python3 tools/import_freedoom.py work/freedoom-0.13.0.zip

Sprites: every frame in assets.sprite_frames(), front view, logical size and
offsets preserved, detail halved (2x2 sampling), colours quantized to the
Pebble's 64-colour palette. Wall patches: assets.PATCHES, composed from
Freedoom textures, scaled to the listed width and tiled to 128 rows.
"""
import struct as st
import sys
import zipfile

from assets import PATCHES, ROOT, WEAPON_SPRITES, patch, read_lumps, sprite_frames


def decode(data):
    w, h, left, top = st.unpack_from('<hhhh', data)
    assert 0 < w <= 320 and 0 < h <= 254
    pixels = [[None] * w for _ in range(h)]
    for x in range(w):
        p = st.unpack_from('<I', data, 8 + 4 * x)[0]
        while data[p] != 255:
            y, n = data[p:p + 2]
            for j, index in enumerate(data[p + 3:p + 3 + n]):
                if y + j < h:
                    pixels[y + j][x] = index
            p += n + 4
    return w, h, left, top, pixels


def texture_canvas(wad, name):
    """Compose a Freedoom texture into rows of palette indices."""
    pnames, data = wad['PNAMES'], wad['TEXTURE1']
    for t in range(st.unpack_from('<i', data)[0]):
        off = st.unpack_from('<i', data, 4 + 4 * t)[0]
        if data[off:off + 8].rstrip(b'\0').decode() == name:
            break
    else:
        raise ValueError('Freedoom texture missing: ' + name)
    tw, th = st.unpack_from('<hh', data, off + 12)
    canvas = [[0] * tw for _ in range(th)]
    for j in range(st.unpack_from('<h', data, off + 20)[0]):
        ox, oy, pn, _, _ = st.unpack_from('<hhhhh', data, off + 22 + 10 * j)
        pw, ph, _, _, pixels = decode(wad[pnames[4 + 8 * pn:12 + 8 * pn].rstrip(b'\0').decode()])
        for y in range(ph):
            for x in range(pw):
                if 0 <= x + ox < tw and 0 <= y + oy < th and pixels[y][x] is not None:
                    canvas[y + oy][x + ox] = pixels[y][x]
    return canvas


def main(path):
    with zipfile.ZipFile(path) as z:
        wad = read_lumps(z.read('freedoom-0.13.0/freedoom1.wad'))
        dest = ROOT / 'licenses'
        dest.mkdir(exist_ok=True)
        for source, target in [('COPYING.txt', 'Freedoom-COPYING.txt'), ('CREDITS.txt', 'Freedoom-CREDITS.txt')]:
            (dest / target).write_bytes(z.read('freedoom-0.13.0/' + source))
    pal = wad['PLAYPAL'][:768]
    colors = [sum(((pal[i * 3 + c] + 42) // 85) * (16, 4, 1)[c] for c in range(3)) for i in range(256)]
    art = []

    for name in sprite_frames():
        donor = next((n for n in (name, name[:5] + '1') if n in wad), None)
        if donor is None:
            donor = next((n for n in wad if n.startswith(name[:5]) and n[5:6] in ('0', '1')), None)
        if donor is None:
            candidates = [n for n in wad if n.startswith(name[:4]) and len(n) >= 6
                          and n[5] in ('0', '1') and n[4] <= name[4]]
            if not candidates:
                raise ValueError('Missing Freedoom sprite ' + name)
            donor = max(candidates, key=lambda n: n[4])
            print(f'Alias {name} to {donor}')
        w, h, left, top, pixels = decode(wad[donor])
        if name[:4] in WEAPON_SPRITES:
            left += 160

        def pixel(x, y, pixels=pixels):
            index = pixels[(y // 2) * 2][(x // 2) * 2]
            return None if index is None else colors[index]
        art.append((name, patch(w, h, left, top, pixel)))

    for name, (source, width, overlay) in PATCHES.items():
        canvas = texture_canvas(wad, source)
        th, tw = len(canvas), len(canvas[0])
        if overlay:
            top_canvas = texture_canvas(wad, overlay[0])
            ox, oy = overlay[1], overlay[2]
            for y, row in enumerate(top_canvas):
                for x, index in enumerate(row):
                    if 0 <= y + oy < th and 0 <= x + ox < tw:
                        canvas[y + oy][x + ox] = index
        # Halve detail like the sprites: column pairs repeat, so patch()
        # stores each pair once; the watch view is only 120 pixels wide.
        def pixel(x, y, canvas=canvas, th=th, tw=tw):
            sx = (x // 2 * 2) * tw // width
            sy = (y // 2 * 2) % th
            return colors[canvas[sy][min(sx, tw - 1)]]
        art.append((name, patch(width, 128, 0, 0, pixel)))

    out = bytearray(b'IWAD' + bytes(8))
    directory = []
    for name, data in art:
        out.extend(bytes((-len(out)) % 4))
        directory.append((len(out), len(data), name.encode()))
        out.extend(data)
    off = len(out)
    for item in directory:
        out.extend(st.pack('<ii8s', *item))
    st.pack_into('<ii', out, 4, len(directory), off)
    (ROOT / 'resources/freedoom-art.pbl').write_bytes(out)
    print(f'Freedoom art: {len(directory)} lumps, {len(out)} bytes; largest {max(s for _, s, _ in directory)}')


if __name__ == '__main__':
    main(sys.argv[1])
