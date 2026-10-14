#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Offline CPU/PE compilation only, no Wine or GPU calls.
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys
import argparse
ROOT=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--contract',type=Path,required=True);p.add_argument('--sha256',required=True);args=p.parse_args();CANON=args.contract.resolve()
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert args.contract.is_absolute() and sha(CANON)==args.sha256=='921101a4b771f3ec2e303da2b64d581f552ba1abd96af70759cf810ed9985371'
child=json.loads(CANON.read_text());assert child['schema']=='native-async-continuous-play-delta-v1' and child['revision']==1 and child['max_jobs']==16384
BASE_CANON=Path(child['base_interface']['path']);assert sha(BASE_CANON)==child['base_interface']['sha256']=='5f4da5793c45d94181f1719d987b3f9c00c77faa0f26583304267a4f0de3af58'
spec=json.loads(BASE_CANON.read_text());assert spec['interface_revision']==3
# Apply only Root's frozen JSON pointers, then the explicitly stated metadata declaration.
for pointer,value in child['deterministic_overrides'].items():
 parts=pointer.split('/')[1:];target=spec
 for part in parts[:-1]:target=target[part.replace('~1','/').replace('~0','~')]
 key=parts[-1].replace('~1','/').replace('~0','~');assert key in target;target[key]=value
spec['bootstrap_exact']['async-meta-g0.json']['required_stage_fields']['max_jobs']='uint exact16384'
assert spec['profile_id']==child['profile_id'] and spec['bootstrap_exact']['async-config.json']['exact_sample']['max_jobs']==16384 and spec['budgets']['service_wall_seconds']==600
ORIG=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-highfps-worktree/poc/game_nr/async-origin')
BASE=ORIG/'out/build.json';assert sha(BASE)=='7dbe1aa68f45b08d9e29328d25e1f215f9d78c3854591f53c9fd21a82943e5bc';b=json.loads(BASE.read_text());pins=b['readonly_dependency_pins'];deps={**b['sources'],**b['readonly_source_pins'],**b['generated'],str(BASE):sha(BASE),b['artifact']:b['artifact_sha256'],str(CANON):sha(CANON),str(BASE_CANON):sha(BASE_CANON)}
for q in [ROOT.parent/'playable-quality/playable_transport.h',ROOT.parent/'playable-quality/playable_runtime_types.h',ROOT.parent/'c2-baseline/fixture_util.h',ROOT.parent/'high-fps-fixture/lowres_production_adapter.h']:deps[str(q)]=sha(q)
assert (ROOT/'origin_frame.h').read_bytes()==(ORIG/'origin_frame.h').read_bytes()
def verify():
 for p,h in deps.items():assert Path(p).is_file() and sha(p)==h,p
verify();out=ROOT/'out';out.mkdir(exist_ok=True);header=out/'async_spec.h';effective=out/'effective-generated-spec.json';effective.write_text(json.dumps(spec,indent=2)+'\n');header.write_text('#pragma once\nstatic const char async_spec_json[]=R"ASYNCSPEC('+json.dumps(spec,indent=2)+')ASYNCSPEC";\n');names=['.gitignore','README.md','async_wire.h','origin_frame.h','wine_origin_nr_play.cpp','wire_check.cpp','build_play.py','SOURCE-EVIDENCE.json'];pre={str(ROOT/n):sha(ROOT/n) for n in names};history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True);steps=[]
def run(name,cmd):
 try:r=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);rc=r.returncode;data=r.stdout
 except subprocess.TimeoutExpired as e:rc=124;data=e.stdout or b''
 p=history/(name+'.log');p.write_bytes(data);steps.append({'name':name,'command':cmd,'exit_code':rc,'log':str(p)});print(json.dumps(steps[-1]),flush=True)
 if rc:print(data.decode(errors='replace')[:16000],flush=True)
 return rc
cc=Path(pins['compiler']['path']);hostcc=Path('/usr/bin/c++');jh=Path(pins['json_header']['path']);checker=out/'wire_check';exe=out/'wine_origin_nr_play.exe';rc=run('CPU-wire-compile',[str(hostcc),'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(jh.parent),str(ROOT/'wire_check.cpp'),'-o',str(checker)]);checks={}
if not rc:
 rc=run('CPU-wire-tests',[str(checker)])
 if not rc:checks=json.loads((history/'CPU-wire-tests.log').read_text())
if not rc:rc=run('PE-async-play-compile',[str(cc),'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+pins['sdk_root'],'-I'+pins['ngx_sdk_root'],'-I'+str(jh.parent),str(ROOT/'wine_origin_nr_play.cpp'),'-o',str(exe)])
post={str(ROOT/n):sha(ROOT/n) for n in names};assert post==pre;verify()
for n in names:(history/n).write_bytes((ROOT/n).read_bytes())
(history/'async_spec.h').write_bytes(header.read_bytes());(history/'canonical.json').write_bytes(CANON.read_bytes());(history/'base-canonical.json').write_bytes(BASE_CANON.read_bytes());(history/'effective-generated-spec.json').write_bytes(effective.read_bytes())
if not rc:(history/exe.name).write_bytes(exe.read_bytes())
j={'status':'BUILD_ONLY_PASS' if not rc else 'BUILD_ONLY_FAIL','exit_code':rc,'sources':post,'sources_pre_compile':pre,'sources_pre_post_equal':post==pre,'readonly_source_pins':deps,'generated':{str(header):sha(header),str(effective):sha(effective),**({str(checker):sha(checker)} if checker.exists() else {})},'build_tools':{str(p):sha(p) for p in [cc,hostcc,Path(sys.executable).resolve()]},'readonly_dependency_pins':pins,'current_runtime_pins':b['current_runtime_pins'],'old_runtime_drift_preserved':b['old_runtime_drift_preserved'],'artifact':str(exe),'artifact_sha256':sha(exe) if not rc else None,'artifact_bytes':exe.stat().st_size if not rc else None,'contract':str(CANON),'contract_sha256':sha(CANON),'base_contract':str(BASE_CANON),'base_contract_sha256':sha(BASE_CANON),'effective_spec':str(effective),'effective_spec_sha256':sha(effective),'profile_id':spec['profile_id'],'steps':steps,'CPU_checks':checks,'runtime':'NOT_RUN','Wine':'NOT_RUN','GPU':'NOT_RUN','protocol_version':1,'interface_revision':3,'model_extent':[1280,720],'guide_extent':[1280,720],'color_output_extent':[1920,1080],'max_jobs':16384,'working_scale':2/3,'parameter_application':spec['parameter_application'],'hot_pixel_readback_bytes':0,'budgets_ms':{'cold':120000,'job':30000,'fence':10000,'control':60000,'wall':600000},'scope':'Continuous backup cap only; production bytes unchanged12199; stage-specific bootstrap/idle and actual-N terminal; Native Sky/cache/120s selected Present window independently measured','history':str(history)}
(out/'build.json').write_text(json.dumps(j,indent=2)+'\n');(history/'build.json').write_text(json.dumps(j,indent=2)+'\n');print(json.dumps({'status':j['status'],'build_sha256':sha(out/'build.json'),'artifact_sha256':j['artifact_sha256'],'CPU':checks}));sys.exit(rc)
