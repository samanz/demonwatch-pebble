import importlib.util
import re
import struct
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]


class Assets(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=(ROOT/'resources/arena.pbl').read_bytes()
        magic,count,offset=struct.unpack_from('<4sii',cls.data)
        assert magic==b'IWAD'
        cls.lumps=[]
        for i in range(count):
            pos,size,name=struct.unpack_from('<ii8s',cls.data,offset+16*i)
            assert 0<=pos<=len(cls.data) and 0<=size<=len(cls.data)-pos
            cls.lumps.append((name.rstrip(b'\0').decode(),cls.data[pos:pos+size]))
        cls.named=dict(cls.lumps)

    def test_map_records_and_indices(self):
        sizes={'THINGS':8,'LINEDEFS':31,'SIDEDEFS':7,'SEGS':18,'SSECTORS':4,'NODES':28,'SECTORS':12}
        for name,size in sizes.items():
            self.assertEqual(len(self.named[name])%size,0,name)
        numlines=len(self.named['LINEDEFS'])//31
        numsides=len(self.named['SIDEDEFS'])//7
        for off in range(0,len(self.named['SEGS']),18):
            side,line=struct.unpack_from('<HH',self.named['SEGS'],off+12)
            self.assertLess(side,numsides); self.assertLess(line,numlines)
        for off in range(0,len(self.named['LINEDEFS']),31):
            if self.named['LINEDEFS'][off+30]==1:
                back=struct.unpack_from('<H',self.named['LINEDEFS'],off+16)[0]
                sector=self.named['SIDEDEFS'][back*7+6]
                self.assertEqual(sector%2,1, 'Manual door must target the door sector')
        self.assertLess(len(self.data)+4096,262144)

    def test_bsp_and_sector_links(self):
        sectors=len(self.named['SECTORS'])//12
        subsectors=len(self.named['SSECTORS'])//4
        self.assertEqual(sectors,7)
        self.assertEqual(subsectors,sectors)
        nodes=self.named['NODES']
        for index,off in enumerate(range(0,len(nodes),28)):
            for child in struct.unpack_from('<HH',nodes,off+24):
                if child & 0x8000: self.assertLess(child & 0x7fff,subsectors)
                else: self.assertLess(child,index)
        for off in range(0,len(self.named['SIDEDEFS']),7):
            self.assertLess(self.named['SIDEDEFS'][off+6],sectors)
        self.assertGreaterEqual(len(self.named['REJECT']),(sectors*sectors+7)//8)

    def test_patches_and_engine_frames(self):
        start=next(i for i,(n,_) in enumerate(self.lumps) if n=='S_START')
        patches=self.lumps[start+1:-1]
        for name,patch in patches:
            width,height,_,_=struct.unpack_from('<hhhh',patch)
            for x in range(width):
                p=struct.unpack_from('<I',patch,8+4*x)[0]
                while patch[p]!=255:
                    top,length=patch[p:p+2]
                    self.assertLessEqual(top+length,height,name)
                    self.assertLess(p+length+4,len(patch),name)
                    p+=length+4
        self.assertIn('POL5A0',self.named)  # A closing door can crush a corpse into gibs.
        states=(ROOT/'src/doom/info.c').read_text(encoding='utf-8')
        prefixes={n[:4] for n,_ in patches}
        for prefix,frame in re.findall(r'\{SPR_(\w+),\s*(\d+)',states):
            if prefix in prefixes:
                self.assertIn(prefix+chr(65+(int(frame)&32767))+'0',self.named)

    def test_texture_references(self):
        table=self.named['TEXTUREP']
        count=len(self.named['TEXHEIGH'])//2
        for i in range(count):
            off=struct.unpack_from('<H',table,2*i)[0]
            mask,width,height,overlap,npatches=struct.unpack_from('<HhhBB',table,off)
            self.assertEqual(mask,width-1)
            for j in range(npatches):
                _,_,patch_id,_=struct.unpack_from('<hhhh',table,off+8+8*j)
                patch=self.lumps[patch_id][1]
                self.assertEqual(struct.unpack_from('<hh',patch),(width,height))


if __name__=='__main__':
    unittest.main()
