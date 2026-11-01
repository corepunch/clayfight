#!/usr/bin/env python3
"""Check active local paths and syntax without proprietary data or build exports."""
from pathlib import Path
import ast
import re
ROOT=Path(__file__).resolve().parents[1]
for pattern in ('tools/*.py','tests/*.py'):
 for path in ROOT.glob(pattern):ast.parse(path.read_text(),filename=str(path))
project=(ROOT/'ios/Clayfight.xcodeproj/project.pbxproj').read_text()
for path in re.findall(r'"path" = "([^"\n]+)";',project):
 if path.endswith(('.c','.cpp','.m','.h')):assert (ROOT/'ios'/path).is_file(),path
for folder in ('src/game','src/support','src/sound','ios/Clayfight'):
 for path in (ROOT/folder).iterdir():
  if path.suffix in ('.c','.cpp','.m'):assert path.name in project,path
for path in (ROOT/'src').rglob('*'):
 if path.is_file() and path.suffix in ('.c','.h','.cpp','.m'):
  assert 'nile-archive/' not in path.read_text(),path
  assert 'PRAGE.EXE' not in path.read_text(),path
print('Standalone source paths, UIKit project references and Python syntax passed')
