#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Offline CPU/PE build only. Root owns sole canonical; no private profile generator.
from pathlib import Path
from datetime import datetime,timezone
import argparse,hashlib,json,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--contract',type=Path,required=True);p.add_argument('--sha256',required=True);args=p.parse_args();ROOT=Path(__file__).resolve().parent;CANON=args.contract.resolve();sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert args.contract.is_absolute() and sha(CANON)==args.sha256=='a35eb054ec9c94011dbab42c0fa9ec3b1b5799451390abf76245b94d134b6103'
# Root child delta is required. Never invent profile/message literals in this builder.
child=json.loads(CANON.read_text());assert child['schema']=='async-pre-sr-minimal-delta-v1' and child['revision']==1
assert child.get('helper_base_commit')=='619ee82e531f7a5a339f85f1938481eb1b717cc7', 'Root helper base must be frozen619ee'
PARENT=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-pre-sr-worktree/poc/game_nr/pre-sr-game')
PARENT_BUILD=PARENT/'out/build.json'
assert sha(PARENT_BUILD)=='31988f9f753502f9eed0d6249ac1e4eed594ec0c4a069cb0d707fafeeaab5fdb'
import re
base_header=Path(child['base_effective_header']['path']);assert base_header==PARENT/'out/pre_sr_spec.h' and sha(base_header)==child['base_effective_header']['sha256']=='8cc2fae3d946a595f4033b79100014da46f97847bbf366c01615850b881184fc' and base_header.stat().st_size==child['base_effective_header']['bytes'];text=base_header.read_text();match=re.search(r'R"([^ (]*)\((.*)\)\1"',text,re.S);assert match
spec=json.loads(match.group(2));BASE_CANON=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/integration/high-fps-20261013/async-origin-interface-v1.json')
assert sha(BASE_CANON)=='5f4da5793c45d94181f1719d987b3f9c00c77faa0f26583304267a4f0de3af58'
assert isinstance(child.get('deterministic_overrides'),dict) and len(child['deterministic_overrides'])==6, 'Root exact JSON pointers required'
for pointer,value in child['deterministic_overrides'].items():
 assert pointer.startswith('/');parts=[x.replace('~1','/').replace('~0','~') for x in pointer.split('/')[1:]];target=spec
 for part in parts[:-1]:target=target[part]
 assert parts[-1] in target;target[parts[-1]]=value
sample=spec['bootstrap_exact']['async-config.json']['exact_sample'];profile=spec['quality_profile']
assert spec['profile_id']==child['profile_id'] and sample['profile_id']==child['profile_id']
assert sample['quality_profile']==profile and type(sample['max_jobs']) is int and sample['max_jobs']==4
assert all(profile[k]==v for k,v in {'color_width':1280,'color_height':720,'guide_width':1280,'guide_height':720,'model_width':1280,'model_height':720,'output_width':1280,'output_height':720,'working_scale':1.0,'passthrough':False,'white_point':1.0}.items())
assert sample['service_wall_seconds']==300 and spec['budgets']['service_wall_seconds']==300
# First4 sparse jobs preserve old actual-N typed limit, one bucket and all model/resource parameters.
parent=json.loads(PARENT_BUILD.read_text())

ORIG=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-highfps-worktree/poc/game_nr');BASE=ORIG/'async-origin/out/build.json';assert sha(BASE)=='7dbe1aa68f45b08d9e29328d25e1f215f9d78c3854591f53c9fd21a82943e5bc';base=json.loads(BASE.read_text());pins=base['readonly_dependency_pins'];deps={**base['sources'],**base['readonly_source_pins'],str(BASE):sha(BASE),base['artifact']:base['artifact_sha256'],str(CANON):sha(CANON),str(BASE_CANON):sha(BASE_CANON),str(PARENT_BUILD):sha(PARENT_BUILD),**parent['sources'],**parent['generated'],parent['artifact']:parent['artifact_sha256']}
def verify():
 for p,h in deps.items():assert Path(p).is_file() and sha(p)==h,p
verify();out=ROOT/'out';out.mkdir(exist_ok=True);header=out/'pre_sr_spec.h';effective=out/'effective-generated-spec.json';effective.write_text(json.dumps(spec,indent=2)+'\n');header.write_text('#pragma once\nstatic const char pre_sr_spec_json[]=R"ASYNCSPEC('+json.dumps(spec,indent=2)+')ASYNCSPEC";\n');names=['.gitignore','README.md','SOURCE-EVIDENCE.json','pre_sr_wire.h','pre_sr_frame.h','pre_sr_transport.h','wine_async_presr_nr.cpp','wire_check.cpp','build_async_presr.py'];pre={str(ROOT/n):sha(ROOT/n) for n in names};history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True);steps=[]
def run(name,cmd):
 try:r=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);rc=r.returncode;data=r.stdout
 except subprocess.TimeoutExpired as e:rc=124;data=e.stdout or b''
 q=history/(name+'.log');q.write_bytes(data);steps.append({'name':name,'command':cmd,'exit_code':rc,'log':str(q)});print(json.dumps(steps[-1]),flush=True)
 if rc:print(data.decode(errors='replace')[:16000],flush=True)
 return rc
cc=Path(pins['compiler']['path']);hostcc=Path('/usr/bin/c++');jh=Path(pins['json_header']['path']);checker=out/'wire_check';exe=out/'wine_async_presr_nr.exe';rc=run('CPU-wire-compile',[str(hostcc),'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(jh.parent),str(ROOT/'wire_check.cpp'),'-o',str(checker)]);checks={}
if not rc:
 rc=run('CPU-wire-tests',[str(checker)])
 if not rc:checks=json.loads((history/'CPU-wire-tests.log').read_text())
if not rc:rc=run('PE-async-presr-compile',[str(cc),'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+pins['sdk_root'],'-I'+pins['ngx_sdk_root'],'-I'+str(jh.parent),str(ROOT/'wine_async_presr_nr.cpp'),'-o',str(exe)])
post={str(ROOT/n):sha(ROOT/n) for n in names};assert post==pre;verify()
for n in names:(history/n).write_bytes((ROOT/n).read_bytes())
(history/'pre_sr_spec.h').write_bytes(header.read_bytes());(history/'canonical-delta.json').write_bytes(CANON.read_bytes());(history/'canonical-base.json').write_bytes(BASE_CANON.read_bytes());(history/'effective-generated-spec.json').write_text(json.dumps(spec,indent=2)+'\n')
if not rc:(history/exe.name).write_bytes(exe.read_bytes())
j={'status':'BUILD_ONLY_PASS' if not rc else 'BUILD_ONLY_FAIL','exit_code':rc,'sources':post,'sources_pre_compile':pre,'sources_pre_post_equal':post==pre,'readonly_source_pins':deps,'generated':{str(header):sha(header),str(effective):sha(effective),**({str(checker):sha(checker)} if checker.exists() else {})},'build_tools':{str(p):sha(p) for p in [cc,hostcc,Path(sys.executable).resolve()]},'readonly_dependency_pins':pins,'current_runtime_pins':base['current_runtime_pins'],'old_runtime_drift_preserved':base['old_runtime_drift_preserved'],'artifact':str(exe),'artifact_sha256':sha(exe) if not rc else None,'artifact_bytes':exe.stat().st_size if not rc else None,'contract':str(CANON),'contract_sha256':sha(CANON),'effective_spec':str(effective),'effective_spec_sha256':sha(effective),'base_contract':str(BASE_CANON),'base_contract_sha256':sha(BASE_CANON),'base_effective_header':str(base_header),'base_effective_header_sha256':sha(base_header),'delta_revision':child['revision'],'effective_config_is_generated_from_Root_base_plus_delta':True,'steps':steps,'CPU_checks':checks,'runtime':'NOT_RUN','Wine':'NOT_RUN','GPU':'NOT_RUN','protocol_version':1,'profile_id':spec['profile_id'],'interface_revision':spec['interface_revision'],'model_extent':[1280,720],'guide_extent':[1280,720],'color_output_extent':[1280,720],'Native_SR_output_extent':[1920,1080],'working_scale':1.0,'max_jobs':4,'parameter_application':spec['parameter_application'],'hot_pixel_readback_bytes':0,'production_modes':['Encode0','Evaluate_API','Resolve1'],'downsample_calls':0,'budgets_ms':{'cold':120000,'job':30000,'fence':10000,'control':60000,'wall':300000},'scope':'Immutable origin original-render720 sparse NR only; Native real lineage/catchup/current render720 Apply and DLSS SR1080 independently required','history':str(history)}
(out/'build.json').write_text(json.dumps(j,indent=2)+'\n');(history/'build.json').write_text(json.dumps(j,indent=2)+'\n');print(json.dumps({'status':j['status'],'build_sha256':sha(out/'build.json'),'artifact_sha256':j['artifact_sha256'],'CPU':checks}));sys.exit(rc)
