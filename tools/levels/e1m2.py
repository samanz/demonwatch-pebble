"""E1M2: Toxin Refinery.

Start room -> door -> hall split by a nukage channel (bridge in the middle)
-> west storage room with a lift up to a ledge holding the blue key -> back
through the hall to the blue door -> computer room with pillars and stairs
-> door -> exit room.
"""
from doommap import Map, NUKAGE, AMBUSH, SKILLS

PLAYER, ZOMBIE, SHOTGUY, IMP, DEMON = 1, 3004, 9, 3001, 3002
BLUEKEY, SHOTGUN, CHAINGUN = 5, 2001, 2002
SHELLS, SHELLBOX, CLIP, AMMOBOX = 2008, 2049, 2007, 2048
STIM, MEDI, ARMOR, BONUS, ARMBONUS = 2011, 2012, 2018, 2014, 2015
LAMP, TECHCOL = 2028, 48

GREY, DARK, BROWNF, TEAL, RUST = 21, 20, 36, 25, 37
LIFT_TAG = 1


def build():
    m = Map('E1M2')

    # Start room and door north.
    m.rect(256, 0, 512, 384, floor=0, ceil=128, floor_col=DARK, ceil_col=GREY, light=160, wall='WALL')
    m.door(352, 384, 416, 400, axis='y')

    # Hall, split by a nukage channel with a bridge.
    hall = dict(ceil=160, floor_col=BROWNF, ceil_col=DARK, light=176, wall='BROWN')
    m.rect(0, 400, 768, 608, floor=0, **hall)
    m.rect(0, 704, 768, 912, floor=0, **hall)
    m.rect(352, 608, 416, 704, floor=0, ceil=160, floor_col=GREY, ceil_col=DARK, light=176, wall='METAL')
    for x1, x2 in ((0, 352), (416, 768)):
        m.rect(x1, 608, x2, 704, floor=-16, ceil=160, floor_col=NUKAGE, ceil_col=DARK, light=176,
               special=7, wall='NUKEDGE')

    # Storage room west of the hall, with a lift up to the key ledge.
    m.sector([(-256, 704), (-256, 768), (-192, 768), (-192, 848), (-256, 848), (-256, 912),
              (0, 912), (0, 704)], floor=0, ceil=192, floor_col=RUST, ceil_col=GREY, light=144,
             wall='COMP')
    m.rect(-256, 768, -192, 848, floor=96, ceil=192, floor_col=GREY, ceil_col=GREY, light=144,
           tag=LIFT_TAG, wall='METAL')
    for a, b in (((-256, 768), (-192, 768)), ((-192, 768), (-192, 848)), ((-192, 848), (-256, 848)),
                 ((-256, 768), (-256, 848))):
        m.edge(a, b, special=62, tag=LIFT_TAG, lower='SUPPORT')
    m.rect(-448, 704, -256, 912, floor=96, ceil=192, floor_col=TEAL, ceil_col=GREY, light=192, wall='WALL')

    # Blue key door into the computer room.
    m.door(352, 912, 416, 928, axis='y', special=26, track='DOORBLU')

    # Computer room: lower floor with two pillars, stairs up to a raised back.
    m.sector([(192, 928), (192, 1120), (576, 1120), (576, 928)], floor=0, ceil=160,
             floor_col=DARK, ceil_col=GREY, light=176, wall='COMP',
             holes=[[(256, 992), (256, 1024), (288, 1024), (288, 992)],
                    [(480, 992), (480, 1024), (512, 1024), (512, 992)]])
    for pillar in (((256, 992), (256, 1024), (288, 1024), (288, 992)),
                   ((480, 992), (480, 1024), (512, 1024), (512, 992))):
        for a, b in zip(pillar, pillar[1:] + pillar[:1]):
            m.edge(a, b, mid='LITE')
    m.rect(320, 1120, 448, 1152, floor=16, ceil=160, floor_col=GREY, ceil_col=GREY, light=176, wall='METAL')
    m.rect(320, 1152, 448, 1184, floor=32, ceil=160, floor_col=GREY, ceil_col=GREY, light=176, wall='METAL')
    for y in (1120, 1152, 1184):
        m.edge((320, y), (448, y), lower='STEP')
    m.rect(192, 1184, 576, 1248, floor=48, ceil=160, floor_col=DARK, ceil_col=GREY, light=192, wall='COMP')

    # Exit room.
    m.door(352, 1248, 416, 1264, axis='y', floor=48)
    m.sector([(288, 1264), (288, 1392), (352, 1392), (352, 1408), (416, 1408), (416, 1392),
              (480, 1392), (480, 1264)], floor=48, ceil=176, floor_col=TEAL, ceil_col=GREY,
             light=208, wall='WALL')
    m.edge((352, 1408), (416, 1408), special=11, mid='SW1EXIT')

    # Things.
    m.thing(384, 48, PLAYER, angle=90)
    m.thing(300, 300, CLIP)
    m.thing(470, 300, ARMBONUS)
    m.thing(60, 440, LAMP)
    m.thing(708, 440, LAMP)
    m.thing(60, 870, LAMP)
    m.thing(708, 870, LAMP)
    m.thing(200, 520, ZOMBIE, angle=0)
    m.thing(600, 480, ZOMBIE, angle=180)
    m.thing(560, 560, SHOTGUY, angle=270)
    m.thing(384, 450, SHELLS)
    m.thing(200, 820, IMP, angle=270, flags=SKILLS | AMBUSH)
    m.thing(620, 840, IMP, angle=270)
    m.thing(700, 760, STIM)
    m.thing(-120, 870, SHOTGUY, angle=0)
    m.thing(-60, 740, MEDI)
    m.thing(-150, 900 - 40, AMMOBOX)
    m.thing(-100, 800, CHAINGUN)
    m.thing(-352, 808, BLUEKEY)
    m.thing(-400, 750, ARMOR)
    m.thing(-400, 870, ZOMBIE, angle=0, flags=SKILLS | AMBUSH)
    m.thing(300, 1060, DEMON, angle=270)
    m.thing(460, 1070, DEMON, angle=270)
    m.thing(384, 980, SHOTGUY, angle=270, flags=SKILLS | AMBUSH)
    m.thing(240, 1080, SHELLBOX)
    m.thing(530, 1080, MEDI)
    m.thing(230, 1216, IMP, angle=270)
    m.thing(540, 1216, ZOMBIE, angle=270)
    m.thing(224, 960, TECHCOL)
    m.thing(544, 960, TECHCOL)
    m.thing(320, 1300, BONUS)
    m.thing(448, 1300, BONUS)
    return m
