"""Extract front-view sprites from the pinned Freedoom release into a small art pack.

Usage: python3 tools/import_freedoom.py work/freedoom-0.13.0.zip
Preserves logical sprite sizes and offsets, quantizes RGB222, and shares columns.
"""
import sys
import zipfile
import struct as st
from pathlib import Path
from build_arena import ROOT, patch


def lumps(data):
    n,off=st.unpack_from('<ii',data,4)
    result={}
    for i in range(n):
        pos,size,name=st.unpack_from('<ii8s',data,off+16*i)
        assert 0<=pos<=len(data) and 0<=size<=len(data)-pos
        result[name.rstrip(b'\0').decode()]=data[pos:pos+size]
    return result


def decode(data):
    w,h,left,top=st.unpack_from('<hhhh',data)
    assert 0<w<=320 and 0<h<=254
    pixels=[[None]*w for _ in range(h)]
    for x in range(w):
        p=st.unpack_from('<I',data,8+4*x)[0]
        while data[p]!=255:
            y,n=data[p:p+2]
            for j,index in enumerate(data[p+3:p+3+n]):
                if y+j<h: pixels[y+j][x]=index
            p+=n+4
    return w,h,left,top,pixels


def main(path):
    with zipfile.ZipFile(path) as z:
        wad=lumps(z.read('freedoom-0.13.0/freedoom1.wad'))
        dest=ROOT/'licenses';dest.mkdir(exist_ok=True)
        for source,target in [('COPYING.txt','Freedoom-COPYING.txt'),('CREDITS.txt','Freedoom-CREDITS.txt')]:
            (dest/target).write_bytes(z.read('freedoom-0.13.0/'+source))
    base=lumps((ROOT/'resources/arena.pbl').read_bytes())
    pal=wad['PLAYPAL'][:768]
    colors=[sum(((pal[i*3+c]+42)//85)*(16,4,1)[c] for c in range(3)) for i in range(256)]
    out=bytearray(b'IWAD'+bytes(8));directory=[]
    for name in base:
        if len(name)!=6 or name[5]!='0': continue
        donor=next((n for n in (name,name[:5]+'1') if n in wad),None)
        if donor is None:
            donor=next((n for n in wad if n.startswith(name[:5]) and n[5:6] in ('0','1')),None)
        if donor is None:
            candidates=[n for n in wad if n.startswith(name[:4]) and len(n)>=6 and n[5] in ('0','1') and n[4]<=name[4]]
            if not candidates: raise ValueError('Missing Freedoom sprite '+name)
            donor=max(candidates,key=lambda n:n[4])
            print(f'Alias {name} to {donor}')
        w,h,left,top,pixels=decode(wad[donor])
        if name[:4] in ('PUNG','PISG','PISF','SHTG','SHTF'): left+=160
        def pixel(x,y):
            index=pixels[(y//2)*2][(x//2)*2]
            return None if index is None else colors[index]
        converted=patch(w,h,left,top,pixel)
        out.extend(bytes((-len(out))%4))
        directory.append((len(out),len(converted),name.encode()))
        out.extend(converted)
    pnames=wad['PNAMES']
    texturedata=wad['TEXTURE1']
    texture_count=st.unpack_from('<i',texturedata)[0]
    for output_name,texture_name in [('PDWALL','STARTAN3'),('PDDOOR','BIGDOOR2'),('PDEXIT','SW1EXIT')]:
        for t in range(texture_count):
            off=st.unpack_from('<i',texturedata,4+4*t)[0]
            if texturedata[off:off+8].rstrip(b'\0').decode()==texture_name: break
        else: raise ValueError('Texture missing '+texture_name)
        tw,th=st.unpack_from('<hh',texturedata,off+12)
        canvas=[[0]*tw for _ in range(th)]
        count=st.unpack_from('<h',texturedata,off+20)[0]
        for j in range(count):
            ox,oy,pn,_,_=st.unpack_from('<hhhhh',texturedata,off+22+10*j)
            pname=pnames[4+8*pn:12+8*pn].rstrip(b'\0').decode()
            pw,ph,_,_,pixels=decode(wad[pname])
            for y in range(ph):
                for x in range(pw):
                    if 0<=x+ox<tw and 0<=y+oy<th and pixels[y][x] is not None:
                        canvas[y+oy][x+ox]=pixels[y][x]
        converted=patch(64,128,0,0,lambda x,y: colors[canvas[(y//2*2)*th//128][(x//2*2)*tw//64]])
        out.extend(bytes((-len(out))%4))
        directory.append((len(out),len(converted),output_name.encode()));out.extend(converted)
    off=len(out)
    for item in directory: out.extend(st.pack('<ii8s',*item))
    st.pack_into('<ii',out,4,len(directory),off)
    (ROOT/'resources/freedoom-art.pbl').write_bytes(out)
    print(f'Freedoom art: {len(directory)} frames, {len(out)} bytes; largest patch {max(s for _,s,_ in directory)}')


if __name__=='__main__': main(sys.argv[1])
