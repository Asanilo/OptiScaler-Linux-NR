#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Offline CPU/PE compilation only, no Wine or GPU calls.
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys
ROOT=Path(__file__).resolve().parent;CANON=ROOT.parents[3]/'integration/high-fps-20261013/async-origin-interface-v1.json'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert sha(CANON)=='5f4da5793c45d94181f1719d987b3f9c00c77faa0f26583304267a4f0de3af58';spec=json.loads(CANON.read_text());assert spec['interface_revision']==3
BASE=ROOT.parent/'high-fps-fixture/out/build.json';assert sha(BASE)=='ab828b20c1c37999fda8b8b6fab849f735281e0101f7ffe1fd2eab455cdc4eb4';b=json.loads(BASE.read_text());pins=b['readonly_dependency_pins'];deps={**b['sources'],**b['readonly_source_pins'],str(BASE):sha(BASE),b['artifact']:b['artifact_sha256'],str(CANON):sha(CANON)}
for q in [ROOT.parent/'playable-quality/playable_transport.h',ROOT.parent/'playable-quality/playable_runtime_types.h',ROOT.parent/'c2-baseline/fixture_util.h']:deps[str(q)]=sha(q)
def verify():
 for p,h in deps.items():assert Path(p).is_file() and sha(p)==h,p
verify();out=ROOT/'out';out.mkdir(exist_ok=True);header=out/'async_spec.h';header.write_text('#pragma once\nstatic const char async_spec_json[]=R"ASYNCSPEC('+CANON.read_text()+')ASYNCSPEC";\n');names=['.gitignore','README.md','async_wire.h','origin_frame.h','wine_origin_nr.cpp','wire_check.cpp','build_origin.py'];pre={str(ROOT/n):sha(ROOT/n) for n in names};history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True);steps=[]
def run(name,cmd):
 try:r=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);rc=r.returncode;data=r.stdout
 except subprocess.TimeoutExpired as e:rc=124;data=e.stdout or b''
 p=history/(name+'.log');p.write_bytes(data);steps.append({'name':name,'command':cmd,'exit_code':rc,'log':str(p)});print(json.dumps(steps[-1]),flush=True)
 if rc:print(data.decode(errors='replace')[:16000],flush=True)
 return rc
cc=Path(pins['compiler']['path']);hostcc=Path('/usr/bin/c++');jh=Path(pins['json_header']['path']);checker=out/'wire_check';exe=out/'wine_origin_nr.exe';rc=run('CPU-wire-compile',[str(hostcc),'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(jh.parent),str(ROOT/'wire_check.cpp'),'-o',str(checker)]);checks={}
if not rc:
 rc=run('CPU-wire-tests',[str(checker)])
 if not rc:checks=json.loads((history/'CPU-wire-tests.log').read_text())
if not rc:rc=run('PE-origin-compile',[str(cc),'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+pins['sdk_root'],'-I'+pins['ngx_sdk_root'],'-I'+str(jh.parent),str(ROOT/'wine_origin_nr.cpp'),'-o',str(exe)])
post={str(ROOT/n):sha(ROOT/n) for n in names};assert post==pre;verify()
for n in names:(history/n).write_bytes((ROOT/n).read_bytes())
(history/'async_spec.h').write_bytes(header.read_bytes());(history/'canonical.json').write_bytes(CANON.read_bytes())
if not rc:(history/exe.name).write_bytes(exe.read_bytes())
j={'status':'BUILD_ONLY_PASS' if not rc else 'BUILD_ONLY_FAIL','exit_code':rc,'sources':post,'sources_pre_compile':pre,'sources_pre_post_equal':post==pre,'readonly_source_pins':deps,'generated':{str(header):sha(header),**({str(checker):sha(checker)} if checker.exists() else {})},'build_tools':{str(p):sha(p) for p in [cc,hostcc,Path(sys.executable).resolve()]},'readonly_dependency_pins':pins,'current_runtime_pins':b['current_runtime_pins'],'old_runtime_drift_preserved':b['old_runtime_drift_preserved'],'artifact':str(exe),'artifact_sha256':sha(exe) if not rc else None,'artifact_bytes':exe.stat().st_size if not rc else None,'contract':str(CANON),'contract_sha256':sha(CANON),'steps':steps,'CPU_checks':checks,'runtime':'NOT_RUN','Wine':'NOT_RUN','GPU':'NOT_RUN','protocol_version':1,'interface_revision':3,'model_extent':[1280,720],'guide_extent':[1280,720],'color_output_extent':[1920,1080],'max_jobs':4,'working_scale':2/3,'parameter_application':spec['parameter_application'],'hot_pixel_readback_bytes':0,'budgets_ms':{'cold':120000,'job':30000,'fence':10000,'control':60000,'wall':300000},'scope':'Source/CPU/build only; stage-specific bootstrap and actual-N terminal; Native Sky cache separate','history':str(history)}
(out/'build.json').write_text(json.dumps(j,indent=2)+'\n');(history/'build.json').write_text(json.dumps(j,indent=2)+'\n');print(json.dumps({'status':j['status'],'build_sha256':sha(out/'build.json'),'artifact_sha256':j['artifact_sha256'],'CPU':checks}));sys.exit(rc)
