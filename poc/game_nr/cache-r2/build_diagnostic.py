#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# r2 exact identity shader + diagnostics. Host compile/check only, no PE/Wine/Vulkan invocation.
import hashlib,json,subprocess,sys,struct,difflib
from pathlib import Path
from datetime import datetime,timezone
from adapt_identity import adapt
ROOT=Path(__file__).resolve().parent;VK=ROOT.parent/'sky-cache-vk';bp=VK/'out/build.json'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert sha(bp)=='6b0c954cf5d7db348979c308eef7847a4abd840ae51e360bd86cad4881587f1f'
b=json.loads(bp.read_text());deps={}
for k in ['sources','readonly_source_pins','generated','build_tools']:deps.update(b[k])
deps[str(bp)]=sha(bp);deps[b['artifact']]=b['artifact_sha256']
for k,h in json.loads((ROOT/'DIRECTED-EVIDENCE.json').read_text())['pins'].items():deps[k]=h
names=['sky_cache_diagnostic.cpp','cache_owned_r2.h','adapt_identity.py','test_identity_cpu.py','diagnostic-only.patch','investigate_zero_cpu.py','DIRECTED-EVIDENCE.json','README.md','build_diagnostic.py','.gitignore'];sources={str(ROOT/n):sha(ROOT/n) for n in names}
def verify():
 for p,h in deps.items():assert sha(p)==h,p
verify();out=ROOT/'out';out.mkdir(exist_ok=True);history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True)
for n in names:(history/n).write_bytes((ROOT/n).read_bytes())
steps=[]
def run(name,cmd):
 try:r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);rc=r.returncode;data=r.stdout
 except subprocess.TimeoutExpired as e:rc=124;data=e.stdout or b''
 p=history/(name+'.log');p.write_bytes(data);steps.append({'name':name,'command':cmd,'exit_code':rc,'log':str(p)});print(json.dumps(steps[-1]),flush=True)
 if rc:print(data.decode(errors='replace')[:12000],flush=True)
 return rc
hlsl=adapt(out);rc=run('r2-HLSL-SPIRV',['/usr/bin/glslangValidator','-D','-V','--target-env','vulkan1.2','-S','comp','-e','CSMain',str(hlsl),'-o',str(out/'sky_cache.spv')]);exe=out/'wine_sky_cache_diagnostic.exe';cpu={};layout={}
if not rc:rc=run('r2-SPIRV-val',['/usr/bin/spirv-val','--target-env','vulkan1.2',str(out/'sky_cache.spv')])
if not rc:rc=run('r2-SPIRV-dis',['/usr/bin/spirv-dis',str(out/'sky_cache.spv'),'-o',str(out/'sky_cache.spvasm')])
if not rc:
 rc=run('CPU-identity-counterexamples',[sys.executable,str(ROOT/'test_identity_cpu.py'),str(hlsl),str(out/'sky_cache.spvasm')])
 if not rc:cpu=json.loads((history/'CPU-identity-counterexamples.log').read_text())
if not rc:
 sys.path.insert(0,str(VK));from cpu_checks import checks
 layout=checks([Path(p) for p in b['case_paths']],out/'sky_cache.spvasm');(history/'CPU-frozen-case-layout-checks.json').write_text(json.dumps(layout,indent=2)+'\n')
 words=struct.unpack('<'+'I'*((out/'sky_cache.spv').stat().st_size//4),(out/'sky_cache.spv').read_bytes());(out/'sky_cache_spv.h').write_text('#pragma once\n#include <cstdint>\nstatic const uint32_t sky_cache_spv[]={\n'+','.join(hex(w) for w in words)+'\n};\n')
base=json.loads((ROOT.parent/'high-fps-fixture/out/build.json').read_text());p=base['readonly_dependency_pins'];cc=p['compiler']['path']
if not rc:rc=run('PE-r2-exact-identity',[cc,'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+p['sdk_root'],'-I'+p['ngx_sdk_root'],'-I'+str(Path(p['json_header']['path']).parent),str(ROOT/'sky_cache_diagnostic.cpp'),'-o',str(exe)])
post={str(ROOT/n):sha(ROOT/n) for n in names};assert sources==post;verify()
generated={str(out/n):sha(out/n) for n in ['sky_cache_vk.hlsl','exact-identity-shader.patch','sky_cache.spv','sky_cache.spvasm','sky_cache_spv.h'] if (out/n).exists()}
prior=[]
for name in ['20261013T041219.452667Z','20261013T041605.626943Z']:
 p=out/'build-history'/name/'build.json';j=json.loads(p.read_text());prior.append({'build_path':str(p),'build_sha256':sha(p),'artifact_sha256':j['artifact_sha256'],'scope':'prior diagnostic-only, shader identity guard NOT_APPLIED; preserved exact metadata/source/logs'})
j={'status':'BUILD_ONLY_PASS' if rc==0 else 'BUILD_ONLY_FAIL','exit_code':rc,'sources':post,'sources_pre_compile':sources,'sources_pre_post_equal':sources==post,'readonly_source_pins':deps,'generated':generated,'build_tools':{v:sha(v) for v in [cc,'/usr/bin/glslangValidator','/usr/bin/spirv-val','/usr/bin/spirv-dis',str(Path(sys.executable).resolve())]},'artifact':str(exe),'artifact_sha256':sha(exe) if rc==0 else None,'steps':steps,'CPU_identity_checks':cpu,'CPU_frozen_case_layout_checks':layout,'shader_identity':{'production_original_unchanged':True,'frozen_generated_base_sha256':sha(VK/'out/sky_cache_vk.hlsl'),'old_SPIRV_preserved_sha256':sha(VK/'out/sky_cache.spv'),'r2_SPIRV_sha256':sha(out/'sky_cache.spv') if (out/'sky_cache.spv').exists() else None,'applied':'finite rawRGB bit equality => exactFreshEdit0/ok1 before max/ratio/log2; no approximate/clamped/residual identity','epsilon_masks_oracle_tolerance':'UNCHANGED'},'frozen_case_paths':b['case_paths'],'diagnostic_only_history':prior,'runtime':'NOT_RUN','GPU':'NOT_RUN','Wine':'NOT_RUN','original_run_numeric_status':'FAIL/UNPROVED_LEDGER_PRESERVED','new_scope':'Exact finite raw identity; save completed-fence reads before gates; exact mismatches; known-safe API retirement separate from numeric acceptance','history':str(history)}
(out/'build.json').write_text(json.dumps(j,indent=2)+'\n');(history/'build.json').write_text(json.dumps(j,indent=2)+'\n')
for n in ['sky_cache_vk.hlsl','exact-identity-shader.patch','sky_cache.spv','sky_cache.spvasm','sky_cache_spv.h']:
 if (out/n).exists():(history/n).write_bytes((out/n).read_bytes())
if not rc:(history/exe.name).write_bytes(exe.read_bytes())
print(json.dumps({'status':j['status'],'build_sha256':sha(out/'build.json'),'artifact_sha256':j['artifact_sha256'],'CPU_identity':cpu.get('checks_count'),'CPU_layout':layout.get('checks_count')}));sys.exit(rc)
