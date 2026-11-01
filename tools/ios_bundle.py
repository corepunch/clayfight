#!/usr/bin/env python3
"""Package local game data inside the native app; no server or Python at runtime."""
from pathlib import Path
import argparse
import shutil
import os

ROOT = Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination',type=Path)
    args=parser.parse_args()
    data=args.destination/'GameData'
    original=ROOT/'data/primrage'
    available=(original/'136102-1044b.29l').exists()
    if not available and not os.getenv('CLAY_ALLOW_MISSING_ROM'):raise SystemExit('Supply the primrageo arcade ROMs in data/primrage/')
    if data.exists(): shutil.rmtree(data)
    data.mkdir(parents=True)
    if available:shutil.copytree(original,data/'primrage',ignore=shutil.ignore_patterns('.*'))
    else:print('Compile-only build: proprietary arcade ROMs omitted')
    if (ROOT/'data/cage-BSD.txt').exists():shutil.copy2(ROOT/'data/cage-BSD.txt',data/'cage-BSD.txt')
    shutil.copytree(ROOT/'data/creatures',data/'creatures',dirs_exist_ok=True,ignore=shutil.ignore_patterns('.*','sources','*.bmp','capture.json','capture.plist'))
if __name__=='__main__':main()
