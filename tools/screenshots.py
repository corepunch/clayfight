#!/usr/bin/env python3
"""Capture and review the arcade runtime with one combined game start."""
from pathlib import Path
import argparse
import subprocess
from PIL import Image
GAME=Path(__file__).resolve().parents[1]
ROOT=GAME
TEMP=GAME/'build/arcade-review'
OUT=GAME/'docs/screenshots'
REVIEW=GAME/'docs/review/arcade/after'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--encode-only',action='store_true',help='Encode existing combined-suite captures without restarting the game')
args=parser.parse_args()
for path in (TEMP,OUT,REVIEW):path.mkdir(parents=True,exist_ok=True)
if not args.encode_only:subprocess.run([str(ROOT/'build/clayfight-arcade'),'-nosound','-creatures','data/creatures'],cwd=GAME,check=True)
for file in TEMP.glob('*.bmp'):
 if not file.name.startswith(('reel-','scene-reel-','title-reel-')):Image.open(file).save(REVIEW/(file.stem+'.png'))
for prefix,name,ticks in [('reel-','fight.gif',6),('scene-reel-','scenery.gif',4),('title-reel-','title.gif',6)]:
 frames=[Image.open(file).convert('RGB') for file in sorted(TEMP.glob(prefix+'*.bmp'))]
 # GIF stores centiseconds. Distribute rounding so four 60 Hz ticks retain
 # their true average duration rather than each being truncated to 60 ms.
 durations=[10*(round((i+1)*ticks*100/60)-round(i*ticks*100/60)) for i in range(len(frames))]
 frames[0].save(REVIEW/name,save_all=True,append_images=frames[1:],duration=durations,loop=0,optimize=True)
for name,source in {'fight':'clay-fight','select':'select','campaign':'campaign','menu':'menu'}.items():
 Image.open(REVIEW/(source+'.png')).save(OUT/(name+'.png'))
 print(OUT/(name+'.png'))
