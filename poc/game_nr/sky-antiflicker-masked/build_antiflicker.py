#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Only native CPU validators, shader compiler/validator and PE compiler; never launches PE/Wine/Vulkan.
from pathlib import Path
from datetime import datetime,timezone
import json,hashlib,subprocess,sys,struct
from adapt_antiflicker import adapt,R2
ROOT=Path(__file__).resolve().parent;sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
bp=R2.parent/'build.json';assert sha(bp)=='ff309ae699adf6958b440c659f39b34b8bc3e103f78e3ac9d0780ee52b54095d';r2=json.loads(bp.read_text());deps={str(bp):sha(bp)}
for k in ['sources','readonly_source_pins','generated','build_tools']:deps.update(r2[k])
deps[r2['artifact']]=r2['artifact_sha256'];old=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-highfps-worktree/poc/game_nr');ctx=old/'sky-cache-vk/cache_context.h';deps[str(ctx)]=sha(ctx)
base=json.loads((old/'high-fps-fixture/out/build.json').read_text());p=base['readonly_dependency_pins'];cc=p['compiler']['path'];names=['.gitignore','README.md','NOTICE.md','NATIVE-INTERFACE.md','adapt_antiflicker.py','sky_antiflicker_policy.h','sky_antiflicker_owned.h','compile_owned.cpp','temporal_reference.py','test_offline.py','audit_actual_energy.py','policy_check.cpp','build_antiflicker.py','provenance/skynizz-6bfc4389-cache.hlsl'];sources={str(ROOT/n):sha(ROOT/n) for n in names}
control=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/integration/high-fps-20261013/antiflicker-kernel-control-v1.json');assert sha(control)=='4667d5f888c2453af88946f3a47fb1cb92afad0218e6948a494d759a3ca211a4';deps[str(control)]=sha(control)
frozen=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-antiflicker-worktree/poc/game_nr/sky-antiflicker/out/build.json');assert sha(frozen)=='f807daf711b9b2a92d31a416a57cda8ad5911b197a51a41cf05eb290dd58a782';fb=json.loads(frozen.read_text());deps[str(frozen)]=sha(frozen)
for k in ['sources','generated']:deps.update(fb[k])
deps[fb['artifact']]=fb['artifact_sha256']
def verify():
 for q,h in deps.items():assert sha(q)==h,q
verify();out=ROOT/'out';history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True);steps=[]
def run(name,cmd):
 try:r=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);rc=r.returncode;data=r.stdout
 except subprocess.TimeoutExpired as e:rc=124;data=e.stdout or b''
 log=history/(name+'.log');log.write_bytes(data);steps.append({'name':name,'command':cmd,'exit_code':rc,'log':str(log)});print(json.dumps(steps[-1]),flush=True)
 if rc:print(data.decode(errors='replace')[:16000],flush=True)
 return rc
hlsl,parts=adapt(out);cpu={};policy={};actual={};exe=out/'sky_antiflicker_compile_gate.exe';rc=run('HLSL-SPIRV',['/usr/bin/glslangValidator','-D','-V','--target-env','vulkan1.2','-S','comp','-e','CSMain',str(hlsl),'-o',str(out/'sky_antiflicker.spv')])
if not rc:rc=run('SPIRV-val',['/usr/bin/spirv-val','--target-env','vulkan1.2',str(out/'sky_antiflicker.spv')])
if not rc:rc=run('SPIRV-dis',['/usr/bin/spirv-dis',str(out/'sky_antiflicker.spv'),'-o',str(out/'sky_antiflicker.spvasm')])
if not rc:
 rc=run('CPU-expressions-negative-layout',[sys.executable,str(ROOT/'test_offline.py')]);
 if not rc:cpu=json.loads((history/'CPU-expressions-negative-layout.log').read_text())
if not rc:
 rc=run('CPU-actual-P1-energy',[sys.executable,str(ROOT/'audit_actual_energy.py')]);
 if not rc:actual=json.loads((history/'CPU-actual-P1-energy.log').read_text())
if not rc:rc=run('CPU-policy-compile',['/usr/bin/c++','-std=c++17','-O2','-Wall','-Wextra','-Werror',str(ROOT/'policy_check.cpp'),'-o',str(out/'policy_check')])
if not rc:
 rc=run('CPU-policy-test',[str(out/'policy_check')]);
 if not rc:policy=json.loads((history/'CPU-policy-test.log').read_text())
if not rc:
 raw=(out/'sky_antiflicker.spv').read_bytes();words=struct.unpack('<'+'I'*(len(raw)//4),raw);(out/'sky_antiflicker_spv.h').write_text('#pragma once\n#include <cstdint>\nstatic const uint32_t sky_antiflicker_spv[]={'+','.join(hex(w) for w in words)+'};\n')
 rc=run('PE-owned-record-compile',[cc,'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+p['sdk_root'],'-I'+p['ngx_sdk_root'],'-I'+str(Path(p['json_header']['path']).parent),str(ROOT/'compile_owned.cpp'),'-o',str(exe)])
post={str(ROOT/n):sha(ROOT/n) for n in names};assert post==sources;verify()
generated={str(out/n):sha(out/n) for n in ['sky_antiflicker.hlsl','exact-adaptation.patch','sky_antiflicker.spv','sky_antiflicker.spvasm','sky_antiflicker_spv.h','policy_check'] if (out/n).is_file()};tools={str(Path(t).resolve()):sha(Path(t).resolve()) for t in [cc,'/usr/bin/c++','/usr/bin/glslangValidator','/usr/bin/spirv-val','/usr/bin/spirv-dis',sys.executable]}
j={'status':'BUILD_ONLY_PASS' if not rc else 'BUILD_ONLY_FAIL','exit_code':rc,'sources':post,'sources_pre_compile':sources,'sources_pre_post_equal':sources==post,'readonly_source_pins':deps,'generated':generated,'build_tools':tools,'artifact':str(exe),'artifact_sha256':sha(exe) if not rc else None,'steps':steps,'CPU_expression_checks':cpu,'CPU_policy_checks':policy,'actual_P1_energy':actual,'upstream':{'url':'https://github.com/Skynizz/optiscaler-dlss5','commit':'6bfc4389c25054ef6747df6e6b1b470e9541c4da','shader_sha256':parts['upstream_source_sha256']},'base_r2_shader_sha256':parts['base_r2_source_sha256'],'pipeline':{'bindings':24,'readonly_prev_mask_binding':23,'mask_bytes':4,'mask_count':2,'mask_bits':{'shown':1,'context':2},'CB_bytes':256,'whole_valid_offset':156,'AntiPopStep_offset':160,'FrameTolerance_offset':164,'ContextW_offset':168,'ContextH_offset':172,'workgroup':[8,8,1],'fine_temporal':0,'crossfade':0,'low_temporal':.95,'NR_strength':1,'age_max':4},'canonical_control':{'path':str(control),'sha256':sha(control)},'runtime':'NOT_RUN','Wine':'NOT_RUN','GPU':'NOT_RUN','history':str(history),'scope':'masked frozen1ef candidates/owned record compile gate only; off uses original e99; NOT runtime/game/phase/flicker/quality/performance acceptance'}
(out/'build.json').write_text(json.dumps(j,indent=2)+'\n');(history/'build.json').write_text(json.dumps(j,indent=2)+'\n')
for n in names:
 dst=history/n;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes((ROOT/n).read_bytes())
for q in generated:(history/Path(q).name).write_bytes(Path(q).read_bytes())
if not rc:(history/exe.name).write_bytes(exe.read_bytes())
print(json.dumps({'status':j['status'],'build':sha(out/'build.json'),'artifact':j['artifact_sha256'],'CPU_expr':cpu.get('count'),'CPU_policy':policy.get('checks')}));sys.exit(rc)
