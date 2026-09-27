"""Structural checks of resources/pdoom.pbl: every map's records and indices,
BSP links, textures and patches, and sprite frames for every packed sprite."""
import re
import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from assets import SOUNDS, sprite_frames  # noqa: E402

ORDER = ['THINGS', 'LINEDEFS', 'SIDEDEFS', 'SEGS', 'SSECTORS', 'NODES', 'SECTORS', 'REJECT', 'BLOCKMAP']
SIZES = {'THINGS': 8, 'LINEDEFS': 31, 'SIDEDEFS': 7, 'SEGS': 18, 'SSECTORS': 4, 'NODES': 28, 'SECTORS': 12}


class Assets(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = (ROOT / 'resources/pdoom.pbl').read_bytes()
        magic, count, offset = struct.unpack_from('<4sii', cls.data)
        assert magic == b'IWAD'
        cls.lumps = []
        for i in range(count):
            pos, size, name = struct.unpack_from('<ii8s', cls.data, offset + 16 * i)
            assert 0 <= pos <= len(cls.data) and 0 <= size <= len(cls.data) - pos
            cls.lumps.append((name.rstrip(b'\0').decode(), cls.data[pos:pos + size]))
        cls.named = dict(cls.lumps)
        cls.maps = {}
        for i, (name, _) in enumerate(cls.lumps):
            if re.fullmatch(r'E1M[1-9]', name):
                cls.maps[name] = {k: cls.lumps[i + 1 + j][1] for j, k in enumerate(ORDER)}
                assert [cls.lumps[i + 1 + j][0] for j in range(9)] == ORDER, name

    def test_maps_in_sequence(self):
        self.assertEqual(sorted(self.maps), [f'E1M{i}' for i in range(1, len(self.maps) + 1)])
        self.assertGreaterEqual(len(self.maps), 3)
        self.assertLess(len(self.data) + 4096, 262144)

    def test_map_records_and_indices(self):
        numtextures = len(self.named['TEXHEIGH']) // 2
        for name, m in self.maps.items():
            with self.subTest(map=name):
                for lump, size in SIZES.items():
                    self.assertEqual(len(m[lump]) % size, 0, lump)
                lines, sides = len(m['LINEDEFS']) // 31, len(m['SIDEDEFS']) // 7
                sectors, subsectors = len(m['SECTORS']) // 12, len(m['SSECTORS']) // 4
                segs = len(m['SEGS']) // 18
                self.assertLessEqual(sectors, 255)
                for off in range(0, len(m['LINEDEFS']), 31):
                    s0, s1 = struct.unpack_from('<HH', m['LINEDEFS'], off + 14)
                    self.assertLess(s0, sides)
                    self.assertTrue(s1 == 0xFFFF or s1 < sides)
                for off in range(0, len(m['SIDEDEFS']), 7):
                    self.assertLess(m['SIDEDEFS'][off + 6], sectors)
                    for t in struct.unpack_from('<bbb', m['SIDEDEFS'], off + 3):
                        self.assertTrue(0 <= t < numtextures)
                for off in range(0, len(m['SEGS']), 18):
                    side, line, front, back = struct.unpack_from('<HHBB', m['SEGS'], off + 12)
                    self.assertLess(side, sides)
                    self.assertLess(line, lines)
                    self.assertLess(front, sectors)
                    self.assertTrue(back == 255 or back < sectors)
                for off in range(0, len(m['SSECTORS']), 4):
                    count, first = struct.unpack_from('<hh', m['SSECTORS'], off)
                    self.assertTrue(count > 0 and first + count <= segs)
                nodes = m['NODES']
                for index, off in enumerate(range(0, len(nodes), 28)):
                    for child in struct.unpack_from('<HH', nodes, off + 24):
                        if child & 0x8000:
                            self.assertLess(child & 0x7fff, subsectors)
                        else:
                            self.assertLess(child, index)
                self.assertGreaterEqual(len(m['REJECT']), (sectors * sectors + 7) // 8)
                players = [t for t in range(0, len(m['THINGS']), 8)
                           if struct.unpack_from('<h', m['THINGS'], t + 4)[0] == 1]
                self.assertEqual(len(players), 1)

    def test_patches_and_engine_frames(self):
        index = {n: i for i, (n, _) in enumerate(self.lumps) if n in ('S_START', 'S_END', 'P_START', 'P_END')}
        patches = (self.lumps[index['P_START'] + 1:index['P_END']]
                   + self.lumps[index['S_START'] + 1:index['S_END']])
        for name, patch in patches:
            width, height, _, _ = struct.unpack_from('<hhhh', patch)
            for x in range(width):
                p = struct.unpack_from('<I', patch, 8 + 4 * x)[0]
                while patch[p] != 255:
                    top, length = patch[p:p + 2]
                    self.assertLessEqual(top + length, height, name)
                    self.assertLess(p + length + 4, len(patch), name)
                    p += length + 4
        for frame in sprite_frames():
            self.assertIn(frame, self.named)
        self.assertIn('POL5A0', self.named)  # A closing door can crush a corpse into gibs.

    def test_sounds(self):
        # 8 kHz signed PCM streamed by src/pebble/i_pebbles.c; lumps must fit
        # the uint16 offsets W_ReadLumpRange uses.
        for name in SOUNDS:
            self.assertIn(name, self.named)
            self.assertTrue(0 < len(self.named[name]) < 65536, name)

    def test_texture_references(self):
        table = self.named['TEXTUREP']
        count = len(self.named['TEXHEIGH']) // 2
        self.assertEqual(struct.unpack_from('<i', self.named['TEXTURE1'])[0], count)
        for i in range(count):
            off = struct.unpack_from('<H', table, 2 * i)[0]
            mask, width, height, overlap, npatches = struct.unpack_from('<HhhBB', table, off)
            self.assertEqual(mask, width - 1)
            self.assertEqual(width & (width - 1), 0)
            for j in range(npatches):
                _, _, patch_id, _ = struct.unpack_from('<hhhh', table, off + 8 + 8 * j)
                patch = self.lumps[patch_id][1]
                self.assertEqual(struct.unpack_from('<hh', patch), (width, height))


if __name__ == '__main__':
    unittest.main()
