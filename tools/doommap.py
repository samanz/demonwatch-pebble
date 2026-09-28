"""Describe Doom maps as sector polygons and write them as vanilla Doom WADs.

A sector is a polygon (list of (x, y) map points) plus heights, flat colours,
light, special, tag and a default wall texture. Holes (pillars, pools) are
extra polygons inside a sector. Edges shared by two sectors become two-sided
lines; a vertex lying on another polygon's edge splits that edge, so rooms may
meet corridors part-way along a wall. Per-edge overrides set specials,
textures and flags.

Flats: this engine draws floors and ceilings as solid colours, so a sector's
floor/ceiling is a 6-bit Pebble colour (0-63, 0b00RRGGBB), SKY or NUKAGE. They
are written as flat names "C<n>", "F_SKY1" and "NUKAGE1" so the WAD still opens
in ordinary Doom editors; tools/convert_map.py turns them back into codes.

The output is a standard map (THINGS, LINEDEFS, SIDEDEFS, VERTEXES, SECTORS)
ready for a node builder.
"""
import struct
from dataclasses import dataclass, field

SKY = 'F_SKY1'
NUKAGE = 'NUKAGE1'

# Line flags (vanilla).
BLOCKING, BLOCKMONSTERS, TWOSIDED, UNPEG_TOP, UNPEG_BOTTOM, SECRET = 1, 2, 4, 8, 16, 32

# Thing option bits: easy/medium/hard skills, ambush. Easy covers Doom's
# first two skills (the game's Easy is sk_baby), NORMAL is sk_medium.
EASY, NORMAL, HARD, AMBUSH = 1, 2, 4, 8
SKILLS = EASY | NORMAL | HARD   # every difficulty
NH = NORMAL | HARD             # Normal and Hard only


def colour(r, g, b):
    """Pebble colour code from 0-3 channel levels."""
    return r * 16 + g * 4 + b


def flat(value):
    return value if isinstance(value, str) else f'C{value}'


@dataclass
class Sector:
    poly: list
    floor: int = 0
    ceil: int = 128
    floor_col: object = 21
    ceil_col: object = 21
    light: int = 160
    special: int = 0
    tag: int = 0
    wall: str = 'WALL'
    holes: list = field(default_factory=list)
    index: int = -1


@dataclass
class Edge:
    a: tuple
    b: tuple
    sector: Sector


class Map:
    def __init__(self, name):
        self.name = name
        self.sectors = []
        self.things = []
        self.overrides = {}   # frozenset({a, b}) -> dict
        self.door_faces = {}  # frozenset({a, b}) -> door sector

    # ----- authoring -------------------------------------------------------
    def sector(self, poly, **kw):
        s = Sector([tuple(p) for p in poly], **kw)
        s.holes = [[tuple(p) for p in h] for h in s.holes]
        s.index = len(self.sectors)
        self.sectors.append(s)
        return s

    def rect(self, x1, y1, x2, y2, **kw):
        return self.sector([(x1, y1), (x1, y2), (x2, y2), (x2, y1)], **kw)

    def edge(self, a, b, **props):
        """Override line properties for the edge between points a and b.

        props: special, tag, flags (added), upper/lower/mid (texture on every
        side), front_*/back_* textures are not needed in practice.
        """
        self.overrides.setdefault(frozenset((tuple(a), tuple(b))), {}).update(props)

    def thing(self, x, y, kind, angle=0, flags=SKILLS):
        self.things.append((x, y, angle, kind, flags))

    def door(self, x1, y1, x2, y2, axis, special=1, tag=0, texture='DOOR',
             track='DOORTRAK', floor=0, secret=False, **kw):
        """A door sector spanning a doorway. axis='x' means the player walks
        along x through it (faces are the x1 and x2 edges). A secret door
        uses the surrounding wall texture for texture/track and gets Doom's
        secret line flag."""
        d = self.rect(x1, y1, x2, y2, floor=floor, ceil=floor, tag=tag, wall=track, **kw)
        faces = [((x1, y1), (x1, y2)), ((x2, y1), (x2, y2))] if axis == 'x' else \
                [((x1, y1), (x2, y1)), ((x1, y2), (x2, y2))]
        tracks = [((x1, y1), (x2, y1)), ((x1, y2), (x2, y2))] if axis == 'x' else \
                 [((x1, y1), (x1, y2)), ((x2, y1), (x2, y2))]
        for a, b in faces:
            self.edge(a, b, special=special, upper=texture, flags=SECRET if secret else 0)
            self.door_faces[frozenset((a, b))] = d
        for a, b in tracks:
            self.edge(a, b, flags=UNPEG_BOTTOM)
        return d

    # ----- building ----------------------------------------------------------
    @staticmethod
    def _area(poly):
        return sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(poly, poly[1:] + poly[:1])) / 2

    def _loops(self):
        """(polygon, sector) pairs oriented so the sector is on the right."""
        for s in self.sectors:
            outer = s.poly if self._area(s.poly) < 0 else s.poly[::-1]
            yield outer, s
            for h in s.holes:   # the sector lies outside a hole
                yield (h if self._area(h) > 0 else h[::-1]), s

    def build_edges(self):
        loops = list(self._loops())
        points = {p for poly, _ in loops for p in poly}
        edges = []
        for poly, s in loops:
            for a, b in zip(poly, poly[1:] + poly[:1]):
                cuts = [p for p in points if p not in (a, b) and _on_segment(p, a, b)]
                cuts.sort(key=lambda p: (p[0] - a[0]) ** 2 + (p[1] - a[1]) ** 2)
                chain = [a] + cuts + [b]
                edges.extend(Edge(p, q, s) for p, q in zip(chain, chain[1:]))
        return edges

    def lookup(self, table, a, b):
        """Find the override covering sub-edge a-b (overrides may name a
        longer edge that was split)."""
        key = frozenset((a, b))
        if key in table:
            return table[key]
        for k, v in table.items():
            p, q = tuple(k)
            if _on_segment_or_end(a, p, q) and _on_segment_or_end(b, p, q):
                return v
        return None

    def write_wad(self):
        edges = self.build_edges()
        by_dir = {}
        for e in edges:
            if (e.a, e.b) in by_dir:
                raise ValueError(f'{self.name}: sectors {by_dir[e.a, e.b].sector.index} and '
                                 f'{e.sector.index} overlap along {e.a}-{e.b}')
            by_dir[e.a, e.b] = e
        verts, vindex = [], {}

        def v(p):
            if p not in vindex:
                vindex[p] = len(verts)
                verts.append(p)
            return vindex[p]

        lines, sides, done = [], [], set()
        for e in edges:
            if (e.a, e.b) in done:
                continue
            other = by_dir.get((e.b, e.a))
            done.add((e.a, e.b))
            front, back, a, b = e.sector, None, e.a, e.b
            if other:
                done.add((e.b, e.a))
                back = other.sector
                door = self.lookup(self.door_faces, a, b)
                if door is not None and front is door:   # specials act on the back sector
                    front, back, a, b = back, front, b, a
            props = self.lookup(self.overrides, a, b) or {}
            flags = props.get('flags', 0) | (TWOSIDED if back else BLOCKING)
            sidenums = []
            for mine, theirs in ((front, back), (back, front)):
                if mine is None:
                    sidenums.append(0xFFFF)
                    continue
                upper = lower = mid = '-'
                if theirs is None:
                    mid = props.get('mid', mine.wall)
                else:
                    if theirs.ceil < mine.ceil and not (theirs.ceil_col == SKY and mine.ceil_col == SKY):
                        upper = props.get('upper', mine.wall)
                    if theirs.floor > mine.floor:
                        lower = props.get('lower', mine.wall)
                    if 'mid2' in props:
                        mid = props['mid2']
                if 'upper' in props and theirs is not None and upper == '-' and theirs.ceil <= mine.ceil:
                    upper = props['upper']   # door faces: texture must exist when the door opens
                sidenums.append(len(sides))
                sides.append((props.get('xoff', 0), props.get('yoff', 0), upper, lower, mid, mine.index))
            lines.append((v(a), v(b), flags, props.get('special', 0), props.get('tag', 0), *sidenums))

        out = []
        out.append(('THINGS', b''.join(struct.pack('<hhhhh', *t) for t in self.things)))
        out.append(('LINEDEFS', b''.join(struct.pack('<HHHHHHH', *l) for l in lines)))
        out.append(('SIDEDEFS', b''.join(
            struct.pack('<hh8s8s8sh', xo, yo, _n(u), _n(lo), _n(m), s) for xo, yo, u, lo, m, s in sides)))
        out.append(('VERTEXES', b''.join(struct.pack('<hh', *p) for p in verts)))
        out.append(('SECTORS', b''.join(
            struct.pack('<hh8s8shhh', s.floor, s.ceil, _n(flat(s.floor_col)), _n(flat(s.ceil_col)),
                        s.light, s.special, s.tag) for s in self.sectors)))
        return wad([(self.name, b'')] + out)


# Line specials the route check understands.
DOOR_SPECIALS = {1: None, 31: None, 26: 'blue', 32: 'blue', 27: 'yellow', 34: 'yellow', 28: 'red', 33: 'red'}
LIFT_SPECIALS = {62, 88, 10, 21, 120, 121, 123}
EXIT_SPECIALS = {11, 51, 52}
BOSS_TAG = 666   # sectors with this tag lower to their lowest neighbour when all Barons die
KEY_THINGS = {5: 'blue', 40: 'blue', 6: 'yellow', 39: 'yellow', 13: 'red', 38: 'red'}
# Thing radius by doomednum; anything else is treated as a 20-unit item.
RADIUS = {1: 16, 3004: 20, 9: 20, 3001: 20, 3002: 30, 58: 30, 3003: 24, 2035: 10}
MAXSTEP, PLAYER_HEIGHT, PLAYER_WIDTH = 24, 56, 32


def check(m):
    """Design checks: every thing inside the map and clear of walls, and the
    exit reachable from the player start. Returns a report string; raises
    ValueError on failure."""
    edges = m.build_edges()
    by_dir = {(e.a, e.b): e for e in edges}
    solid = [(e.a, e.b) for e in edges if (e.b, e.a) not in by_dir]

    def sector_at(x, y):
        found = [s for s in m.sectors
                 if _inside(s.poly, x, y) and not any(_inside(h, x, y) for h in s.holes)]
        return min(found, key=lambda s: abs(Map._area(s.poly)), default=None)

    problems = []
    for x, y, _angle, kind, _flags in m.things:
        s = sector_at(x, y)
        if s is None:
            problems.append(f'thing {kind} at ({x},{y}) is outside the map')
            continue
        r = RADIUS.get(kind, 20)
        near = min((_dist_to_segment((x, y), a, b) for a, b in solid), default=1e9)
        if kind in RADIUS and near < r:
            problems.append(f'thing {kind} at ({x},{y}) overlaps a wall (clearance {near:.0f} < {r})')

    # Sector graph for the route check.
    lifts = {props.get('tag') for props in m.overrides.values() if props.get('special') in LIFT_SPECIALS}
    lifts.add(BOSS_TAG)   # lowered by A_BossDeath when every Baron is dead
    links = {s.index: [] for s in m.sectors}
    exits = set()
    for e in edges:
        props = m.lookup(m.overrides, e.a, e.b) or {}
        length = ((e.a[0] - e.b[0]) ** 2 + (e.a[1] - e.b[1]) ** 2) ** 0.5
        if props.get('special') in EXIT_SPECIALS:
            exits.add(e.sector.index)
        other = by_dir.get((e.b, e.a))
        if other is None or length < PLAYER_WIDTH:
            continue
        a, b = e.sector, other.sector
        door = m.lookup(m.door_faces, e.a, e.b)
        key = DOOR_SPECIALS.get(props.get('special'), None) if door is not None else None
        links[a.index].append((b, door is not None, key))

    def passable(a, b, through_door):
        if through_door or b.tag in lifts or a.tag in lifts:
            return True
        floor_b = b.floor
        return floor_b - a.floor <= MAXSTEP and min(a.ceil, b.ceil) - max(a.floor, floor_b) >= PLAYER_HEIGHT

    start = next((t for t in m.things if t[3] == 1), None)
    if start is None:
        raise ValueError(f'{m.name}: no player start')
    keys, reached = set(), set()
    first = sector_at(start[0], start[1])
    changed = True
    while changed:
        changed = False
        frontier = [first]
        seen = {first.index}
        while frontier:
            s = frontier.pop()
            for t, through_door, key in links[s.index]:
                if t.index in seen or (key and key not in keys):
                    continue
                if passable(s, t, through_door):
                    seen.add(t.index)
                    frontier.append(t)
        if seen != reached:
            reached, changed = seen, True
        for x, y, _a, kind, _f in m.things:
            k = KEY_THINGS.get(kind)
            if k and k not in keys and sector_at(x, y) and sector_at(x, y).index in reached:
                keys.add(k)
                changed = True
    if not exits:
        problems.append('no exit line')
    elif not exits & reached:
        problems.append(f'exit not reachable (reached sectors {sorted(reached)}, keys {sorted(keys)})')
    monsters = [t for t in m.things if t[3] in (3004, 9, 3001, 3002, 58, 3003)]
    per_skill = '/'.join(str(sum(1 for t in monsters if t[4] & bit)) for bit in (EASY, NORMAL, HARD))
    unreachable = [t for t in m.things if sector_at(t[0], t[1]) and sector_at(t[0], t[1]).index not in reached]
    if problems:
        raise ValueError(f'{m.name}: ' + '; '.join(problems))
    return (f'{len(reached)}/{len(m.sectors)} sectors reachable, keys {sorted(keys) or "none"}, '
            f'monsters easy/normal/hard {per_skill}, {len(unreachable)} things out of reach')


def _inside(poly, x, y):
    inside = False
    for (x1, y1), (x2, y2) in zip(poly, poly[1:] + poly[:1]):
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
            inside = not inside
    return inside


def _dist_to_segment(p, a, b):
    (px, py), (ax, ay), (bx, by) = p, a, b
    dx, dy = bx - ax, by - ay
    t = max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return ((px - ax - t * dx) ** 2 + (py - ay - t * dy) ** 2) ** 0.5


def _n(name):
    return name.upper().encode()[:8]


def _on_segment(p, a, b):
    """p strictly inside segment a-b (integer points)."""
    cross = (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])
    if cross:
        return False
    dot = (p[0] - a[0]) * (b[0] - a[0]) + (p[1] - a[1]) * (b[1] - a[1])
    return 0 < dot < (b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2


def _on_segment_or_end(p, a, b):
    return p in (a, b) or _on_segment(p, a, b)


def wad(lumps, kind=b'PWAD'):
    body = bytearray()
    directory = []
    for name, data in lumps:
        directory.append((12 + len(body), len(data), name))
        body.extend(data)
    out = bytearray(kind + struct.pack('<ii', len(lumps), 12 + len(body)))
    out.extend(body)
    for pos, size, name in directory:
        out.extend(struct.pack('<ii8s', pos, size, name.encode()))
    return bytes(out)


def read_wad(data):
    n, off = struct.unpack_from('<ii', data, 4)
    result = []
    for i in range(n):
        pos, size = struct.unpack_from('<ii', data, off + 16 * i)
        name = data[off + 16 * i + 8: off + 16 * i + 16].rstrip(b'\0').decode('latin1')
        result.append((name, data[pos:pos + size]))
    return result
