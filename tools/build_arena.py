"""Build an original five-sector arena and procedural artwork.

Optional converted Freedoom sprites are loaded from the local art pack. Data is in Doom64KB's little-endian format.
Generated artwork and geometry may be redistributed under CC0-1.0.
"""
import math
import re
import struct as st
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def patch(width, height, left, top, pixel):
    out = bytearray(st.pack('<hhhh', width, height, left, top) + bytes(width * 4))
    columns={}
    for x in range(width):
        column=bytearray()
        y=0
        while y<height:
            if pixel(x,y) is None:
                y+=1; continue
            start=y; row=[]
            while y<height and pixel(x,y) is not None:
                row.append(pixel(x,y)); y+=1
            column.extend(bytes([start,len(row),0]+row+[0]))
        column.append(255)
        key=bytes(column)
        if key not in columns:
            columns[key]=len(out);out.extend(column)
        st.pack_into('<I',out,8+x*4,columns[key])
    return bytes(out)


def build():
    lumps = []
    def add(name, data=b''):
        lumps.append((name, data))
        return len(lumps) - 1

    add('COLORMAP', bytes(range(256)))
    add('E1M1')
    things = [
        # Sector 0: Starting Armory
        (128, 256, 1, 0),     # Player 1 start
        (240, 256, 2018, 0),  # Green Armor
        (280, 256, 2001, 0),  # Shotgun
        (350, 256, 2008, 0),  # Shells
        (420, 380, 2011, 0),  # Stimpack

        # Sector 2: Main Courtyard
        (740, 200, 3004, 4),  # Zombieman
        (740, 320, 3004, 4),  # Zombieman
        (920, 256, 9, 4),     # Shotgun Guy
        (1040, 380, 3001, 4), # Imp
        (640, 120, 2011, 0),  # Stimpack
        (820, 420, 2008, 0),  # Shells
        (1020, 140, 2007, 0), # Clip

        # Sector 4: Tech Lab / Reactor Chamber
        (1360, 160, 3001, 4), # Imp
        (1360, 350, 3001, 4), # Imp
        (1520, 256, 9, 4),    # Shotgun Guy
        (1680, 200, 3004, 4), # Zombieman
        (1680, 320, 3004, 4), # Zombieman
        (1280, 400, 2011, 0), # Stimpack
        (1440, 100, 2018, 0), # Green Armor
        (1620, 420, 2008, 0), # Shells

        # Sector 6: Exit Sanctum
        (1980, 160, 3001, 4), # Imp
        (1980, 352, 3001, 4), # Imp
        (2140, 256, 9, 4),    # Shotgun Guy
        (2260, 200, 3001, 4), # Imp
        (2260, 312, 3001, 4), # Imp
        (1920, 380, 2011, 0), # Stimpack
        (2100, 120, 2008, 0), # Shells
        (2280, 400, 2011, 0), # Stimpack
    ]
    add('THINGS', b''.join(st.pack('<hhhbb', x, y, kind, angle, 15 if x>1152 and kind in (3001,3004,9) else 7)
                          for x, y, kind, angle in things))
    # Clockwise cells; the front sector lies to the right of each edge.
    cells = [
        (0, 512, 0, 512),       # Sector 0: Starting Armory
        (512, 576, 192, 320),   # Sector 1: Airlock Door 1
        (576, 1152, 0, 512),    # Sector 2: Main Courtyard
        (1152, 1216, 192, 320), # Sector 3: Security Door 2
        (1216, 1792, 0, 512),   # Sector 4: Tech Lab / Reactor Chamber
        (1792, 1856, 192, 320), # Sector 5: Blast Door 3
        (1856, 2368, 0, 512),   # Sector 6: Exit Sanctum
    ]
    exit_x = cells[-1][1]
    lines, sides, segs, subsectors = [], [], [], []
    edges = {}
    for sector, (lx, hx, ly, hy) in enumerate(cells):
        firstseg = len(segs)
        if sector % 2 == 1:
            verts = [(lx, ly), (lx, hy), (hx, hy), (hx, ly)]
        else:
            verts = [(lx, ly)]
            if sector > 0:
                verts.extend([(lx, 192), (lx, 320)])
            verts.append((lx, hy))
            verts.append((hx, hy))
            if sector < len(cells) - 1:
                verts.extend([(hx, 320), (hx, 192)])
            verts.append((hx, ly))
        nverts = len(verts)
        for a, b in zip(verts, verts[1:] + verts[:1]):
            side = len(sides)
            sides.append([sector, False])
            if (b, a) in edges:
                num = edges[b, a]
                lines[num][3] = side
                sides[lines[num][2]][1] = sides[side][1] = True
            else:
                num = len(lines)
                edges[a, b] = num
                lines.append([a, b, side, 65535])
            segs.append((a, b, side, num, sector))
        subsectors.append((nverts, firstseg))
    # Manual doors act on the back sector. Both doorway faces must point
    # into the door, never into the adjacent playable room.
    for line in lines:
        if line[3] != 65535 and sides[line[2]][0] % 2:
            line[0],line[1]=line[1],line[0]
            line[2],line[3]=line[3],line[2]
    linebytes = bytearray()
    for i, (a, b, front, back) in enumerate(lines):
        dx, dy = b[0]-a[0], b[1]-a[1]
        portal = back != 65535
        # An exit switch is on the far east wall.
        special = 1 if portal else (11 if a[0] == b[0] == exit_x else 0)
        linebytes.extend(st.pack('<hhhhHhhHHhhhhhBbb', *a, *b, i, dx, dy,
                                front, back, max(a[1], b[1]), min(a[1], b[1]),
                                min(a[0], b[0]), max(a[0], b[0]), 0,
                                4 if portal else 1, 1 if dx == 0 else 0, special))
    add('LINEDEFS', bytes(linebytes))
    add('SIDEDEFS', b''.join(st.pack('<hBbbbB', 0, 0, 2 if portal else 0,
                                   0, 0 if portal else (23 if any(a[0]==b[0]==exit_x and front==side for a,b,front,back in lines) else 1), sector)
                             for side,(sector, portal) in enumerate(sides)))
    segbytes = bytearray()
    for a, b, side, num, sector in segs:
        line = lines[num]
        other = line[3] if line[2] == side else line[2]
        backsector = sides[other][0] if other != 65535 else 255
        angle = round(math.atan2(b[1]-a[1], b[0]-a[0]) * 65536 / math.tau + 16384) & 65535
        segbytes.extend(st.pack('<hhhhhHHHBB', *a, *b, 0, angle, side, num, sector, backsector))
    add('SEGS', bytes(segbytes))
    add('SSECTORS', b''.join(st.pack('<hh', *s) for s in subsectors))
    def node(x, right, left, lo, hi):
        return st.pack('<hhhhhhhhhhhhHH', x, 0, 0, 512,
                       512, 0, x, hi, 512, 0, lo, x, right, left)
    add('NODES', b''.join(node(cells[i][0], 0x8000+i, 0x8000 if i==1 else i-2, 0, cells[i][1]) for i in range(1,len(cells))))
    add('SECTORS', b''.join(st.pack('<hhhhBbh', 0, 0 if i % 2 else 128,
                                    21, 0 if i % 2 else 21, 224, 0, 0) for i in range(len(cells))))
    add('REJECT', bytes((len(cells)**2+7)//8))
    # Every cell references the same small complete line list.
    nx = math.ceil(exit_x / 128) + 1
    ny = 5
    words = [0, 0, nx, ny] + [4 + nx*ny] * (nx*ny) + [0] + list(range(len(lines))) + [-1]
    add('BLOCKMAP', st.pack('<'+'h'*len(words), *words))
    texture_names = ['VOID', 'WALL', 'DOOR', 'SKY1', 'SLADRIP1', 'SLADRIP2', 'SLADRIP3']
    texture_names += re.findall(r'"(SW[12][A-Z0-9]+)"', (ROOT/'src/doom/p_switch.c').read_text())
    # Filled after patch indices are known.
    tex1_id = add('TEXTURE1')
    texp_id = add('TEXTUREP')
    add('TEXHEIGH', st.pack('<'+'h'*len(texture_names), *([128]*len(texture_names))))
    add('P_START')
    # Broad panel shapes survive downsampling onto the watch display.
    def wall_pixel(x,y):
        if x<2 or x>61 or y<3 or y>124: return 1
        if y in (3,4) or x==2: return 26
        if y in range(24,28): return 11
        if y in range(94,99): return 1
        if 12<x<52 and 44<y<80:
            return 1 if y%8<2 else 5
        return 22 if y<94 else 5
    def door_pixel(x,y):
        if x<4 or x>59: return 1
        if y<12 or y>115: return 60 if (x+y)//8%2 else 1
        if 29<x<34: return 1
        return 26 if x%28<5 else 21
    def exit_pixel(x,y):
        if 14<x<50 and 32<y<92:
            if x<18 or x>46 or y<36 or y>88: return 1
            return 12 if y<60 else 4
        return wall_pixel(x,y)
    wall_id = add('PDWALL', patch(64,128,0,0,wall_pixel))
    door_id = add('PDDOOR', patch(64,128,0,0,door_pixel))
    exit_id = add('PDEXIT', patch(64,128,0,0,exit_pixel))
    add('P_END')
    tex1 = bytearray(st.pack('<i', len(texture_names)) + bytes(4*len(texture_names)))
    texp = bytearray(2*len(texture_names))
    for i,name in enumerate(texture_names):
        st.pack_into('<i', tex1, 4+i*4, len(tex1))
        tex1.extend(st.pack('<8shhhhhh', name.encode(), 64, 128, 1, 0, 0, 0))
        st.pack_into('<H', texp, i*2, len(texp))
        texp.extend(st.pack('<HhhBBhhhh', 63, 64, 128, 0, 1, 0, 0,
                            door_id if name == 'DOOR' else exit_id if name.startswith('SW') else wall_id, 64))
    lumps[tex1_id] = ('TEXTURE1', bytes(tex1))
    lumps[texp_id] = ('TEXTUREP', bytes(texp))
    add('S_START')
    # Infer the frame count from engine states, not from copyrighted game data.
    states = (ROOT/'src/doom/info.c').read_text()
    wanted = 'TROO POSS SPOS PUNG PISG PISF SHTG SHTF BLUD PUFF BAL1 ARM1 SHOT SHEL STIM CLIP PLAY POL5'.split()
    for prefix in wanted:
        frames = [int(n) & 32767 for n in re.findall(r'\{SPR_'+prefix+r',\s*(\d+)', states)]
        for f in range(max(frames, default=0)+1):
            weapon = prefix in ('PUNG','PISG','PISF','SHTG','SHTF')
            monster = prefix in ('TROO','POSS','SPOS')
            if weapon:
                w,h = 64,64
                def pixel(x,y):
                    return (63 if x%8<3 else 42) if abs(x-32)<8+y//4 else None
                data = patch(w,h,32,-100,pixel)
            elif monster:
                w,h = 32,56
                dead = f >= 8
                def pixel(x,y,dead=dead,f=f):
                    if dead:
                        return 16 if y>45 and 3<x<29 else None
                    if 8<x<24 and 2<y<15:
                        return 60 if 6<y<10 else 42
                    if 5<x<27 and 16<y<38:
                        return 48 if f in (4,5) else 18
                    if 39<=y<55 and (7<x<13 or 19<x<25):
                        return 21
                    return None
                data=patch(w,h,16,56,pixel)
            else:
                w,h=24,20
                color=12 if prefix=='ARM1' else 60 if prefix in ('BAL1','PUFF') else 63
                data=patch(w,h,12,20,lambda x,y,c=color: c if 3<x<21 and 3<y<18 else None)
            add(prefix+chr(65+f)+'0',data)
    add('S_END')
    art_path=ROOT/'resources/freedoom-art.pbl'
    if art_path.exists():
        from import_freedoom import lumps as read_art
        art=read_art(art_path.read_bytes())
        lumps=[(name,art.get(name,data)) for name,data in lumps]
    out=bytearray(b'IWAD'+st.pack('<ii',len(lumps),0))
    directory=[]
    dedup={}
    for name,data in lumps:
        if data not in dedup:
            out.extend(bytes((-len(out))%4))
            dedup[data]=len(out)
            out.extend(data)
        directory.append((dedup[data],len(data),name.encode()))
    out.extend(bytes((-len(out))%4))
    st.pack_into('<i',out,8,len(out))
    for pos,size,name in directory:
        out.extend(st.pack('<ii8s',pos,size,name))
    (ROOT/'resources/arena.pbl').write_bytes(out)
    print(f'Original arena: {len(out)} bytes, {len(lumps)} lumps, {len(lines)} lines, {len(things)} things')


if __name__ == '__main__':
    build()
