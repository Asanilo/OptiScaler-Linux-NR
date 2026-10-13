#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Offline CPU/PE build only. Root owns sole canonical; no private profile generator.
from pathlib import Path
from datetime import datetime,timezone
import argparse,hashlib,json,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--contract',type=Path,required=True);p.add_argument('--sha256',required=True);args=p.parse_args();ROOT=Path(__file__).resolve().parent;CANON=args.contract.resolve();sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert args.contract.is_absolute() and sha(CANON)==args.sha256=='9e236dff1bf695c6dcf5f5024b219ffb11371e288299da652f7ce83c84ba8fda'
child=json.loads(CANON.read_text());assert child['schema']=='pre-sr-continuous120-delta-v1' and child['revision']==1
assert child['max_jobs']==120 and type(child['max_jobs']) is int
PARENT_CANON=Path(child['base_pre_sr_delta']['path']);assert PARENT_CANON.is_absolute() and sha(PARENT_CANON)==child['base_pre_sr_delta']['sha256']=='ad2e589a36bdb59b6425d2842572dd0a8acbbbd6fc7fecdcbbc398e4b72d0770'
delta=json.loads(PARENT_CANON.read_text());assert delta['schema']=='pre-sr-minimal-delta-v1' and delta['revision']==2
assert child['base_interface_sha256']==delta['base_interface_sha256'] and child['helper_base_commit']=='619ee82e531f7a5a339f85f1938481eb1b717cc7'
BASE_CANON=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/integration/high-fps-20261013/async-origin-interface-v1.json');assert sha(BASE_CANON)==delta['base_interface_sha256']=='5f4da5793c45d94181f1719d987b3f9c00c77faa0f26583304267a4f0de3af58'
spec=json.loads(BASE_CANON.read_text());assert spec['interface_revision']==delta['base_revision']==3
# Deterministic application of Root's small delta, not an independent canonical.
spec['profile_id']=delta['profile_id'];spec['quality_profile'].update(delta['quality_profile_overrides'])
sample=spec['bootstrap_exact']['async-config.json']['exact_sample'];sample['profile_id']=spec['profile_id'];sample['quality_profile']=spec['quality_profile'].copy()
for k,v in delta['config_overrides'].items():
 if k=='parameters.working_scale':sample['parameters']['working_scale']=v
 else:sample[k]=v
for k in ['width','height','render_width','render_height','model_width','model_height']:sample[k]=delta['resource_override'][k]
# Update inherited per-role/metadata extent declarations using only Root's resource override.
for role,oldname in [('color','Color'),('depth','Depth'),('mv','MotionVectors'),('output','Output')]:
 spec['shared_images']['roles'][oldname]=delta['resource_override']['roles'][role].copy()
fields=spec['bootstrap_exact']['async-meta-g0.json']['required_stage_fields']
for k in ['width','height','render_width','render_height','model_width','model_height']:
 fields[k]='uint exact'+str(delta['resource_override'][k])
# The only child changes to the effective v3 spec: exact cap declarations.
sample['max_jobs']=child['max_jobs'];spec['first_runtime_limits']['max_jobs']=child['max_jobs'];fields['max_jobs']='uint exact120'
profile=spec['quality_profile'];assert spec['profile_id']=='pre-sr-quality-hdr720-j1-experimental-v1'
assert all(profile[k]==v for k,v in {'color_width':1280,'color_height':720,'guide_width':1280,'guide_height':720,'model_width':1280,'model_height':720,'output_width':1280,'output_height':720,'working_scale':1.0,'passthrough':False,'white_point':1.0}.items());assert type(profile['passthrough'])==bool

ORIG=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-highfps-worktree/poc/game_nr');BASE=ORIG/'async-origin/out/build.json';assert sha(BASE)=='7dbe1aa68f45b08d9e29328d25e1f215f9d78c3854591f53c9fd21a82943e5bc';base=json.loads(BASE.read_text());pins=base['readonly_dependency_pins'];deps={**base['sources'],**base['readonly_source_pins'],str(BASE):sha(BASE),base['artifact']:base['artifact_sha256'],str(CANON):sha(CANON),str(BASE_CANON):sha(BASE_CANON)}
FROZEN_BUILD=ROOT.parent/'pre-sr-game/out/build.json';assert sha(FROZEN_BUILD)=='31988f9f753502f9eed0d6249ac1e4eed594ec0c4a069cb0d707fafeeaab5fdb'
frozen=json.loads(FROZEN_BUILD.read_text());deps.update({**frozen['sources'],**frozen['generated'],str(FROZEN_BUILD):sha(FROZEN_BUILD),frozen['artifact']:frozen['artifact_sha256'],str(PARENT_CANON):sha(PARENT_CANON)})
def verify():
 for p,h in deps.items():assert Path(p).is_file() and sha(p)==h,p
verify();out=ROOT/'out';out.mkdir(exist_ok=True);header=out/'pre_sr_spec.h';effective=out/'effective-generated-spec.json';effective.write_text(json.dumps(spec,indent=2)+'\n');header.write_text('#pragma once\nstatic const char pre_sr_spec_json[]=R"ASYNCSPEC('+json.dumps(spec,indent=2)+')ASYNCSPEC";\n');names=['.gitignore','README.md','SOURCE-EVIDENCE.json','pre_sr_wire.h','pre_sr_frame.h','pre_sr_transport.h','wine_presr_nr120.cpp','wire_check.cpp','build_presr120.py'];pre={str(ROOT/n):sha(ROOT/n) for n in names};history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True);steps=[]
def run(name,cmd):
 try:r=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);rc=r.returncode;data=r.stdout
 except subprocess.TimeoutExpired as e:rc=124;data=e.stdout or b''
 q=history/(name+'.log');q.write_bytes(data);steps.append({'name':name,'command':cmd,'exit_code':rc,'log':str(q)});print(json.dumps(steps[-1]),flush=True)
 if rc:print(data.decode(errors='replace')[:16000],flush=True)
 return rc
cc=Path(pins['compiler']['path']);hostcc=Path('/usr/bin/c++');jh=Path(pins['json_header']['path']);checker=out/'wire_check';exe=out/'wine_presr_nr120.exe';rc=run('CPU-wire-compile',[str(hostcc),'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(jh.parent),str(ROOT/'wire_check.cpp'),'-o',str(checker)]);checks={}
if not rc:
 rc=run('CPU-wire-tests',[str(checker)])
 if not rc:checks=json.loads((history/'CPU-wire-tests.log').read_text())
if not rc:rc=run('PE-presr120-compile',[str(cc),'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+pins['sdk_root'],'-I'+pins['ngx_sdk_root'],'-I'+str(jh.parent),str(ROOT/'wine_presr_nr120.cpp'),'-o',str(exe)])
post={str(ROOT/n):sha(ROOT/n) for n in names};assert post==pre;verify()
for n in names:(history/n).write_bytes((ROOT/n).read_bytes())
(history/'pre_sr_spec.h').write_bytes(header.read_bytes());(history/'canonical-delta.json').write_bytes(CANON.read_bytes());(history/'canonical-base.json').write_bytes(BASE_CANON.read_bytes());(history/'canonical-parent-delta.json').write_bytes(PARENT_CANON.read_bytes());(history/'effective-generated-spec.json').write_text(json.dumps(spec,indent=2)+'\n')
if not rc:(history/exe.name).write_bytes(exe.read_bytes())
j={'status':'BUILD_ONLY_PASS' if not rc else 'BUILD_ONLY_FAIL','exit_code':rc,'sources':post,'sources_pre_compile':pre,'sources_pre_post_equal':post==pre,'readonly_source_pins':deps,'generated':{str(header):sha(header),str(effective):sha(effective),**({str(checker):sha(checker)} if checker.exists() else {})},'build_tools':{str(p):sha(p) for p in [cc,hostcc,Path(sys.executable).resolve()]},'readonly_dependency_pins':pins,'current_runtime_pins':base['current_runtime_pins'],'old_runtime_drift_preserved':base['old_runtime_drift_preserved'],'artifact':str(exe),'artifact_sha256':sha(exe) if not rc else None,'artifact_bytes':exe.stat().st_size if not rc else None,'contract':str(CANON),'contract_sha256':sha(CANON),'base_contract':str(BASE_CANON),'base_contract_sha256':sha(BASE_CANON),'delta_revision':child['revision'],'parent_contract':str(PARENT_CANON),'parent_contract_sha256':sha(PARENT_CANON),'effective_spec':str(effective),'effective_spec_sha256':sha(effective),'effective_config_is_generated_from_Root_base_plus_delta':True,'steps':steps,'CPU_checks':checks,'runtime':'NOT_RUN','Wine':'NOT_RUN','GPU':'NOT_RUN','protocol_version':1,'profile_id':spec['profile_id'],'interface_revision':spec['interface_revision'],'model_extent':[1280,720],'guide_extent':[1280,720],'color_output_extent':[1280,720],'Native_SR_output_extent':[1920,1080],'working_scale':1.0,'max_jobs':120,'parameter_application':spec['parameter_application'],'hot_pixel_readback_bytes':0,'production_modes':['Encode0','Evaluate_API','Resolve1'],'downsample_calls':0,'budgets_ms':{'cold':120000,'job':30000,'fence':10000,'control':60000,'wall':300000},'scope':'True original-render Pre-SR720 only; Native same-source DLSS SR1080 independently required','history':str(history)}
(out/'build.json').write_text(json.dumps(j,indent=2)+'\n');(history/'build.json').write_text(json.dumps(j,indent=2)+'\n');print(json.dumps({'status':j['status'],'build_sha256':sha(out/'build.json'),'artifact_sha256':j['artifact_sha256'],'CPU':checks}));sys.exit(rc)
