"""E1M3: Command Center.

Raised start room -> stairs down into an outdoor arena with a guarded
pedestal -> west armoury (yellow key on a dais, demons) -> back across the
arena to the yellow door -> control room with pillars -> exit.
"""
from doommap import Map, SKY, AMBUSH, SKILLS

PLAYER, ZOMBIE, SHOTGUY, IMP, DEMON = 1, 3004, 9, 3001, 3002
YELLOWKEY = 6
SHELLS, SHELLBOX, CLIP, AMMOBOX = 2008, 2049, 2007, 2048
STIM, MEDI, ARMOR, BLUEARMOR, BONUS = 2011, 2012, 2018, 2019, 2014
LAMP, TECHCOL, CANDELABRA = 2028, 48, 35

GREY, DARK, STONEF, TEAL, RUST = 21, 20, 42, 25, 37


def build():
    m = Map('E1M3')

    # Raised start room.
    m.rect(0, 0, 320, 256, floor=128, ceil=256, floor_col=DARK, ceil_col=GREY, light=176, wall='WALL')

    # Stairs down into the arena.
    for i, floor in enumerate((104, 80, 56, 32)):
        y = 256 + 32 * i
        m.rect(96, y, 224, y + 32, floor=floor, ceil=256, floor_col=GREY, ceil_col=GREY,
               light=176, wall='METAL')
        m.edge((96, y + 32), (224, y + 32), lower='STEP')

    # Outdoor arena with chamfered corners and a pedestal.
    pedestal = [(96, 608), (96, 736), (224, 736), (224, 608)]
    m.sector([(-256, 448), (-256, 960), (-192, 1024), (512, 1024), (576, 960), (576, 448),
              (512, 384), (-192, 384)], floor=16, ceil=320, floor_col=STONEF, ceil_col=SKY,
             light=208, wall='STONE', holes=[pedestal])
    m.sector(pedestal, floor=64, ceil=320, floor_col=GREY, ceil_col=SKY, light=208, wall='SUPPORT')
    for a, b in zip(pedestal, pedestal[1:] + pedestal[:1]):
        m.edge(a, b, lower='SUPPORT')

    # West armoury: yellow key on a dais.
    m.door(-272, 672, -256, 736, axis='x', floor=16)
    dais = [(-560, 672), (-560, 736), (-464, 736), (-464, 672)]
    m.sector([(-592, 576), (-592, 832), (-272, 832), (-272, 576)], floor=16, ceil=144,
             floor_col=RUST, ceil_col=GREY, light=96, special=8, wall='GRAY', holes=[dais])
    m.sector(dais, floor=24, ceil=144, floor_col=TEAL, ceil_col=GREY, light=192, wall='STEP')

    # Yellow door east into the control room.
    m.door(576, 672, 592, 736, axis='x', special=27, track='DOORYEL', floor=16)
    pillars = [[(720, 624), (720, 656), (752, 656), (752, 624)],
               [(720, 752), (720, 784), (752, 784), (752, 752)]]
    m.sector([(592, 576), (592, 832), (912, 832), (912, 736), (928, 736), (928, 672),
              (912, 672), (912, 576)], floor=16, ceil=160, floor_col=DARK, ceil_col=GREY,
             light=136, wall='COMP', holes=pillars)
    for p in pillars:
        for a, b in zip(p, p[1:] + p[:1]):
            m.edge(a, b, mid='SUPPORT')
    m.edge((928, 672), (928, 736), special=11, mid='SW1EXIT')

    # Things.
    m.thing(160, 40, PLAYER, angle=90)
    m.thing(40, 200, SHELLS)
    m.thing(280, 200, STIM)
    m.thing(40, 40, LAMP)
    m.thing(280, 40, LAMP)
    # Arena.
    m.thing(-120, 520, ZOMBIE, angle=90)
    m.thing(440, 520, ZOMBIE, angle=90)
    m.thing(-150, 900, SHOTGUY, angle=270)
    m.thing(450, 900, SHOTGUY, angle=270)
    m.thing(128, 672, IMP, angle=270)
    m.thing(192, 672, IMP, angle=270)
    m.thing(-40, 960, IMP, angle=270, flags=SKILLS | AMBUSH)
    m.thing(360, 960, IMP, angle=270, flags=SKILLS | AMBUSH)
    m.thing(160, 900, DEMON, angle=270)
    m.thing(-200, 700, MEDI)
    m.thing(520, 700, SHELLBOX)
    m.thing(160, 980, BONUS)
    m.thing(-160, 460, CANDELABRA)
    m.thing(480, 460, CANDELABRA)
    # Armoury.
    m.thing(-512, 704, YELLOWKEY)
    m.thing(-400, 620, DEMON, angle=0)
    m.thing(-400, 790, DEMON, angle=0)
    m.thing(-540, 800, ARMOR)
    m.thing(-330, 620, AMMOBOX)
    # Control room.
    m.thing(820, 640, SHOTGUY, angle=180, flags=SKILLS | AMBUSH)
    m.thing(820, 770, SHOTGUY, angle=180)
    m.thing(660, 790, ZOMBIE, angle=180)
    m.thing(860, 704, MEDI)
    m.thing(640, 610, TECHCOL)
    m.thing(640, 800, TECHCOL)
    return m
