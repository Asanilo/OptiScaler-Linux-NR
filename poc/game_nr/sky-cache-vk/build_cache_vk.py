#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""HLSL -> SPIRV/validation/reflection + host oracle checks + PE BUILD_ONLY; no runtime."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys,struct
from adapt_shader import adapt,SOURCE,SHA
from make_cases import generate,RESULT
from cpu_checks import checks
ROOT=Path(__file__).resolve().parent
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
BASE=ROOT.parent/'high-fps-fixture'/'out'/'build.json';assert sha(BASE)=='ab828b20c1c37999fda8b8b6fab849f735281e0101f7ffe1fd2eab455cdc4eb4'
base=json.loads(BASE.read_text());deps={}
for k in ['sources','readonly_source_pins','generated','build_tools','current_runtime_pins']:deps.update(base[k])
deps[str(BASE)]=sha(BASE);deps[base['artifact']]=base['artifact_sha256']
for p in [SOURCE,Path('/home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/OptiScaler/shaders/dlssnr/DlssNr_CacheCommon.h'),RESULT,*list((ROOT.parent/'sky-cache-cpu').glob('*.py'))]:deps[str(p)]=sha(p)
names=['.gitignore','adapt_shader.py','cache_context.h','cache_owned.h','sky_cache_fixture.cpp','fixture_schema.py','make_cases.py','cpu_checks.py','build_cache_vk.py','README.md','source-map.json','CASE-SCHEMA.md']
sources={str(ROOT/n):sha(ROOT/n) for n in names};out=ROOT/'out';out.mkdir(exist_ok=True);history=out/'build-history'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');history.mkdir(parents=True)
for n in names:(history/n).write_bytes((ROOT/n).read_bytes())
steps=[];rc=0;cpu={};exe=out/'wine_sky_cache_fixture.exe';cases=[]
def verify():
 for p,h in deps.items():assert Path(p).is_file() and sha(p)==h,p
verify()
def run(name,command):
 global rc
 try:r=subprocess.run(command,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);code=r.returncode;data=r.stdout
 except subprocess.TimeoutExpired as e:code=124;data=(e.stdout or b'')+b'\nOFFLINE_BUILD_TIMEOUT120\n'
 p=history/(name+'.log');p.write_bytes(data);steps.append({'name':name,'command':command,'exit_code':code,'log':str(p)});print(json.dumps(steps[-1]),flush=True)
 if code:print(data.decode(errors='replace')[:14000]);rc=code
 return code
hlsl=adapt(out);cases=generate(out)
if not run('HLSL-to-SPIRV',['/usr/bin/glslangValidator','-D','-V','--target-env','vulkan1.2','-S','comp','-e','CSMain',str(hlsl),'-o',str(out/'sky_cache.spv')]):
 if not run('spirv-val',['/usr/bin/spirv-val','--target-env','vulkan1.2',str(out/'sky_cache.spv')]):
  if not run('spirv-dis',['/usr/bin/spirv-dis',str(out/'sky_cache.spv'),'-o',str(out/'sky_cache.spvasm')]):
   cpu=checks(cases,out/'sky_cache.spvasm');(history/'CPU-checks.json').write_text(json.dumps(cpu,indent=2)+'\n')
   words=struct.unpack('<'+'I'*(len((out/'sky_cache.spv').read_bytes())//4),(out/'sky_cache.spv').read_bytes());(out/'sky_cache_spv.h').write_text('#pragma once\n#include <cstdint>\nstatic const uint32_t sky_cache_spv[]={\n'+','.join(hex(w) for w in words)+'\n};\n')
   compiler=base['readonly_dependency_pins']['compiler']['path'];pins=base['readonly_dependency_pins']
   run('PE-cache-compile',[compiler,'-std=c++17','-static','-O1','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-function','-I'+pins['sdk_root'],'-I'+pins['ngx_sdk_root'],'-I'+str(Path(pins['json_header']['path']).parent),str(ROOT/'sky_cache_fixture.cpp'),'-o',str(exe)])
post={str(ROOT/n):sha(ROOT/n) for n in names};assert sources==post;verify()
generated={str(p):sha(p) for p in out.rglob('*') if p.is_file() and 'build-history' not in p.parts and p!=out/'build.json' and p!=exe}
for p in cases:
 c=json.loads(p.read_text())
 for f in c['frames']:
  for a in [*f['assets'].values(),*([] if f['expected'] is None else [f['expected']])]:deps[a['path']]=a['sha256']
verify();tools={p:sha(p) for p in ['/usr/bin/glslangValidator','/usr/bin/spirv-val','/usr/bin/spirv-dis',base['readonly_dependency_pins']['compiler']['path'],str(Path(sys.executable).resolve())]}
git=lambda *a:subprocess.check_output(['git',*a],cwd=ROOT,text=True).strip()
d={'status':'BUILD_ONLY_PASS' if rc==0 else 'BUILD_ONLY_FAIL','exit_code':rc,'scope':'host CPU known-domain oracle/schema + SPIRV compile/validate/reflection + PE crosscompile; NEVER Wine/PE/Vulkan API execution','sources':post,'sources_pre_compile':sources,'sources_pre_post_equal':sources==post,'readonly_source_pins':deps,'generated':generated,'build_tools':tools,'artifact':str(exe),'artifact_sha256':sha(exe) if rc==0 else None,'artifact_bytes':exe.stat().st_size if rc==0 else None,'steps':steps,'CPU_checks':cpu,'case_paths':[str(p) for p in cases],'shader_original_sha256':SHA,'shader_adaptation_patch':str(out/'shader-adaptation.patch'),'runtime':{'model_create':0,'model_Evaluate':0,'model_resolve':0,'external_FD':'NOT_USED','Vulkan':'NOT_RUN','Wine':'NOT_RUN','GPU':'NOT_RUN','real_phase':'UNVERIFIED','quality':'NOT_EVALUATED','performance':'NOT_MEASURED'},'managed_slot_count':1,'max_edit_links':4,'history_policy':'modelReset1 on sparse refresh separate from each per-game-frame edit link; this fixture consumes saved edited outputs only','budgets_ms':{'cold':120000,'frame':30000,'fence':10000,'wall':300000},'git_head':git('rev-parse','HEAD'),'git_branch':git('branch','--show-current'),'history':str(history)}
(out/'build.json').write_text(json.dumps(d,indent=2)+'\n');(history/'build.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps({'status':d['status'],'artifact':str(exe),'sha256':d['artifact_sha256'],'build_sha256':sha(out/'build.json'),'CPU_checks':cpu.get('checks_count'),'GPU':'NOT_RUN'}));sys.exit(rc)
