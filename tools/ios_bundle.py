#!/usr/bin/env python3
"""Package local game data inside the native app; no server or Python at runtime."""
from pathlib import Path
import argparse
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination',type=Path)
    args=parser.parse_args()
    data=args.destination/'GameData'
    original=ROOT/'primalrage'
    if not (original/'INDEX').exists():raise SystemExit('Put your original Primal Rage PC data in primalrage/ first')
    subprocess.run(['/usr/bin/python3','-I',str(ROOT/'tools/pr2creature.py'),str(original),str(ROOT/'creatures/primalrage')],check=True)
    if data.exists(): shutil.rmtree(data)
    (data/'primalrage').mkdir(parents=True,exist_ok=True)
    for file in original.iterdir():
        if file.suffix.upper()=='.GRA' or file.name=='INDEX':shutil.copy2(file,data/'primalrage'/file.name)
    shutil.copytree(ROOT/'creatures',data/'creatures',dirs_exist_ok=True,ignore=shutil.ignore_patterns('.*','sources','*.bmp','capture.json','capture.plist'))
if __name__=='__main__':main()
