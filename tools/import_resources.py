#!/usr/bin/env python3
"""Import the supported arcade chip set; no DOS installation or conversion."""
import argparse
from pathlib import Path
import shutil
GAME=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('source',nargs='?',type=Path,default=GAME/'data/primrage')
a=p.parse_args();source=a.source.resolve();destination=GAME/'data/primrage'
required=['136102-1044b.29l','136102-1043b.28l','136102-1042b.27l','136102-1041b.25l',
 '136102-1100a.2v','136102-1101a.2w','136102-0050a.25n','136102-0051a.27n','136102-0052a.28n',
 '136102-1045b.23p','136102-1078a.11a','136102-0075','136102-0077']
required += [f'136102-{332+2*i:04d}.mol1.{i}' for i in range(4)]
required += [f'136102-{333+2*i:04d}.moh1.{i}' for i in range(4)]
required += [f'136102-{316+2*i:04d}.mol0.{i}' for i in range(8)]
required += [f'136102-{317+2*i:04d}.moh0.{i}' for i in range(8)]
missing=[name for name in required if not (source/name).is_file()]
if missing:p.error('Missing arcade chips: '+', '.join(missing))
if source!=destination.resolve():
 destination.mkdir(parents=True,exist_ok=True)
 for name in required:shutil.copy2(source/name,destination/name)
print(f'Arcade ROMs ready: {destination}. Fighters, menus, stages and fonts load directly from the chips.')
