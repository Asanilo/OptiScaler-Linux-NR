#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Compiler/static SPIR-V validation only; never invokes Wine/game/Vulkan runtime.
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys,struct
ROOT=Path(__file__).resolve().parent
BASE=ROOT.parent/'c2-baseline'
CANON=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/integration/C4-CONTRACT.md')
REVISION=CANON.with_name('C4-POLL-REVISION.md')
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
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
if not CANON.is_file():raise SystemExit('Integration-owned C4 canonical missing')
contract_hash=h(CANON)
if contract_hash!='18af8d771d22577e5b7806ed36213762a61bdb3db1b38d49324955e57fb8e0f6':raise SystemExit('C4 canonical differs from reviewed 18af8d77')
revision_hash=h(REVISION)
if revision_hash!='5b71aeb2d8007c8c284a202d3e026d28719e852c5f4789592d6c565d6f7d311f':raise SystemExit('C4 polling revision differs from Root-reviewed text')
C3=ROOT.parent/'c3';c3build=C3/'out/build.json'
if h(c3build)!='ca08496eda92f0da0bdca1484a3d7e0166843c1017a3e465380d84e310bffadd':raise SystemExit('Frozen C3 build manifest changed')
c3=json.loads(c3build.read_text())
for p,d in {**c3['sources'],**c3['generated'],c3['artifact']:c3['artifact_sha256']}.items():
 if h(p)!=d:raise SystemExit('Frozen C3 source/generated/artifact changed: '+p)
source_names=['wine_game_nr_hot.cpp','c4_production_adapter.h','gpu_validation.h','validate_frame.comp','build_c4.py']
source_pre={str(ROOT/n):h(ROOT/n) for n in source_names}
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
shader=out/'validate_frame.spv';exe=out/'wine_game_nr_hot.exe'
rc=run('glsl-compile',['/usr/bin/glslangValidator','-V','--target-env','vulkan1.2',str(ROOT/'validate_frame.comp'),'-o',str(shader)])
if rc==0:rc=run('spirv-validation',['/usr/bin/spirv-val','--target-env','vulkan1.2',str(shader)])
if rc==0:
 data=shader.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data)
 (out/'validate_spv.h').write_text('#pragma once\n#include <cstdint>\nstatic const uint32_t c3_validate_spv[]={'+','.join(hex(v) for v in words)+'};\n')
 rc=run('helper-compile',[str(cc),'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+pins['sdk_root'],'-I'+pins['ngx_sdk_root'],'-I'+str(jh.parent),str(ROOT/'wine_game_nr_hot.cpp'),'-o',str(exe)])
if h(CANON)!=contract_hash:raise SystemExit('Canonical changed during compile; preserve logs, rebuild against current canonical before freezing')
if h(REVISION)!=revision_hash:raise SystemExit('C4 polling revision changed during compile; preserve logs, not frozen')
source_names=['wine_game_nr_hot.cpp','c4_production_adapter.h','gpu_validation.h','validate_frame.comp','build_c4.py']
sources={str(ROOT/n):h(ROOT/n) for n in source_names}
if sources!=source_pre:raise SystemExit('C4 source changed during compile; preserve history, not frozen')
readonly={**identities,str(jh):h(jh),str(BASE/'c2_game_transport.h'):h(BASE/'c2_game_transport.h'),str(BASE/'c2_game_control.h'):h(BASE/'c2_game_control.h'),str(BASE/'fixture_util.h'):h(BASE/'fixture_util.h'),str(BASE/'c2-runtime-pins.json'):h(BASE/'c2-runtime-pins.json'),str(CANON):contract_hash,str(REVISION):revision_hash,str(c3build):h(c3build),**c3['sources'],**c3['generated']}
for n in source_names:(history/n).write_bytes((ROOT/n).read_bytes())
for p in readonly:
 path=Path(p)
 if path.suffix in ['.h','.hpp','.cpp','.py','.md','.json','.spv']:
  target=history/'readonly-dependencies'/path.relative_to('/');target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(path.read_bytes())
generated={str(p):h(p) for p in [shader,out/'validate_spv.h'] if p.exists()}
tools={str(p):h(p) for p in ['/usr/bin/glslangValidator','/usr/bin/spirv-val',cc]}
git=lambda *args:subprocess.check_output(['git',*args],cwd=ROOT,text=True).strip()
d={'status':'PASS' if rc==0 else 'FAIL','scope':'C4_HOT_DIAGNOSTIC_HELPER_BUILD_ONLY_NO_GPU','time':datetime.now(timezone.utc).isoformat(),'exit_code':rc,'steps':steps,'workdir':str(ROOT),'sources':sources,'sources_pre_compile':source_pre,'sources_pre_post_equal':sources==source_pre,'generated':generated,'readonly_source_pins':readonly,'readonly_dependency_pins':pins,'build_tools':tools,'artifact':str(exe),'artifact_sha256':h(exe) if rc==0 else None,'artifact_bytes':exe.stat().st_size if rc==0 else None,'contract':str(CANON),'contract_sha256':contract_hash,'additive_revision':str(REVISION),'additive_revision_sha256':revision_hash,'active_frame_poll_interval_ms':1,'frame_elapsed_clock':'QueryPerformanceCounter','git_branch':git('branch','--show-current'),'git_head':git('rev-parse','HEAD'),'git_status':git('status','--short'),'runtime':'NOT_RUN','C4_diagnostic':'NOT_RUN','C4_hot':'NOT_RUN','C4_matched_performance':'NOT_RUN','C4_sustained_play':'NOT_RUN','C3_commits':{'A':'1df65cd61395aba56aae07c0a0b1c7ed84accf5a','B':'f9132dff6027d080bd15aafaccad3bcabb026a1b'},'frozen_C3_artifact':{'path':c3['artifact'],'sha256':c3['artifact_sha256'],'bytes':c3['artifact_bytes']},'extent':[1920,1080],'dispatch_groups':[240,135,1],'production_shader_entry':'CSMain','hot':{'pixel_readback_bytes':0,'validator_allocation':False,'signature':'NOT_RUN','timestamp_query_count':6,'query_readback_bytes':96},'diagnostic':{'gpu_stats_bytes':64,'pixel_readback_bytes':82944000,'timestamp_query_count':8,'query_readback_bytes':128,'signature_algorithm':'rgba32f_mix32_xor_sum_v1'},'GPU_execution':False}
(out/'build.json').write_text(json.dumps(d,indent=2)+'\n');(history/'build.json').write_text(json.dumps(d,indent=2)+'\n')
print(json.dumps({k:v for k,v in d.items() if k not in ['sources','generated','readonly_source_pins','readonly_dependency_pins']},indent=2));sys.exit(rc)
