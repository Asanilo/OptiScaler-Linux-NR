#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Host-only CPU reproduction and hash inventory, never Wine/Vulkan/model loading."""
import datetime,hashlib,json,platform,subprocess,sys
from pathlib import Path
BASE=Path(__file__).resolve().parent

def pin(p):
    p=Path(p).resolve();b=p.read_bytes()
    return {'path':str(p),'sha256':hashlib.sha256(b).hexdigest(),'bytes':len(b)}
def main():
    name=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    out=BASE/'out'/'build-history'/name;out.mkdir(parents=True,exist_ok=False)
    sources=[BASE/n for n in ['build_cache_cpu.py','sky_cache_reference.py','sky_cache_test.py','CACHE-README.md','source-map.json','.gitignore']]
    before=[pin(p) for p in sources]
    # Immutable original fixture metadata and original Sky shader are dependency inputs only.
    deps=[BASE.parent/'high-fps-fixture'/'input-manifest.json',Path('/home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/OptiScaler/shaders/dlssnr/precompile/dlssnr_cache.hlsl')]
    dependency_pins=[pin(p) for p in deps]
    proc=subprocess.run([sys.executable,str(BASE/'sky_cache_test.py')],cwd=BASE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=60)
    (out/'stdout.json').write_bytes(proc.stdout);(out/'stderr.log').write_bytes(proc.stderr)
    after=[pin(p) for p in sources];data=None
    if proc.returncode==0:data=json.loads(proc.stdout)
    result={'schema':'sky-cache-cpu-v1','status':'PASS' if proc.returncode==0 and before==after and data['status']=='PASS_CPU_ONLY' else 'FAIL','exit_code':proc.returncode,'sources':before,'source_pre_post_equal':before==after,'readonly_dependency_pins':dependency_pins,'python':pin(sys.executable),'python_version':platform.python_version(),'numpy_version':__import__('numpy').__version__,'checks_count':None if data is None else data['checks_count'],'raw_result':pin(out/'stdout.json'),'stderr':pin(out/'stderr.log'),'GPU':'NOT_RUN','Wine':'NOT_RUN','model':'NOT_RUN','actual_game_spatial_reprojection':'NOT_RUN','NR_phase':'UNVERIFIED','quality':'NOT_EVALUATED'}
    (out/'build-result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'status':result['status'],'result':str(out/'build-result.json'),'sha256':pin(out/'build-result.json')['sha256'],'checks':result['checks_count']}))
    return 0 if result['status']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
