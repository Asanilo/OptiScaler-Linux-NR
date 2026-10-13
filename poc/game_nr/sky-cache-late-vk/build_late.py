#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib,json,subprocess,sys
from pathlib import Path
from datetime import datetime,timezone
from late_cases import generate
from cpu_checks import run as cpu_run
ROOT=Path(__file__).resolve().parent;R2=ROOT.parent/'cache-r2';bp=R2/'out/build.json';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert sha(bp)=='ff309ae699adf6958b440c659f39b34b8bc3e103f78e3ac9d0780ee52b54095d';base=json.loads(bp.read_text());deps={}
for k in ['sources','readonly_source_pins','generated','build_tools']:deps.update(base[k])
deps[str(bp)]=sha(bp);deps[base['artifact']]=base['artifact_sha256']
for n in ['late_arrival.py','sky_cache_reference.py']:p=ROOT.parent/'sky-cache-cpu'/n;deps[str(p)]=sha(p)
names=['.gitignore','README.md','IMPLEMENTATION-BOUNDARY.md','SKY-PASS-INTERFACE.md','late_origin_sequence.h','late_fixture.cpp','late_cases.py','cpu_checks.py','build_late.py'];sources={str(ROOT/n):sha(ROOT/n) for n in names}
def verify():
 for p,h in deps.items():assert sha(p)==h,p
verify();out=ROOT/'out';out.mkdir(exist_ok=True);history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True)
for n in names:(history/n).write_bytes((ROOT/n).read_bytes())
case,proofs=generate(out);checks=cpu_run(case,proofs);(history/'CPU-checks.json').write_text(json.dumps(checks,indent=2)+'\n');(history/'CPU-lineage-proofs.json').write_text(json.dumps(proofs,indent=2)+'\n')
steps=[]
def cmd(name,args):
 try:r=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);rc=r.returncode;data=r.stdout
 except subprocess.TimeoutExpired as e:rc=124;data=e.stdout or b''
 p=history/(name+'.log');p.write_bytes(data);steps.append({'name':name,'command':args,'exit_code':rc,'log':str(p)});print(json.dumps(steps[-1]),flush=True)
 if rc:print(data.decode(errors='replace')[:12000],flush=True)
 return rc
rc=cmd('Exact-r2-SPIRV-val',['/usr/bin/spirv-val','--target-env','vulkan1.2',str(R2/'out/sky_cache.spv')]);b=json.loads((ROOT.parent/'high-fps-fixture/out/build.json').read_text());p=b['readonly_dependency_pins'];cc=p['compiler']['path'];exe=out/'wine_sky_cache_late_fixture.exe'
if not rc:rc=cmd('PE-late-origin-compile',[cc,'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+p['sdk_root'],'-I'+p['ngx_sdk_root'],'-I'+str(Path(p['json_header']['path']).parent),str(ROOT/'late_fixture.cpp'),'-o',str(exe)])
post={str(ROOT/n):sha(ROOT/n) for n in names};assert post==sources;verify();generated={str(p):sha(p) for p in out.rglob('*') if p.is_file() and 'build-history' not in p.parts and p not in [exe,out/'build.json']}
j={'status':'BUILD_ONLY_PASS' if rc==0 else 'BUILD_ONLY_FAIL','exit_code':rc,'sources':post,'sources_pre_compile':sources,'sources_pre_post_equal':sources==post,'readonly_source_pins':deps,'generated':generated,'build_tools':{v:sha(v) for v in [cc,'/usr/bin/spirv-val',str(Path(sys.executable).resolve())]},'artifact':str(exe),'artifact_sha256':sha(exe) if not rc else None,'steps':steps,'CPU_checks':checks,'case_bundle':str(case),'case_bundle_sha256':sha(case),'r2_SPIRV_sha256':sha(R2/'out/sky_cache.spv'),'planned_jobs':13,'accepted_positive_jobs':6,'negative_jobs':7,'model_evaluate_calls':0,'runtime':'NOT_RUN','Wine':'NOT_RUN','GPU':'NOT_RUN','phase':'KNOWN_SYNTHETIC_ONLY_REAL_720_1080_UNVERIFIED','native_async_delivery':'NOT_IMPLEMENTED','scope':'GPU owned late-origin fixture only, exact same shader; no partial Apply, retain immutable original to actual last use','history':str(history)}
(out/'build.json').write_text(json.dumps(j,indent=2)+'\n');(history/'build.json').write_text(json.dumps(j,indent=2)+'\n');(history/'case-bundle.json').write_bytes(case.read_bytes())
if not rc:(history/exe.name).write_bytes(exe.read_bytes())
print(json.dumps({'status':j['status'],'build_sha256':sha(out/'build.json'),'artifact_sha256':j['artifact_sha256'],'CPU_checks':checks['checks_count'],'jobs':13}));sys.exit(rc)
