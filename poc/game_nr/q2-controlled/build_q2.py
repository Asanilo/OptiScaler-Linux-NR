#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Offline build only: host CPU schema checks, asset SHA validation, PE compilation. Never Wine/GPU.
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys
ROOT=Path(__file__).resolve().parent
BASE=ROOT.parent/'quality-nr/out/build.json'
B=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/bbport-worktree/tools/nr_probe/q2_controlled')
ASSETS=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/bbport/runs/q2-controlled-assets-v1')
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
if h(BASE)!='20dc61593a4f801262d0f618abdcfd323dd06e98570f7db0645dca0ecf793257':raise SystemExit('frozen Q1 manifest mismatch')
base=json.loads(BASE.read_text());pins=base['readonly_dependency_pins']
readonly={**base['readonly_source_pins'],**base['sources'],**base['generated'],str(BASE):h(BASE),base['artifact']:base['artifact_sha256']}
def collect(v):
 if isinstance(v,dict):
  if 'path' in v and 'sha256' in v:readonly[v['path']]=v['sha256']
  for k,value in v.items():
   if k.startswith('/') and isinstance(value,str) and len(value)==64:readonly[k]=value
   else:collect(value)
 elif isinstance(v,list):
  for item in v:collect(item)
collect(pins);collect(base['build_tools'])
if h(B/'fixture-spec.json')!='12fcd962a4fdb09543994cbc0a0b21bb8ea3e1b85f8b4f7c6daeb44e3f5abb66':raise SystemExit('B plan changed')
if h(ASSETS/'asset-index.json')!='45d78520d9b7c8fb88e152064e5b3a5e4b016306694bf0a4e34873f4d4a88192':raise SystemExit('B index changed')
index=json.loads((ASSETS/'asset-index.json').read_text())
for p in [B/'fixture-spec.json',B/'CASE-SCHEMA.md',ASSETS/'asset-index.json']:readonly[str(p)]=h(p)
for obj in index['cases']+index['assets']:
 p=Path(obj['path']);assert p.is_file() and h(p)==obj['sha256'],str(p)
 if 'bytes' in obj:assert p.stat().st_size==obj['bytes'],str(p)
 readonly[str(p)]=obj['sha256']
GE=Path('/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64')
wine=GE/'files/lib/wine/x86_64-unix/wine';preloader=wine.with_name('wine-preloader')
for p in [wine,preloader]:readonly[str(p)]=h(p)
# Existing Vulkan/NVAPI/DXGI and all exact SDK/runtime dependencies stay inherited readonly.
def verify():
 for p,d in readonly.items():
  if not Path(p).is_file() or h(p)!=d:raise SystemExit('readonly pin changed: '+p)
verify()
cc=Path(pins['compiler']['path']);jh=Path(pins['json_header']['path']);hostcc=Path('/usr/bin/c++')
assert h(cc)==pins['compiler']['sha256'] and h(jh)==pins['json_header']['sha256']
names=['wine_q2_motion_fixture.cpp','q2_production_adapter.h','q2_common.h','q2_vulkan_context.h','q2_case.h','case_check.cpp','build_q2.py','README.md']
pre={str(ROOT/n):h(ROOT/n) for n in names}
out=ROOT/'out';out.mkdir(exist_ok=True);history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True)
steps=[]
def run(name,cmd):
 rc=125;output=b''
 try:
  r=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);rc=r.returncode;output=r.stdout
 except subprocess.TimeoutExpired as e:output=(e.stdout or b'')+b'\nOFFLINE_BUILD_WATCHDOG120s; Wine/GPU NOT_RUN\n'
 log=history/(name+'.log');log.write_bytes(output);steps.append({'name':name,'command':cmd,'exit_code':rc,'log':str(log)});print(json.dumps(steps[-1]),flush=True)
 if rc:print(output.decode(errors='replace')[:10000],flush=True)
 return rc
checker=out/'case_check';exe=out/'wine_q2_motion_fixture.exe'
rc=run('CPU-schema-compile',[str(hostcc),'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(jh.parent),str(ROOT/'case_check.cpp'),'-o',str(checker)])
checks={}
if not rc:
 rc=run('CPU-schema-check',[str(checker),*[o['path'] for o in index['cases']]])
 if not rc:checks=json.loads((history/'CPU-schema-check.log').read_text())
if not rc:rc=run('PE-helper-compile',[str(cc),'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+pins['sdk_root'],'-I'+pins['ngx_sdk_root'],'-I'+str(jh.parent),str(ROOT/'wine_q2_motion_fixture.cpp'),'-o',str(exe)])
sources={str(ROOT/n):h(ROOT/n) for n in names}
if pre!=sources:raise SystemExit('source changed during build')
verify()
for n in names:(history/n).write_bytes((ROOT/n).read_bytes())
# Snapshot text dependencies only; never copy models/PE/assets/SDK trees into git.
for p in [B/'fixture-spec.json',B/'CASE-SCHEMA.md',ASSETS/'asset-index.json',*[Path(o['path']) for o in index['cases']]]:
 dest=history/'canonical'/p.name;dest.parent.mkdir(exist_ok=True);dest.write_bytes(p.read_bytes())
git=lambda *a:subprocess.check_output(['git',*a],cwd=ROOT,text=True).strip()
d={'status':'PASS' if rc==0 else 'FAIL','scope':'Q2_CONTROLLED_OFFLINE_BUILD_CPU_ONLY','schema':'q2-known-translation-v1','time':datetime.now(timezone.utc).isoformat(),'exit_code':rc,'workdir':str(ROOT),'sources':sources,'sources_pre_compile':pre,'sources_pre_post_equal':sources==pre,'readonly_source_pins':readonly,'generated':{str(checker):h(checker)} if checker.exists() else {},'build_tools':{str(p):h(p) for p in [cc,hostcc]},'readonly_dependency_pins':pins,'artifact':str(exe),'artifact_sha256':h(exe) if rc==0 else None,'artifact_bytes':exe.stat().st_size if rc==0 else None,'steps':steps,'offline_checks':checks,'assets_CPU_SHA_verified':len(index['assets']),'assets_total_bytes':index['total_asset_bytes'],'case_count':6,'branch_ids':['primary','repeat_primary','zero_mv','reverse_mv','reset_second'],'frames_per_branch':2,'GPU_execution':False,'Wine_execution':False,'runtime':'NOT_RUN','model_runtime':'NOT_RUN','NR_phase':'UNVERIFIED','NR_MVecScale':'UNVERIFIED','bcrypt_dependency':'NOT_USED','budgets_ms':{'cold':120000,'frame':30000,'gpu_fence':10000,'outer':300000},'git_branch':git('branch','--show-current'),'git_head':git('rev-parse','HEAD'),'git_status':git('status','--short'),'history':str(history),'inherited_Q1_build':str(BASE),'inherited_Q1_build_sha256':h(BASE),'plan_sha256':h(B/'fixture-spec.json'),'case_schema_sha256':h(B/'CASE-SCHEMA.md')}
(out/'build.json').write_text(json.dumps(d,indent=2)+'\n');(history/'build.json').write_text(json.dumps(d,indent=2)+'\n')
print(json.dumps({k:d[k] for k in ['status','exit_code','artifact','artifact_sha256','artifact_bytes','runtime','offline_checks']},indent=2),flush=True);sys.exit(rc)
