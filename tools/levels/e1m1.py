"""E1M1: Hangar Gate.

Entry room -> door -> brown corridor with a window onto the courtyard ->
stairs up -> outdoor courtyard around a nukage pool -> door -> octagonal
computer room -> door -> exit alcove.
"""
from doommap import Map, NUKAGE, SKY, AMBUSH, SKILLS, UNPEG_BOTTOM

# Doomednums used below.
PLAYER, ZOMBIE, SHOTGUY, IMP = 1, 3004, 9, 3001
SHOTGUN, SHELLS, CLIP, STIM, MEDI, ARMOR, BONUS = 2001, 2008, 2007, 2011, 2012, 2018, 2014
LAMP, TECHCOL = 2028, 48

GREY, DARK, BROWNF, STONEF, TEAL = 21, 20, 36, 42, 25


def build():
    m = Map('E1M1')

    # Entry room.
    m.rect(0, 0, 384, 320, floor=0, ceil=128, floor_col=DARK, ceil_col=GREY, light=176, wall='WALL')
    m.door(160, 320, 224, 336, axis='y')

    # Corridor running north, then stairs east up into the courtyard.
    m.rect(128, 336, 256, 768, floor=0, ceil=112, floor_col=BROWNF, ceil_col=DARK, light=144, wall='BROWN')
    for i, x in enumerate((256, 288, 320)):
        m.rect(x, 640, x + 32, 768, floor=16 * (i + 1), ceil=176, floor_col=BROWNF, ceil_col=DARK,
               light=160, wall='BROWN')
        m.edge((x, 640), (x, 768), lower='STEP')
    # Window from the corridor into the courtyard (sill 48, lintel 96).
    m.rect(256, 448, 352, 544, floor=48, ceil=96, floor_col=GREY, ceil_col=GREY, light=160, wall='METAL')

    # Outdoor courtyard with a nukage pool and a support pillar.
    pool = [(560, 448), (560, 672), (800, 672), (800, 448)]
    m.sector([(352, 256), (352, 896), (1024, 896), (1024, 256)], floor=64, ceil=256,
             floor_col=STONEF, ceil_col=SKY, light=208, wall='STONE',
             holes=[[(864, 352), (864, 416), (928, 416), (928, 352)], pool])
    m.edge((864, 352), (864, 416), mid='SUPPORT')
    m.edge((864, 416), (928, 416), mid='SUPPORT')
    m.edge((928, 416), (928, 352), mid='SUPPORT')
    m.edge((928, 352), (864, 352), mid='SUPPORT')
    m.sector(pool, floor=40, ceil=256, floor_col=NUKAGE, ceil_col=SKY, light=208,
             special=7, wall='NUKEDGE')

    # Computer room: octagon with a door on the west and north.
    m.door(1024, 544, 1040, 608, axis='x', floor=64)
    m.sector([(1040, 480), (1040, 672), (1072, 704), (1328, 704), (1360, 672), (1360, 480),
              (1328, 448), (1072, 448)], floor=64, ceil=192, floor_col=DARK, ceil_col=GREY,
             light=176, wall='GRAY')
    for a, b in (((1072, 704), (1168, 704)), ((1232, 704), (1328, 704)), ((1360, 480), (1360, 672))):
        m.edge(a, b, mid='COMP')
    for a, b in (((1040, 672), (1072, 704)), ((1328, 704), (1360, 672)),
                 ((1360, 480), (1328, 448)), ((1072, 448), (1040, 480))):
        m.edge(a, b, mid='LITE')
    m.door(1168, 704, 1232, 720, axis='y', floor=64)

    # Exit room with the switch in an alcove.
    m.sector([(1136, 720), (1136, 848), (1168, 848), (1168, 864), (1232, 864), (1232, 848),
              (1264, 848), (1264, 720)], floor=64, ceil=136, floor_col=TEAL, ceil_col=GREY,
             light=192, wall='WALL')
    m.edge((1168, 864), (1232, 864), special=11, mid='SW1EXIT')

    # Things.
    m.thing(192, 64, PLAYER, angle=90)
    m.thing(64, 272, LAMP)
    m.thing(320, 272, LAMP)
    m.thing(96, 64, CLIP)
    m.thing(192, 520, ZOMBIE, angle=270)
    m.thing(192, 700, ZOMBIE, angle=270, flags=SKILLS | AMBUSH)
    m.thing(160, 736, SHOTGUN)
    m.thing(224, 736, SHELLS)
    m.thing(460, 330, IMP, angle=180)
    m.thing(480, 820, ZOMBIE, angle=270)
    m.thing(680, 800, IMP, angle=270)
    m.thing(940, 780, SHOTGUY, angle=180)
    m.thing(960, 300, IMP, angle=90, flags=SKILLS | AMBUSH)
    m.thing(420, 300, STIM)
    m.thing(980, 860, SHELLS)
    m.thing(896, 460, ARMOR)
    m.thing(1100, 480, TECHCOL)
    m.thing(1300, 480, TECHCOL)
    m.thing(1200, 520, SHOTGUY, angle=180, flags=SKILLS | AMBUSH)
    m.thing(1300, 660, ZOMBIE, angle=180)
    m.thing(1110, 670, IMP, angle=0)
    m.thing(1200, 600, MEDI)
    m.thing(1160, 760, BONUS)
    m.thing(1240, 760, BONUS)
    return m
