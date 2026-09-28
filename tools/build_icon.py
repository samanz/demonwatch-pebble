"""Demonwatch icon: a horned demon face in pixel art (original, CC0).

Writes resources/menu_icon.png (25 x 25 launcher icon) and the store icons
store/icon-48.png and store/icon-144.png (2x and 6x the 24 x 24 art, nearest
neighbour, so pixels stay crisp). Colours are exact Pebble palette values
(channels 0/85/170/255), so the watch shows them unchanged.
No external graphics dependencies.
"""
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Left half of each row; the right half is its mirror image.
LEFT = [
    '..KK........',
    '.KHK........',
    '.KHHK.......',
    '.KHHK.......',
    '..KHHK......',
    '..KHHHK..KKK',
    '...KHHHKKRRR',
    '....KHHRRRRR',
    '.....KRRRRRR',
    '....KRRRRRRR',
    '....KRKKKRRR',   # brow
    '...KRRKYYKRR',   # eyes, slanting down to the centre
    '...KRRRKYYKR',
    '...KRRRRKKRR',
    '...KrRRRRRRK',   # nose
    '...KrrRRRRRK',
    '....KrRRRRRR',
    '....KrKKKKKK',   # mouth
    '....KrKWKWKW',   # teeth
    '.....KKKKKKK',
    '.....KrWKWKW',
    '......KrKKKK',
    '.......KKrrr',
    '.........KKK',
]
ART = [row + row[::-1] for row in LEFT]
COLOURS = {
    'K': (0, 0, 0),          # outline
    'H': (255, 255, 170),    # horn
    'R': (255, 0, 0),        # skin: bright, so it stays light in the greyscale launcher
    'r': (170, 0, 0),        # skin shadow
    'Y': (255, 255, 0),      # eyes
    'W': (255, 255, 255),    # teeth
}


def png(path, width, height, pixel):
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for x in range(width):
            c = pixel(x, y)
            raw.extend((*COLOURS[c], 255) if c in COLOURS else (0, 0, 0, 0))

    def chunk(name, data):
        return struct.pack('>I', len(data)) + name + data + struct.pack('>I', zlib.crc32(name + data) & 0xffffffff)
    path.parent.mkdir(exist_ok=True)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(bytes(raw))) + chunk(b'IEND', b''))


def art(x, y):
    return ART[y][x] if 0 <= x < 24 and 0 <= y < 24 else '.'


assert all(len(row) == 24 for row in ART) and len(ART) == 24
png(ROOT / 'resources/menu_icon.png', 25, 25, art)   # 24 x 24 art, 1 px margin right/bottom
for scale in (2, 6):
    png(ROOT / f'store/icon-{24 * scale}.png', 24 * scale, 24 * scale, lambda x, y: art(x // scale, y // scale))
print('icons: resources/menu_icon.png, store/icon-48.png, store/icon-144.png')
