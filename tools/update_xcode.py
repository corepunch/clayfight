#!/usr/bin/env python3
"""Keep the standalone UIKit target's sources in sync with src/ and ios/Clayfight/."""
from pathlib import Path
import hashlib
import json
import plistlib
import subprocess
ROOT=Path(__file__).resolve().parents[1]
PROJECT=ROOT/'ios/Clayfight.xcodeproj/project.pbxproj'
def encode(value):
 if isinstance(value,dict):return '{'+''.join(f'{encode(k)} = {encode(v)};\n' for k,v in value.items())+'}'
 if isinstance(value,list):return '('+''.join(encode(v)+',\n' for v in value)+')'
 return json.dumps(str(value))
def uid(value):return hashlib.sha256(value.encode()).hexdigest()[:24].upper()
def update():
 project=json.loads(subprocess.check_output(['plutil','-convert','json','-o','-',str(PROJECT)]))
 objects=project['objects']
 old={k for k,v in objects.items() if v.get('isa')=='PBXFileReference' and str(v.get('path','')).endswith(('.c','.cpp','.m'))}
 oldbuild={k for k,v in objects.items() if v.get('isa')=='PBXBuildFile' and v.get('fileRef') in old}
 for v in objects.values():
  for key in ('children','files'):
   if key in v:v[key]=[i for i in v[key] if i not in old|oldbuild]
 for k in old|oldbuild:objects.pop(k)
 source_group=objects[objects[project['rootObject']]['mainGroup']]
 source_phase=next(v for v in objects.values() if v.get('isa')=='PBXSourcesBuildPhase')
 sources=sorted((ROOT/'src/game').glob('*.c'))+sorted((ROOT/'src/support').glob('*.c'))+sorted((ROOT/'src/sound').glob('*.cpp'))+[ROOT/'src/platform/d_event.c']+sorted((ROOT/'ios/Clayfight').glob('*.m'))
 for p in sources:
  path=p.relative_to(ROOT/'ios').as_posix() if p.is_relative_to(ROOT/'ios') else '../'+p.relative_to(ROOT).as_posix()
  ref=uid('source/'+path);build=uid('build/'+path)
  objects[ref]=dict(isa='PBXFileReference',lastKnownFileType='sourcecode.cpp.cpp' if p.suffix=='.cpp' else 'sourcecode.c.objc' if p.suffix=='.m' else 'sourcecode.c.c',path=path,sourceTree='SOURCE_ROOT')
  objects[build]=dict(isa='PBXBuildFile',fileRef=ref)
  source_group['children'].append(ref);source_phase['files'].append(build)
 for obj in objects.values():
  if obj.get('path','').startswith('../src/engine/'):
   obj['path']='../src/game/'+('resources.h' if obj['path'].endswith('engine.h') else Path(obj['path']).name)
  if obj.get('isa')=='XCBuildConfiguration':
   settings=obj.get('buildSettings',{});settings.update(CLANG_CXX_LANGUAGE_STANDARD='c++20',CLANG_CXX_LIBRARY='libc++')
   if 'HEADER_SEARCH_PATHS' in settings:settings['HEADER_SEARCH_PATHS']=['$(SRCROOT)/../src','$(SRCROOT)/../third_party']
 frameworks=source_group
 phase=next(v for v in objects.values() if v.get('isa')=='PBXFrameworksBuildPhase')
 if not any(v.get('path','').endswith('GameController.framework') for v in objects.values()):
  ref=uid('GameController.framework');build=uid('link/GameController.framework')
  objects[ref]=dict(isa='PBXFileReference',lastKnownFileType='wrapper.framework',path='System/Library/Frameworks/GameController.framework',sourceTree='SDKROOT')
  objects[build]=dict(isa='PBXBuildFile',fileRef=ref);frameworks['children'].append(ref);phase['files'].append(build)
 PROJECT.write_text('// !$*UTF8*$!\n'+encode(project)+'\n')
if __name__=='__main__':update()
