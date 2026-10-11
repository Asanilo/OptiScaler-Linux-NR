#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Isolated C2 compiler only. Never invokes Wine/Vulkan/game/GPU.
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys
ROOT=Path(__file__).resolve().parent
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
pins=json.loads((ROOT/'c2-runtime-pins.json').read_text())
review=json.loads((ROOT.parent/'integration/c2-contract-review.json').read_text())
if review['status']!='APPROVED_SOURCE_AND_BUILD_ONLY' or review['sha256']!=pins['contract_sha256'] or h(ROOT/'C2-CONTRACT.md')!=pins['contract_sha256']:
 raise SystemExit('C2 exact reviewed contract required')
if h(pins['inherited_actual_G3'])!=pins['inherited_G3_results_sha256']:raise SystemExit('inherited actual G3 evidence changed')
for obj in pins['runtime_dependencies']:
 p=Path(obj['path'])
 if p.stat().st_size!=obj['bytes'] or h(p)!=obj['sha256']:raise SystemExit('read-only inherited runtime identity changed: '+str(p))
identities={**pins['frozen_artifacts'],**pins['SDK_headers'],**pins['ngx_sdk_headers'],**pins['frozen_C1_sources'],**pins['derived_from']}
identities[pins['frozen_C1_artifact']['path']]=pins['frozen_C1_artifact']['sha256']
for p,d in identities.items():
 if h(p)!=d:raise SystemExit('frozen/include identity changed: '+p)
cc=Path(pins['compiler']['path']);jh=Path(pins['json_header']['path'])
if h(cc)!=pins['compiler']['sha256'] or h(jh)!=pins['json_header']['sha256'] or jh.stat().st_size!=pins['json_header']['bytes']:raise SystemExit('compiler/json header identity changed')
for k in ['wine','preloader']:
 if h(pins['loader'][k])!=pins['loader'][k+'_sha']:raise SystemExit('frozen loader identity changed')
source_names=['wine_game_nr_helper.cpp','c2_production_adapter.h','c2_game_transport.h','c2_game_control.h','fixture_util.h','build_c2.py','c2-runtime-pins.json','C2-CONTRACT.md']
out=ROOT/'out-c2';out.mkdir(exist_ok=True);history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True)
exe=out/'wine_game_nr_helper.exe'
cmd=[str(cc),'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+pins['sdk_root'],'-I'+pins['ngx_sdk_root'],'-I'+str(jh.parent),str(ROOT/'wine_game_nr_helper.cpp'),'-o',str(exe)]
rc=125;output=b'';started=datetime.now(timezone.utc)
try:
 p=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);rc=p.returncode;output=p.stdout
except subprocess.TimeoutExpired as e:output=(e.stdout or b'')+b'\nBUILD_WATCHDOG120s (compiler only, no GPU)\n'
log=history/'helper-build.log';log.write_bytes(output)
sources={str(ROOT/n):h(ROOT/n) for n in source_names}
for n in source_names:(history/n).write_bytes((ROOT/n).read_bytes())
readonly_source_pins={**identities,str(jh):h(jh)}
readonly_source_pins.update({obj['path']:obj['sha256'] for obj in pins['runtime_dependencies'] if obj.get('runtime_binary_reuse') is None})
for p in readonly_source_pins:
 path=Path(p)
 if path.suffix in ['.h','.hpp','.cpp','.py','.md','.json','.spv']:
  target=history/'readonly-dependencies'/path.relative_to('/');target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(path.read_bytes())
(history/'c2-contract-review.json').write_bytes((ROOT.parent/'integration/c2-contract-review.json').read_bytes())
d={'status':'PASS' if rc==0 else 'FAIL','scope':'C2_SINGLE_HELPER_BUILD_ONLY_NO_GPU','time':datetime.now(timezone.utc).isoformat(),'started':started.isoformat(),'exit_code':rc,'command':cmd,'workdir':str(ROOT),'sources':sources,'readonly_source_pins':readonly_source_pins,'readonly_dependency_pins':pins,'artifact':str(exe),'artifact_sha256':h(exe) if rc==0 else None,'artifact_bytes':exe.stat().st_size if rc==0 else None,'log':str(log),'contract_sha256':pins['contract_sha256'],'runtime':'NOT_RUN','C2_single':'NOT_RUN','C2_skipEvaluate':'NOT_RUN','old_C1_G1_G2_G3_changed':False,'production_shader_entry':'CSMain','extent':[1920,1080],'dispatch_groups':[240,135,1],'readback_segments':['raw16F','shared_sourceColor32F','output32F'],'readback_bytes':82944000}
(out/'build.json').write_text(json.dumps(d,indent=2)+'\n');(history/'build.json').write_text(json.dumps(d,indent=2)+'\n')
print(output.decode(errors='replace'));print(json.dumps({k:v for k,v in d.items() if k not in ['sources','readonly_source_pins','readonly_dependency_pins']},indent=2));sys.exit(rc)
