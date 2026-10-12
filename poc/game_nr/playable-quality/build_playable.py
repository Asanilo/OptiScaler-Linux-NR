#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Offline only. No Wine/GPU. Root owns p0 spec; old Q1/Q2/C4 inputs are readonly.
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys
ROOT=Path(__file__).resolve().parent
CANON=ROOT.parents[3]/'integration/playtest/p0-wire-spec.json'
BASE=ROOT.parent/'quality-nr/out/build.json'
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert h(BASE)=='20dc61593a4f801262d0f618abdcfd323dd06e98570f7db0645dca0ecf793257','frozen Q1 build'
base=json.loads(BASE.read_text());pins=base['readonly_dependency_pins'];spec=json.loads(CANON.read_text());assert spec['revision']==2 and spec['protocol_version']==6
readonly={**base['readonly_source_pins'],**base['sources'],**base['generated'],str(BASE):h(BASE),base['artifact']:base['artifact_sha256'],str(CANON):h(CANON)}
def collect(v):
 if isinstance(v,dict):
  if 'path' in v and 'sha256' in v:readonly[v['path']]=v['sha256']
  for k,x in v.items():
   if k.startswith('/') and isinstance(x,str) and len(x)==64:readonly[k]=x
   else:collect(x)
 elif isinstance(v,list):
  for x in v:collect(x)
collect(pins);collect(base['build_tools'])
Q2=ROOT.parent/'q2-controlled/out/build.json';assert h(Q2)=='4c8332fd6a96e57b847afb11b973141bb6dbafcfc5354d4b381e647d5b938826';q2=json.loads(Q2.read_text())
readonly[str(Q2)]=h(Q2);readonly.update(q2['sources']);readonly[q2['artifact']]=q2['artifact_sha256']
GE=Path('/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/files/lib/wine/x86_64-unix')
for p in [GE/'wine',GE/'wine-preloader']:readonly[str(p)]=h(p)
def verify():
 for p,d in readonly.items():
  if not Path(p).is_file() or h(p)!=d:raise SystemExit('readonly source/dependency changed '+p)
verify();out=ROOT/'out';out.mkdir(exist_ok=True);spec_header=out/'p0_spec.h';spec_header.write_text('#pragma once\nstatic const char p0_spec_json[] = R"P0CANON('+CANON.read_text()+')P0CANON";\n')
names=['wine_game_nr_playable.cpp','playable_frame.h','playable_runtime_types.h','playable_transport.h','playable_control.h','playable_wire.h','wire_check.cpp','resource_skeleton.cpp','build_playable.py','README.md']
pre={str(ROOT/n):h(ROOT/n) for n in names};history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True);steps=[]
def run(name,cmd):
 rc=125;output=b''
 try:r=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);rc=r.returncode;output=r.stdout
 except subprocess.TimeoutExpired as e:output=(e.stdout or b'')+b'\nOFFLINE_BUILD_WATCHDOG120s; no Wine/GPU\n'
 log=history/(name+'.log');log.write_bytes(output);steps.append({'name':name,'command':cmd,'exit_code':rc,'log':str(log)});print(json.dumps(steps[-1]),flush=True)
 if rc:print(output.decode(errors='replace')[:10000],flush=True)
 return rc
cc=Path(pins['compiler']['path']);hostcc=Path('/usr/bin/c++');jh=Path(pins['json_header']['path']);assert h(cc)==pins['compiler']['sha256'] and h(jh)==pins['json_header']['sha256'];checker=out/'wire_check';exe=out/'wine_game_nr_playable.exe'
rc=run('CPU-wire-compile',[str(hostcc),'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(jh.parent),str(ROOT/'wire_check.cpp'),'-o',str(checker)]);checks={}
if not rc:
 rc=run('CPU-wire-check',[str(checker)])
 if not rc:checks=json.loads((history/'CPU-wire-check.log').read_text())
if not rc:rc=run('PE-helper-compile',[str(cc),'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+pins['sdk_root'],'-I'+pins['ngx_sdk_root'],'-I'+str(jh.parent),str(ROOT/'wine_game_nr_playable.cpp'),'-o',str(exe)])
sources={str(ROOT/n):h(ROOT/n) for n in names};assert sources==pre,'source changed during build';verify()
for n in names:(history/n).write_bytes((ROOT/n).read_bytes())
(history/'p0_spec.h').write_bytes(spec_header.read_bytes());(history/'p0-wire-spec.json').write_bytes(CANON.read_bytes());(history/'compile-Q1-adapter.h').write_bytes((ROOT.parent/'quality-nr/quality_production_adapter.h').read_bytes())
git=lambda *a:subprocess.check_output(['git',*a],cwd=ROOT,text=True).strip()
d={'status':'PASS' if not rc else 'FAIL','scope':'P0_MANAGED_QUALITY_BUILD_CPU_ONLY_RUNTIME_NOT_APPROVED','exit_code':rc,'time':datetime.now(timezone.utc).isoformat(),'workdir':str(ROOT),'sources':sources,'sources_pre_compile':pre,'sources_pre_post_equal':pre==sources,'readonly_source_pins':readonly,'generated':{str(spec_header):h(spec_header),**({str(checker):h(checker)} if checker.exists() else {})},'build_tools':{str(p):h(p) for p in [cc,hostcc]},'readonly_dependency_pins':pins,'artifact':str(exe),'artifact_sha256':h(exe) if not rc else None,'artifact_bytes':exe.stat().st_size if not rc else None,'steps':steps,'offline_checks':checks,'contract':str(CANON),'contract_sha256':h(CANON),'protocol_version':6,'profile_id':spec['profile_id'],'transport_layout':spec['layout'],'runtime':'NOT_RUN','GPU_execution':False,'Wine_execution':False,'model_runtime':'NOT_RUN','NR_phase':'UNVERIFIED','NR_MVecScale':'EXPERIMENTAL_UNVERIFIED','long_runtime':'NOT_AUTHORIZED_NOT_RUN','parameter_application':spec['config']['parameter_application'],'inherited_Q1_build_sha256':h(BASE),'git_branch':git('branch','--show-current'),'git_head':git('rev-parse','HEAD'),'git_status':git('status','--short'),'history':str(history)}
(out/'build.json').write_text(json.dumps(d,indent=2)+'\n');(history/'build.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps({k:d[k] for k in ['status','exit_code','artifact','artifact_sha256','artifact_bytes','runtime','offline_checks']},indent=2));sys.exit(rc)
