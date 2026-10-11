#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Isolated offline build/CPU checks; never invokes Wine/Vulkan/NR runtime.
from pathlib import Path
from datetime import datetime, timezone
import hashlib,json,subprocess,sys,struct
ROOT=Path(__file__).resolve().parent
BASE=ROOT.parent/'c4-bounded'
CANON=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/integration/QUALITY-NR-CONTRACT.md')
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
if h(CANON)!='482a5bccba778445efbcc2023e07a998e545fcd66c54ecb72d98c2514b9af7b0':raise SystemExit('Canonical Quality contract differs')
basebuild=BASE/'out/build.json'
if h(basebuild)!='463ef9c0c2c2ce67c45a21d57d48a07c4a35bfdf5be0439bea69ea1cc48c9e47':raise SystemExit('Frozen bounded build changed')
base=json.loads(basebuild.read_text());pins=base['readonly_dependency_pins']
if base['artifact_sha256']!='1889a6448a2dc37fa1872fed67d71ac697cd77326ff270954dba206cd899e4fc':raise SystemExit('Frozen bounded artifact identity mismatch')
readonly={**base['readonly_source_pins'],**base['sources'],**base['generated'],str(basebuild):h(basebuild),base['artifact']:base['artifact_sha256'],str(CANON):h(CANON)}
def verify():
 for p,d in readonly.items():
  if h(p)!=d:raise SystemExit('Frozen readonly source/generated/artifact changed: '+p)
 for obj in pins['runtime_dependencies']:
  if Path(obj['path']).stat().st_size!=obj['bytes'] or h(obj['path'])!=obj['sha256']:raise SystemExit('Runtime dependency changed: '+obj['path'])
 for p,d in pins['SDK_headers'].items():
  if h(p)!=d:raise SystemExit('Vulkan SDK changed: '+p)
 for p,d in pins['ngx_sdk_headers'].items():
  if h(p)!=d:raise SystemExit('NGX SDK changed: '+p)
verify()
cc=Path(pins['compiler']['path']);jh=Path(pins['json_header']['path']);hostcc=Path('/usr/bin/c++')
if h(cc)!=pins['compiler']['sha256'] or h(jh)!=pins['json_header']['sha256']:raise SystemExit('Compiler/JSON mismatch')
source_names=['wine_game_nr_quality.cpp','quality_game_transport.h','quality_game_control.h','quality_wire.h','quality_production_adapter.h','frame_paths.h','gpu_validation.h','validate_frame.comp','path_check.cpp','wire_check.cpp','build_quality.py','README.md']
pre={str(ROOT/n):h(ROOT/n) for n in source_names}
out=ROOT/'out';out.mkdir(exist_ok=True);history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True)
steps=[]
def run(name,cmd):
 log=history/(name+'.log');rc=125;output=b''
 try:
  r=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);rc=r.returncode;output=r.stdout
 except subprocess.TimeoutExpired as e:output=(e.stdout or b'')+b'\nOFFLINE_BUILD_WATCHDOG120s; no Wine/GPU\n'
 log.write_bytes(output);steps.append({'name':name,'command':cmd,'exit_code':rc,'log':str(log)})
 print(json.dumps({'step':name,'exit_code':rc,'log':str(log)}),flush=True)
 if rc:print(output.decode(errors='replace')[:8000],flush=True)
 return rc
pathcheck=out/'path_check';wirecheck=out/'wire_check';shader=out/'validate_frame.spv';header=out/'validate_spv.h';exe=out/'wine_game_nr_quality.exe'
checks={};rc=0
for name,src,target,arg in [('routing','path_check.cpp',pathcheck,[str(history/'synthetic-routing')]),('wire','wire_check.cpp',wirecheck,[])]:
 if rc:break
 rc=run(name+'-compile',[str(hostcc),'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(jh.parent),str(ROOT/src),'-o',str(target)])
 if rc==0:rc=run('native-CPU-'+name,[str(target),*arg])
 if rc==0:
  checks[name]=json.loads((history/('native-CPU-'+name+'.log')).read_text())
  if checks[name]['GPU_execution'] or checks[name]['Wine_execution'] or checks[name]['model_calls']!=0:raise SystemExit('CPU check scope mismatch')
  (history/(name+'-result.json')).write_text(json.dumps(checks[name],indent=2)+'\n')
if rc==0:rc=run('glsl-compile',['/usr/bin/glslangValidator','-V','--target-env','vulkan1.2',str(ROOT/'validate_frame.comp'),'-o',str(shader)])
if rc==0:rc=run('spirv-validation',['/usr/bin/spirv-val','--target-env','vulkan1.2',str(shader)])
if rc==0:
 words=struct.unpack('<'+'I'*(shader.stat().st_size//4),shader.read_bytes())
 header.write_text('#pragma once\n#include <cstdint>\nstatic const uint32_t c3_validate_spv[] = {\n'+','.join(hex(w)+'u' for w in words)+'\n};\n')
 rc=run('helper-compile',[str(cc),'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+pins['sdk_root'],'-I'+pins['ngx_sdk_root'],'-I'+str(jh.parent),str(ROOT/'wine_game_nr_quality.cpp'),'-o',str(exe)])
sources={str(ROOT/n):h(ROOT/n) for n in source_names}
if pre!=sources:raise SystemExit('Source changed during compile, history retained')
verify()
for n in source_names:(history/n).write_bytes((ROOT/n).read_bytes())
for p in readonly:
 path=Path(p)
 if path.suffix in ['.h','.hpp','.cpp','.py','.md','.json']:
  dest=history/'readonly-dependencies'/path.relative_to('/');dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(path.read_bytes())
for p in [shader,header,pathcheck,wirecheck]:
 if p.exists():(history/p.name).write_bytes(p.read_bytes())
tools={str(p):h(p) for p in ['/usr/bin/glslangValidator','/usr/bin/spirv-val',cc,hostcc]}
# Verify the installed shader tools against the inherited accepted C4 build.
old_c4=json.loads((ROOT.parent/'c4/out/build.json').read_text())
for p,d in old_c4['build_tools'].items():
 if p in tools and tools[p]!=d:raise SystemExit('Build tool changed: '+p)
git=lambda *args:subprocess.check_output(['git',*args],cwd=ROOT,text=True).strip()
d={'status':'PASS' if rc==0 else 'FAIL','scope':'QUALITY_PROTOCOL5_OFFLINE_BUILD_AND_CPU_PREDICATES_ONLY','time':datetime.now(timezone.utc).isoformat(),'exit_code':rc,'workdir':str(ROOT),'sources':sources,'sources_pre_compile':pre,'sources_pre_post_equal':sources==pre,'readonly_source_pins':readonly,'generated':{str(p):h(p) for p in [shader,header,pathcheck,wirecheck] if p.exists()},'build_tools':tools,'readonly_dependency_pins':pins,'runtime':'NOT_RUN','artifact':str(exe),'artifact_sha256':h(exe) if rc==0 else None,'artifact_bytes':exe.stat().st_size if rc==0 else None,'steps':steps,'protocol_version':5,'execution_mode':'diagnostic','transport_layout':'frame-buckets-64-v1','profile_id':'quality-hdr-direct720-raw-renderpx-v1','contract':str(CANON),'contract_sha256':h(CANON),'offline_checks':checks,'GPU_execution':False,'Wine_execution':False,'model_runtime':'NOT_RUN','Quality_model_feasibility':'NOT_RUN','NR_phase':'UNVERIFIED','NR_MVecScale':'production_candidate_UNVERIFIED','frozen_bounded_artifact':{'path':base['artifact'],'sha256':base['artifact_sha256'],'bytes':base['artifact_bytes']},'frozen_bounded_build_sha256':h(basebuild),'frozen_C4_artifact':base['frozen_C4_artifact'],'path_classes':base['path_classes'],'git_branch':git('branch','--show-current'),'git_head':git('rev-parse','HEAD'),'git_status':git('status','--short')}
(out/'build.json').write_text(json.dumps(d,indent=2)+'\n');(history/'build.json').write_text(json.dumps(d,indent=2)+'\n')
print(json.dumps({k:d[k] for k in ['status','exit_code','artifact','artifact_sha256','artifact_bytes','protocol_version','runtime']},indent=2));sys.exit(rc)
