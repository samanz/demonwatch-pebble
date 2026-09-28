"""Convert art from the pinned Freedoom release into resources/freedoom-art.pbl.

Usage: python3 tools/import_freedoom.py work/freedoom-0.13.0.zip

Sprites: every frame in assets.sprite_frames(), front view, logical size and
offsets preserved, detail halved (2x2 sampling). Wall patches: assets.PATCHES,
composed from Freedoom textures, scaled to the listed width and tiled to 128
rows.

Sounds: assets.SOUNDS, converted to 8 kHz signed 8-bit PCM (convert_sound).

Colour: the Pebble has 4 levels per channel, which flattens Doom's greys and
browns into one grey. Each colour gets a mild saturation/brightness boost and
a 2x2 ordered dither per 2x2 texel block (texel pairs are stored once, so the
pattern must not split a pair).
"""
import colorsys
import struct as st
import sys
import zipfile

from assets import PATCHES, ROOT, SOUNDS, SPRITE_DONORS, WEAPON_SPRITES, patch, read_lumps, sprite_frames


def convert_sound(data, max_seconds, rate_out=8000):
    """DMX sound lump (8-bit unsigned) -> 8 kHz signed PCM, trailing silence
    trimmed, capped at max_seconds with a 40 ms fade-out."""
    fmt, rate, count = st.unpack_from('<HHI', data)
    assert fmt == 3, 'not a DMX sound lump'
    samples = data[8 + 16:8 + count - 16]   # skip the 16-byte pads
    n_out = int(len(samples) * rate_out / rate)
    out = []
    for i in range(n_out):
        pos = i * rate / rate_out
        j = int(pos)
        a = samples[j] - 128
        b = (samples[j + 1] - 128) if j + 1 < len(samples) else a
        out.append(a + (b - a) * (pos - j))
    while out and abs(out[-1]) < 3:
        out.pop()
    out = out[:int(max_seconds * rate_out)]
    fade = min(len(out), int(0.04 * rate_out))
    for k in range(fade):
        out[len(out) - fade + k] *= (fade - k) / fade
    return bytes(int(max(-128, min(127, round(v)))) & 0xFF for v in out)


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


BAYER = ((0, 2), (3, 1))


def make_converter(playpal):
    """Return colour(index, bx, by) -> Pebble colour code for block (bx, by)."""
    boosted = []
    for i in range(256):
        h, l, s = colorsys.rgb_to_hls(*(v / 255 for v in playpal[i * 3:i * 3 + 3]))
        rgb = colorsys.hls_to_rgb(h, min(1, l * 1.1), min(1, s * 1.25))
        boosted.append(tuple(v * 255 for v in rgb))

    def colour(index, bx, by):
        offset = ((BAYER[by & 1][bx & 1] + 0.5) / 4 - 0.5) * 85
        levels = [int(max(0, min(255, v + offset)) + 42) // 85 for v in boosted[index]]
        return levels[0] * 16 + levels[1] * 4 + levels[2]
    return colour


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
    colour = make_converter(wad['PLAYPAL'][:768])
    art = []

    for name in sprite_frames():
        # Engine frame letter -> Freedoom frame letter (e.g. the Baron's 8 frames).
        want = name
        if name[:4] in SPRITE_DONORS:
            want = name[:4] + SPRITE_DONORS[name[:4]][ord(name[4]) - 65] + name[5:]
        donor = next((n for n in (want, want[:5] + '1') if n in wad), None)
        if donor is None:
            donor = next((n for n in wad if n.startswith(want[:5]) and n[5:6] in ('0', '1')), None)
        if donor is None:
            candidates = [n for n in wad if n.startswith(want[:4]) and len(n) >= 6
                          and n[5] in ('0', '1') and n[4] <= want[4]]
            if not candidates:
                raise ValueError('Missing Freedoom sprite ' + name)
            donor = max(candidates, key=lambda n: n[4])
            print(f'Alias {name} to {donor}')
        w, h, left, top, pixels = decode(wad[donor])
        if name[:4] in WEAPON_SPRITES:
            left += 160

        def pixel(x, y, pixels=pixels):
            index = pixels[(y // 2) * 2][(x // 2) * 2]
            return None if index is None else colour(index, x // 2, y // 2)
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
            return colour(canvas[sy][min(sx, tw - 1)], x // 2, y // 2)
        art.append((name, patch(width, 128, 0, 0, pixel)))

    for name, seconds in SOUNDS.items():
        art.append((name, convert_sound(wad[name], seconds)))

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
