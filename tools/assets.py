"""Art manifest shared by build_wad.py and import_freedoom.py.

SPRITES lists the sprite prefixes packed into the resource; the frames of each
come from the engine state table (src/doom/info.c). Only rotation 0 is packed.
PATCHES describes the wall patches imported from Freedoom; TEXTURES maps map
texture names to those patches.
"""
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# PLAY is omitted on purpose: in single player the view sits inside the player
# mobj, which R_ProjectSprite culls before touching its (missing) frames.
SPRITES = (
    'PUNG PISG PISF SHTG SHTF CHGG CHGF '          # weapons
    'POSS SPOS TROO SARG '                         # monsters
    'BLUD PUFF BAL1 POL5 '                         # effects, crushed corpse
    'CLIP AMMO SHEL SBOX MGUN SHOT '               # ammo and weapons
    'STIM MEDI BON1 BON2 ARM1 ARM2 '               # health and armour
    'BKEY RKEY YKEY '                              # keycards
    'COLU ELEC CAND CBRA'                          # decorations
).split()

WEAPON_SPRITES = {'PUNG', 'PISG', 'PISF', 'SHTG', 'SHTF', 'CHGG', 'CHGF'}

# Patch lump -> (Freedoom texture, output width, overlay). Output height is
# always 128; shorter sources are tiled vertically. overlay=(texture, x, y)
# pastes a second texture on top (switch panels on a wall).
PATCHES = {
    'PWALL':   ('STARTAN3', 64, None),
    'PBROWN':  ('BROWN1', 64, None),
    'PGRAY':   ('GRAY4', 64, None),
    'PCOMP':   ('COMPSTA1', 64, None),
    'PSTONE':  ('STONE2', 64, None),
    'PMETAL':  ('METAL1', 64, None),
    'PSUPPORT': ('SUPPORT2', 64, None),
    'PLITE':   ('LITE3', 32, None),
    'PDOOR':   ('BIGDOOR2', 64, None),
    'PDTRAK':  ('DOORTRAK', 8, None),
    'PDBLUE':  ('DOORBLU', 8, None),
    'PDRED':   ('DOORRED', 8, None),
    'PDYEL':   ('DOORYEL', 8, None),
    'PNUKE':   ('NUKE24', 64, None),
    'PSTEP':   ('STEP6', 32, None),
    'PSWEX1':  ('STARTAN3', 64, ('SW1EXIT', 16, 28)),
    'PSWEX2':  ('STARTAN3', 64, ('SW2EXIT', 16, 28)),
    'PSWCM1':  ('SW1COMP', 64, None),
    'PSWCM2':  ('SW2COMP', 64, None),
}

# Map texture name -> patch lump. Texture 0 is reserved for "no texture".
TEXTURES = {
    'VOID': 'PWALL',
    'WALL': 'PWALL', 'BROWN': 'PBROWN', 'GRAY': 'PGRAY', 'COMP': 'PCOMP',
    'STONE': 'PSTONE', 'METAL': 'PMETAL', 'SUPPORT': 'PSUPPORT', 'LITE': 'PLITE',
    'DOOR': 'PDOOR', 'DOORTRAK': 'PDTRAK', 'DOORBLU': 'PDBLUE', 'DOORRED': 'PDRED',
    'DOORYEL': 'PDYEL', 'NUKEDGE': 'PNUKE', 'STEP': 'PSTEP',
    # Required by the engine at startup (r_sky.c, p_spec.c); never drawn.
    'SKY1': 'PWALL', 'SLADRIP1': 'PWALL', 'SLADRIP2': 'PWALL', 'SLADRIP3': 'PWALL',
}
SWITCH_PATCHES = {'SW1EXIT': 'PSWEX1', 'SW2EXIT': 'PSWEX2', 'SW1COMP': 'PSWCM1', 'SW2COMP': 'PSWCM2'}


def texture_names():
    """All texture names in index order: design textures, then every switch
    texture the engine's switch list requires."""
    switches = re.findall(r'"(SW[12][A-Z0-9]+)"', (ROOT / 'src/doom/p_switch.c').read_text())
    names = list(TEXTURES) + [s for s in switches if s not in TEXTURES]
    return names


def texture_patch(name):
    if name in TEXTURES:
        return TEXTURES[name]
    if name in SWITCH_PATCHES:
        return SWITCH_PATCHES[name]
    return 'PSWEX1' if name.startswith('SW1') else 'PSWEX2'   # unused switch pairs


def sprite_frames():
    """Lump names (e.g. TROOA0) for every frame the state table uses."""
    states = (ROOT / 'src/doom/info.c').read_text(encoding='utf-8')
    names = []
    for prefix in SPRITES:
        frames = [int(n) & 32767 for n in re.findall(r'\{SPR_' + prefix + r',\s*(\d+)', states)]
        if not frames:
            raise ValueError(f'sprite {prefix} is not used by any engine state')
        names += [prefix + chr(65 + f) + '0' for f in range(max(frames) + 1)]
    return names


def patch(width, height, left, top, pixel):
    """Doom patch from pixel(x, y) -> colour code or None (transparent).
    Identical columns are stored once."""
    out = bytearray(struct.pack('<hhhh', width, height, left, top) + bytes(width * 4))
    columns = {}
    for x in range(width):
        column = bytearray()
        y = 0
        while y < height:
            if pixel(x, y) is None:
                y += 1
                continue
            start, row = y, []
            while y < height and pixel(x, y) is not None and len(row) < 128:
                row.append(pixel(x, y))
                y += 1
            column.extend(bytes([start, len(row), 0] + row + [0]))
        column.append(255)
        key = bytes(column)
        if key not in columns:
            columns[key] = len(out)
            out.extend(column)
        struct.pack_into('<I', out, 8 + x * 4, columns[key])
    return bytes(out)


def read_lumps(data):
    n, off = struct.unpack_from('<ii', data, 4)
    result = {}
    for i in range(n):
        pos, size, name = struct.unpack_from('<ii8s', data, off + 16 * i)
        assert 0 <= pos <= len(data) and 0 <= size <= len(data) - pos
        result[name.rstrip(b'\0').decode()] = data[pos:pos + size]
    return result
