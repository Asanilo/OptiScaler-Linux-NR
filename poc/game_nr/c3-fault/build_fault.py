#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Compiler/static SPIR-V validation only; never invokes Wine/game/Vulkan runtime.
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys,struct
ROOT=Path(__file__).resolve().parent
BASE=ROOT.parent/'c2-baseline'
PRIMARY=ROOT.parent/'c3'
PRIMARY_BUILD_SHA='ca08496eda92f0da0bdca1484a3d7e0166843c1017a3e465380d84e310bffadd'
CANON=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/integration/C3-CONTRACT.md')
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
if h(PRIMARY/'out/build.json')!=PRIMARY_BUILD_SHA:raise SystemExit('Frozen primary build provenance changed')
primary_build=json.loads((PRIMARY/'out/build.json').read_text())
primary_pins={str(PRIMARY/'out/build.json'):PRIMARY_BUILD_SHA}
for group in ['sources','generated','readonly_source_pins','build_tools']:primary_pins.update(primary_build[group])
primary_pins[primary_build['artifact']]=primary_build['artifact_sha256']
for path,sha in primary_pins.items():
 if h(path)!=sha:raise SystemExit('Frozen primary pin changed: '+path)
fault_source_names=['wine_game_nr_resident_fault.cpp','c3_production_adapter.h','gpu_validation.h','validate_frame.comp','build_fault.py','fault-source.patch']
fault_pre={str(ROOT/n):h(ROOT/n) for n in fault_source_names}
pins=json.loads((BASE/'c2-runtime-pins.json').read_text())
for obj in pins['runtime_dependencies']:
 p=Path(obj['path'])
 if p.stat().st_size!=obj['bytes'] or h(p)!=obj['sha256']:raise SystemExit('Frozen runtime changed: '+str(p))
identities={**pins['frozen_artifacts'],**pins['SDK_headers'],**pins['ngx_sdk_headers'],**pins['frozen_C1_sources'],**pins['derived_from']}
identities[pins['frozen_C1_artifact']['path']]=pins['frozen_C1_artifact']['sha256']
if h(pins['inherited_actual_G3'])!=pins['inherited_G3_results_sha256']:raise SystemExit('G3 evidence pin changed')
for p,d in identities.items():
 if h(p)!=d:raise SystemExit('Frozen include/dependency changed: '+p)
cc=Path(pins['compiler']['path']);jh=Path(pins['json_header']['path'])
if h(cc)!=pins['compiler']['sha256'] or h(jh)!=pins['json_header']['sha256'] or jh.stat().st_size!=pins['json_header']['bytes']:raise SystemExit('compiler/json identity changed')
for k in ['wine','preloader']:
 if h(pins['loader'][k])!=pins['loader'][k+'_sha']:raise SystemExit('loader identity changed')
if not CANON.is_file():raise SystemExit('Integration-owned C3 canonical missing')
contract_hash=h(CANON)
out=ROOT/'out';out.mkdir(exist_ok=True)
history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True)
steps=[]
def run(name,cmd):
 log=history/(name+'.log');rc=125;output=b''
 try:
  r=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);rc=r.returncode;output=r.stdout
 except subprocess.TimeoutExpired as e:output=(e.stdout or b'')+b'\nBUILD_WATCHDOG120s; no GPU\n'
 log.write_bytes(output);steps.append({'name':name,'command':cmd,'exit_code':rc,'log':str(log)})
 print(output.decode(errors='replace'));return rc
shader=out/'validate_frame.spv';exe=out/'wine_game_nr_resident_fault.exe'
rc=run('glsl-compile',['/usr/bin/glslangValidator','-V','--target-env','vulkan1.2',str(ROOT/'validate_frame.comp'),'-o',str(shader)])
if rc==0:rc=run('spirv-validation',['/usr/bin/spirv-val','--target-env','vulkan1.2',str(shader)])
if rc==0:
 data=shader.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data)
 (out/'validate_spv.h').write_text('#pragma once\n#include <cstdint>\nstatic const uint32_t c3_validate_spv[]={'+','.join(hex(v) for v in words)+'};\n')
 rc=run('helper-compile',[str(cc),'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+pins['sdk_root'],'-I'+pins['ngx_sdk_root'],'-I'+str(jh.parent),str(ROOT/'wine_game_nr_resident_fault.cpp'),'-o',str(exe)])
for path,sha in {**primary_pins,**fault_pre}.items():
 if h(path)!=sha:raise SystemExit('Source/primary changed during fault-only build: '+path)
if h(CANON)!=contract_hash:raise SystemExit('Canonical changed during compile; preserve logs, rebuild against current canonical before freezing')
source_names=fault_source_names
sources={str(ROOT/n):h(ROOT/n) for n in source_names}
readonly={**primary_pins,**identities,str(jh):h(jh),str(BASE/'c2_game_transport.h'):h(BASE/'c2_game_transport.h'),str(BASE/'c2_game_control.h'):h(BASE/'c2_game_control.h'),str(BASE/'fixture_util.h'):h(BASE/'fixture_util.h'),str(BASE/'c2-runtime-pins.json'):h(BASE/'c2-runtime-pins.json'),str(CANON):contract_hash}
for n in source_names:(history/n).write_bytes((ROOT/n).read_bytes())
for p in readonly:
 path=Path(p)
 if path.suffix in ['.h','.hpp','.cpp','.py','.md','.json','.spv']:
  target=history/'readonly-dependencies'/path.relative_to('/');target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(path.read_bytes())
generated={str(p):h(p) for p in [shader,out/'validate_spv.h'] if p.exists()}
tools={str(p):h(p) for p in ['/usr/bin/glslangValidator','/usr/bin/spirv-val',cc]}
git=lambda *args:subprocess.check_output(['git',*args],cwd=ROOT,text=True).strip()
d={'status':'PASS' if rc==0 else 'FAIL','scope':'C3_FAULT_ONLY_REAL_LATE_COMPLETION_BUILD_ONLY_NO_GPU','fault_flag':'C3_FAULT_DELAY_COMPLETE=after-fence-f0-35000ms','fault_delay_ms':35000,'fault_delay_hard_max_ms':40000,'primary_build_sha256':PRIMARY_BUILD_SHA,'expected_fault_exit_code':3,'normal_release_expected':False,'time':datetime.now(timezone.utc).isoformat(),'exit_code':rc,'steps':steps,'workdir':str(ROOT),'sources':sources,'generated':generated,'readonly_source_pins':readonly,'readonly_dependency_pins':pins,'build_tools':tools,'artifact':str(exe),'artifact_sha256':h(exe) if rc==0 else None,'artifact_bytes':exe.stat().st_size if rc==0 else None,'contract':str(CANON),'contract_sha256':contract_hash,'git_branch':git('branch','--show-current'),'git_head':git('rev-parse','HEAD'),'git_status':git('status','--short'),'runtime':'NOT_RUN','C3_smoke':'NOT_RUN','C3_static120':'NOT_RUN','C3_motion120':'NOT_RUN','C3_negative':'NOT_RUN','extent':[1920,1080],'dispatch_groups':[240,135,1],'production_shader_entry':'CSMain','stats_bytes':64,'full_readback_bytes':82944000,'signature_algorithm':'rgba32f_mix32_xor_sum_v1','GPU_execution':False}
(out/'build.json').write_text(json.dumps(d,indent=2)+'\n');(history/'build.json').write_text(json.dumps(d,indent=2)+'\n')
print(json.dumps({k:v for k,v in d.items() if k not in ['sources','generated','readonly_source_pins','readonly_dependency_pins']},indent=2));sys.exit(rc)
