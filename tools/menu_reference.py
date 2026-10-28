#!/usr/bin/env python3
"""Extract the installed PC game's menu reference to ignored build output.

python3 tools/menu_reference.py [--data primalrage] [--output build/menu-reference]
No game artwork is downloaded or added to version control.
"""
import argparse
import json
from pathlib import Path
import struct
import grasheet

ART = {"title-slab": ("S04TITLE.GRA",45), "title-primal": ("S04TITLE.GRA",37),
       "title-rage": ("S04TITLE.GRA",19), "select-temple": ("S04SELMO.GRA",136),
       "select-temple-overlay": ("S04SELMO.GRA",137),
       "select-podium-left": ("S04SELMO.GRA",105), "select-podium-right": ("S04SELMO.GRA",106),
       "menu-stone": ("S04SLABS.GRA",37),
       "urth-globe": ("S04GLOBM.GRA",19), "urth-north": ("S04GLOBM.GRA",23),
       "urth-island": ("S04GLOBM.GRA",37), "urth-south": ("S04GLOBM.GRA",44)}


def strings(path):
    data=path.read_bytes(); result=[]; offset=0
    while offset+8<=len(data):
        kind,magic,nxt=struct.unpack_from('<HHI',data,offset)
        if kind!=1 or magic!=0x3334: raise ValueError('Not a Primal Rage text archive')
        end=nxt or len(data); pos=offset+8
        while pos<end:
            size=data[pos];pos+=1
            if pos+size>end:break
            if size:result.append(data[pos:pos+size].decode('latin1'))
            pos+=size
        if not nxt:break
        offset=nxt
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data',type=Path,default=Path('primalrage'))
    parser.add_argument('--output',type=Path,default=Path('build/menu-reference'))
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    grasheet.DATA=str(args.data)
    reference={"source":"Primal Rage PC 1.0, installed local data", "strings":strings(args.data/'ENGLISH.TXT'),"art":{}}
    for name,(archive,index) in ART.items():
        raw,frames,palettes=grasheet.load(archive);f=frames[index];pixels=grasheet.decode(raw,f);palette=grasheet.guess(pixels,palettes)
        rgba=bytearray(f[0]*f[1]*4)
        for y,row in enumerate(pixels):
            for x,value in enumerate(row):
                if value and value<=len(palette):
                    i=(y*f[0]+x)*4;rgba[i:i+4]=bytes((*palette[value-1],255))
        grasheet.png(str(args.output/f'{name}.png'),f[0],f[1],rgba)
        reference['art'][name]={"archive":archive,"frame":index,"width":f[0],"height":f[1],"hotspot":list(f[2:4]),"palette":"guessed"}
    (args.output/'reference.json').write_text(json.dumps(reference,ensure_ascii=False,indent=2))
    print(f"Extracted {len(reference['strings'])} original strings and {len(ART)} menu/map patches to {args.output}")

if __name__=='__main__':main()
