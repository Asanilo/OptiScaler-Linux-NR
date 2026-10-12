#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# CPU/schema/hash build only. No Wine, Vulkan API, GPU or executable PE invocation.
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys
ROOT=Path(__file__).resolve().parent
ORIG=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-worktree')
BASE=ORIG/'poc/game_nr/quality-nr/out/build.json'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert sha(BASE)=='20dc61593a4f801262d0f618abdcfd323dd06e98570f7db0645dca0ecf793257'
base=json.loads(BASE.read_text());pins=base['readonly_dependency_pins'];readonly={**base['readonly_source_pins'],**base['sources'],**base['generated'],str(BASE):sha(BASE),base['artifact']:base['artifact_sha256']}
def collect(v):
 if isinstance(v,dict):
  if 'path' in v and 'sha256' in v:readonly[v['path']]=v['sha256']
  for k,item in v.items():
   if k.startswith('/') and isinstance(item,str) and len(item)==64:readonly[k]=item
   else:collect(item)
 elif isinstance(v,list):
  for item in v:collect(item)
collect(pins);collect(base['build_tools'])
# Only new manifest changes these external installed-driver pins. Old manifest/file bytes stay exact.
oldcore='/usr/lib64/nvidia/wine/_nvngx.dll';oldlib='/usr/lib64/libnvidia-ngx.so.615.71.09';newlib='/usr/lib64/libnvidia-ngx.so.615.78.08'
drift=[]
for p in [oldcore,oldlib]:
 if p in readonly:drift.append({'path':p,'old_sha256':readonly[p],'current_sha256':sha(p) if Path(p).is_file() else None})
readonly.pop(oldlib,None)
readonly[oldcore]='dabe18b0dc559750113b589d06f672141c8c49a2a571e0a5a7ca7b3e584a7ac3'
readonly[newlib]='85da2399cceeb954b8dc2753cc9bb50ddabd1baf847d8664b85374cc99b03990'
for p in ['q2_common.h','q2_vulkan_context.h','q2_production_adapter.h']:
 fp=ROOT.parent/'q2-controlled'/p;readonly[str(fp)]=sha(fp)
GE=Path('/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/files/lib/wine/x86_64-unix')
for p in [GE/'wine',GE/'wine-preloader']:readonly[str(p)]=sha(p)
case=json.loads((ROOT/'case.json').read_text());manifest=json.loads((ROOT/'input-manifest.json').read_text())
assert case['frames']==manifest['frames'] and case['input_manifest_sha256']==sha(ROOT/'input-manifest.json')
for frame in manifest['frames']:
 for obj in [*frame['assets'].values(),frame['original_control']]:
  p=Path(obj['path']);assert p.is_file() and sha(p)==obj['sha256'],str(p)
  if 'bytes' in obj:assert p.stat().st_size==obj['bytes']
  readonly[str(p)]=obj['sha256']
def verify():
 for p,h in readonly.items():assert Path(p).is_file() and sha(p)==h,p
verify()
compiler=Path(pins['compiler']['path']);hostcc=Path('/usr/bin/c++');jh=Path(pins['json_header']['path'])
names=['lowres_fixture.cpp','lowres_production_adapter.h','lowres_vulkan_context.h','lowres_common.h','lowres_schema.h','sha256.h','half_rne.h','schema_check.cpp','build_lowres.py','case.json','input-manifest.json','CASE-SCHEMA.md','README.md','.gitignore']
sources={str(ROOT/n):sha(ROOT/n) for n in names};out=ROOT/'out';out.mkdir(exist_ok=True);history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True);steps=[]
def run(name,cmd):
 try:r=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);code=r.returncode;data=r.stdout
 except subprocess.TimeoutExpired as e:code=124;data=(e.stdout or b'')+b'\nCPU_BUILD_TIMEOUT120_NO_WINE_GPU\n'
 log=history/(name+'.log');log.write_bytes(data);steps.append({'name':name,'command':cmd,'exit_code':code,'log':str(log)});print(json.dumps(steps[-1]),flush=True)
 if code:print(data.decode(errors='replace')[:12000],flush=True)
 return code
checker=out/'schema_check';exe=out/'wine_nr_lowres_fixture.exe'
rc=run('CPU-schema-compile',[str(hostcc),'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(jh.parent),str(ROOT/'schema_check.cpp'),'-o',str(checker)])
checks={}
if not rc:
 rc=run('CPU-schema-actual-assets',[str(checker),str(ROOT/'case.json'),str(ROOT/'input-manifest.json')])
 if not rc:checks=json.loads((history/'CPU-schema-actual-assets.log').read_text())
if not rc:rc=run('PE-lowres-compile',[str(compiler),'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+pins['sdk_root'],'-I'+pins['ngx_sdk_root'],'-I'+str(jh.parent),str(ROOT/'lowres_fixture.cpp'),'-o',str(exe)])
post={str(ROOT/n):sha(ROOT/n) for n in names};assert post==sources;verify()
for name in names:(history/name).write_bytes((ROOT/name).read_bytes())
git=lambda *a:subprocess.check_output(['git',*a],cwd=ROOT,text=True).strip()
d={'status':'BUILD_ONLY_PASS' if rc==0 else 'BUILD_ONLY_FAIL','exit_code':rc,'scope':'CPU_schema/hash/assets and PE crosscompile only; no Wine/Vulkan/GPU execution','sources':post,'sources_pre_compile':sources,'sources_pre_post_equal':True,'readonly_source_pins':readonly,'generated':{str(checker):sha(checker)} if checker.exists() else {},'build_tools':{str(p):sha(p) for p in [compiler,hostcc,Path(sys.executable)]},'readonly_dependency_pins':pins,'current_runtime_pins':{p:readonly[p] for p in [oldcore,newlib]},'old_runtime_drift_preserved':drift,'runtime_load_scope':'Requested file identities pinned; actual module paths/driver logged only future PE run. Current-runtime1080 reference MUST PASS before720. P3 historical times not matching baseline.','artifact':str(exe),'artifact_sha256':sha(exe) if rc==0 else None,'artifact_bytes':exe.stat().st_size if rc==0 else None,'steps':steps,'offline_checks':checks,'case_sha256':sha(ROOT/'case.json'),'input_manifest_sha256':sha(ROOT/'input-manifest.json'),'planned_single_PE':{'frames':12,'reference1080':4,'lowres720':4,'skip720':4,'Evaluate':8,'resolve':8,'feature_create_release':2,'repetitions':1},'possible_repeat':'Root may request at most2 identicalPE starts in new user-authorized budget; no automatic retry','budgets_ms':{'cold':120000,'frame':30000,'fence':10000,'outer':300000},'runtime':'NOT_RUN','GPU':'NOT_RUN','NR_phase':'UNVERIFIED','MVecScale':'EXPERIMENTAL_UNVERIFIED','cost_60FPS':'NOT_IDENTIFIED','cache':'separate CPU core not native wire/game integration','git_branch':git('branch','--show-current'),'git_head':git('rev-parse','HEAD'),'git_status':git('status','--short'),'history':str(history)}
(out/'build.json').write_text(json.dumps(d,indent=2)+'\n');(history/'build.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps({k:d[k] for k in ['status','exit_code','artifact','artifact_sha256','offline_checks','runtime']},indent=2));sys.exit(rc)
