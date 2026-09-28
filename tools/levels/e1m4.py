"""E1M4: Reactor Core (finale).

Supply room -> door -> octagonal arena with four pillars and the Baron of Hell
(two on Hard) -> when every Baron is dead, the stone block sealing the north
passage sinks (tag 666, A_BossDeath) -> exit room.
"""
from doommap import Map, BOSS_TAG, HARD, NH, SKILLS

PLAYER, ZOMBIE, SHOTGUY, IMP, BARON = 1, 3004, 9, 3001, 3003
SHELLS, SHELLBOX, AMMOBOX, MEDI, STIM, BLUEARMOR = 2008, 2049, 2048, 2012, 2011, 2019
CANDELABRA, TECHCOL = 35, 48

GREY, DARK, RUST, TEAL = 21, 20, 37, 25


def build():
    m = Map('E1M4')

    # Supply room.
    m.rect(0, 0, 256, 256, floor=0, ceil=128, floor_col=DARK, ceil_col=GREY, light=176, wall='METAL')
    m.door(96, 256, 160, 272, axis='y')

    # Arena: octagon with four pillars for cover.
    pillars = [[(-96, 464), (-96, 528), (-32, 528), (-32, 464)],
               [(288, 464), (288, 528), (352, 528), (352, 464)],
               [(-96, 752), (-96, 816), (-32, 816), (-32, 752)],
               [(288, 752), (288, 816), (352, 816), (352, 752)]]
    m.sector([(-160, 272), (-256, 368), (-256, 912), (-160, 1008), (416, 1008), (512, 912),
              (512, 368), (416, 272)], floor=0, ceil=224, floor_col=RUST, ceil_col=DARK,
             light=144, special=8, wall='STONE', holes=pillars)
    for p in pillars:
        for a, b in zip(p, p[1:] + p[:1]):
            m.edge(a, b, mid='SUPPORT')

    # The seal: a stone block filling the passage, lowered when the Barons die.
    m.rect(96, 1008, 160, 1024, floor=224, ceil=224, floor_col=RUST, ceil_col=DARK,
           light=144, tag=BOSS_TAG, wall='STONE')

    # Exit room.
    m.sector([(64, 1024), (64, 1152), (96, 1152), (96, 1168), (160, 1168), (160, 1152),
              (192, 1152), (192, 1024)], floor=0, ceil=128, floor_col=TEAL, ceil_col=GREY,
             light=192, wall='WALL')
    m.edge((96, 1168), (160, 1168), special=11, mid='SW1EXIT')

    # Things.
    m.thing(128, 48, PLAYER, angle=90)
    m.thing(48, 200, SHELLBOX)
    m.thing(208, 200, SHELLBOX)
    m.thing(128, 180, BLUEARMOR)
    m.thing(48, 48, MEDI)
    m.thing(208, 48, AMMOBOX)
    m.thing(128, 880, BARON, angle=270)
    m.thing(-100, 880, BARON, angle=270, flags=HARD)
    m.thing(-200, 620, IMP, angle=0, flags=NH)
    m.thing(460, 620, IMP, angle=180, flags=NH)
    m.thing(128, 360, ZOMBIE, angle=270, flags=HARD)
    m.thing(-160, 400, SHELLS)
    m.thing(420, 400, SHELLS)
    m.thing(-64, 640, MEDI)
    m.thing(320, 640, STIM)
    m.thing(-200, 950, SHELLBOX)
    m.thing(460, 950, SHELLBOX)
    m.thing(-200, 330, CANDELABRA)
    m.thing(460, 330, CANDELABRA)
    m.thing(96, 1100, TECHCOL)
    m.thing(160, 1100, TECHCOL)
    return m
