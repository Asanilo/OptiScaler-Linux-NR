#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Offline CPU/PE build only. Root owns sole canonical; no private profile generator.
from pathlib import Path
from datetime import datetime,timezone
import argparse,hashlib,json,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--contract',type=Path,required=True);p.add_argument('--sha256',required=True);args=p.parse_args();ROOT=Path(__file__).resolve().parent;CANON=args.contract.resolve();sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert args.contract.is_absolute() and sha(CANON)==args.sha256=='902026b106ccb9ff170f6aad84a3ceeee11587334ad60b506ffa87f95e952a92'
child=json.loads(CANON.read_text());assert child['schema']=='async-pre-sr-continuous-minimal-delta-v1' and child['revision']==1
assert child['Helper_base_commit']=='57b62ecc31cb5abbb4cde9225bc034c1caa05a92'
PARENT=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-async-pre-sr-worktree/poc/game_nr/async-pre-sr')
PARENT_BUILD=PARENT/'out/build.json';assert sha(PARENT_BUILD)=='f7e85ba9fd4dab1c6f22cc94d6a3924f4f5c5c9834503351bc75c3bef0847822'
base_effective=Path(child['base_effective_spec']['path']);assert base_effective==PARENT/'out/effective-generated-spec.json' and sha(base_effective)==child['base_effective_spec']['sha256']=='a501dc377bf388dfd1c3eadff973dc096d55134823547b37ef6050a38c325cb4'
base_header=PARENT/'out/pre_sr_spec.h'
original=json.loads(base_effective.read_text());spec=json.loads(base_effective.read_text())
BASE_CANON=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/integration/high-fps-20261013/async-pre-sr-minimal-delta-v1.json');assert sha(BASE_CANON)=='a35eb054ec9c94011dbab42c0fa9ec3b1b5799451390abf76245b94d134b6103'
assert isinstance(child['deterministic_overrides'],dict) and len(child['deterministic_overrides'])==10
for pointer,value in child['deterministic_overrides'].items():
 assert pointer.startswith('/');parts=[x.replace('~1','/').replace('~0','~') for x in pointer.split('/')[1:]];target=spec
 for part in parts[:-1]:target=target[part]
 assert parts[-1] in target and target[parts[-1]]!=value;target[parts[-1]]=value
def differences(a,b,p=''):
 if isinstance(a,dict) and isinstance(b,dict):
  assert a.keys()==b.keys();return [q for k in a for q in differences(a[k],b[k],p+'/'+k)]
 return [] if a==b else [p]
assert set(differences(original,spec))==set(child['deterministic_overrides'])
sample=spec['bootstrap_exact']['async-config.json']['exact_sample'];profile=spec['quality_profile']
assert spec['profile_id']==sample['profile_id']=='async-pre-sr-continuous-hdr720-j1-experimental-v1'
assert profile==original['quality_profile']==sample['quality_profile']
assert type(sample['max_jobs']) is int and sample['max_jobs']==spec['first_runtime_limits']['max_jobs']==16384
assert sample['service_wall_seconds']==spec['budgets']['service_wall_seconds']==spec['first_runtime_limits']['watchdog_seconds']==600
assert spec['helper_directory']=='poc/game_nr/async-pre-sr-play' and spec['helper_executable']=='wine_async_presr_play.exe'
parent=json.loads(PARENT_BUILD.read_text())
for n in ['pre_sr_frame.h','pre_sr_transport.h']:assert sha(ROOT/n)==sha(PARENT/n),'production must remain byte-identical57b'

ORIG=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-highfps-worktree/poc/game_nr');BASE=ORIG/'async-origin/out/build.json';assert sha(BASE)=='7dbe1aa68f45b08d9e29328d25e1f215f9d78c3854591f53c9fd21a82943e5bc';base=json.loads(BASE.read_text());pins=base['readonly_dependency_pins'];deps={**base['sources'],**base['readonly_source_pins'],str(BASE):sha(BASE),base['artifact']:base['artifact_sha256'],str(CANON):sha(CANON),str(BASE_CANON):sha(BASE_CANON),str(PARENT_BUILD):sha(PARENT_BUILD),str(base_effective):sha(base_effective),**parent['sources'],**parent['generated'],parent['artifact']:parent['artifact_sha256']}
def verify():
 for p,h in deps.items():assert Path(p).is_file() and sha(p)==h,p
verify();out=ROOT/'out';out.mkdir(exist_ok=True);header=out/'pre_sr_spec.h';effective=out/'effective-generated-spec.json';effective.write_text(json.dumps(spec,indent=2)+'\n');header.write_text('#pragma once\nstatic const char pre_sr_spec_json[]=R"ASYNCSPEC('+json.dumps(spec,indent=2)+')ASYNCSPEC";\n');names=['.gitignore','README.md','SOURCE-EVIDENCE.json','pre_sr_wire.h','pre_sr_frame.h','pre_sr_transport.h','wine_async_presr_play.cpp','wire_check.cpp','build_async_presr_play.py'];pre={str(ROOT/n):sha(ROOT/n) for n in names};history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True);steps=[]
def run(name,cmd):
 try:r=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);rc=r.returncode;data=r.stdout
 except subprocess.TimeoutExpired as e:rc=124;data=e.stdout or b''
 q=history/(name+'.log');q.write_bytes(data);steps.append({'name':name,'command':cmd,'exit_code':rc,'log':str(q)});print(json.dumps(steps[-1]),flush=True)
 if rc:print(data.decode(errors='replace')[:16000],flush=True)
 return rc
cc=Path(pins['compiler']['path']);hostcc=Path('/usr/bin/c++');jh=Path(pins['json_header']['path']);checker=out/'wire_check';exe=out/'wine_async_presr_play.exe';rc=run('CPU-wire-compile',[str(hostcc),'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(jh.parent),str(ROOT/'wire_check.cpp'),'-o',str(checker)]);checks={}
if not rc:
 rc=run('CPU-wire-tests',[str(checker)])
 if not rc:checks=json.loads((history/'CPU-wire-tests.log').read_text())
if not rc:rc=run('PE-async-presr-play-compile',[str(cc),'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+pins['sdk_root'],'-I'+pins['ngx_sdk_root'],'-I'+str(jh.parent),str(ROOT/'wine_async_presr_play.cpp'),'-o',str(exe)])
post={str(ROOT/n):sha(ROOT/n) for n in names};assert post==pre;verify()
for n in names:(history/n).write_bytes((ROOT/n).read_bytes())
(history/'pre_sr_spec.h').write_bytes(header.read_bytes());(history/'canonical-delta.json').write_bytes(CANON.read_bytes());(history/'canonical-base.json').write_bytes(BASE_CANON.read_bytes());(history/'effective-generated-spec.json').write_text(json.dumps(spec,indent=2)+'\n')
if not rc:(history/exe.name).write_bytes(exe.read_bytes())
j={'status':'BUILD_ONLY_PASS' if not rc else 'BUILD_ONLY_FAIL','exit_code':rc,'sources':post,'sources_pre_compile':pre,'sources_pre_post_equal':post==pre,'readonly_source_pins':deps,'generated':{str(header):sha(header),str(effective):sha(effective),**({str(checker):sha(checker)} if checker.exists() else {})},'build_tools':{str(p):sha(p) for p in [cc,hostcc,Path(sys.executable).resolve()]},'readonly_dependency_pins':pins,'current_runtime_pins':base['current_runtime_pins'],'old_runtime_drift_preserved':base['old_runtime_drift_preserved'],'artifact':str(exe),'artifact_sha256':sha(exe) if not rc else None,'artifact_bytes':exe.stat().st_size if not rc else None,'contract':str(CANON),'contract_sha256':sha(CANON),'effective_spec':str(effective),'effective_spec_sha256':sha(effective),'base_contract':str(BASE_CANON),'base_contract_sha256':sha(BASE_CANON),'base_effective_header':str(base_header),'base_effective_header_sha256':sha(base_header),'base_effective_spec':str(base_effective),'base_effective_spec_sha256':sha(base_effective),'exact_changed_JSON_pointers':differences(original,spec),'prespawn_bucket_count':256,'delta_revision':child['revision'],'effective_config_is_generated_from_Root_base_plus_delta':True,'steps':steps,'CPU_checks':checks,'runtime':'NOT_RUN','Wine':'NOT_RUN','GPU':'NOT_RUN','protocol_version':1,'profile_id':spec['profile_id'],'interface_revision':spec['interface_revision'],'model_extent':[1280,720],'guide_extent':[1280,720],'color_output_extent':[1280,720],'Native_SR_output_extent':[1920,1080],'working_scale':1.0,'max_jobs':16384,'parameter_application':spec['parameter_application'],'hot_pixel_readback_bytes':0,'production_modes':['Encode0','Evaluate_API','Resolve1'],'downsample_calls':0,'budgets_ms':{'cold':120000,'job':30000,'fence':10000,'control':60000,'wall':600000},'scope':'Immutable origin original-render720 sparse NR only; Native real lineage/catchup/current render720 Apply and DLSS SR1080 independently required','history':str(history)}
(out/'build.json').write_text(json.dumps(j,indent=2)+'\n');(history/'build.json').write_text(json.dumps(j,indent=2)+'\n');print(json.dumps({'status':j['status'],'build_sha256':sha(out/'build.json'),'artifact_sha256':j['artifact_sha256'],'CPU':checks}));sys.exit(rc)
