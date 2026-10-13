#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Root-authorized one semantic guard; frozen generated source and all other Sky math unchanged."""
from pathlib import Path
import hashlib,json,difflib
ROOT=Path(__file__).resolve().parent;VK=ROOT.parent/'sky-cache-vk'
GUARD='''    // Exact semantic identity on FINITE RAW RGB, before max/clamp or ratio/log2.
    // Signed-zero follows raw IEEE754 bits; unequal inputs keep the original full math.
    const bool rawSame = all(asuint(nr) == asuint(orig));
    if (rawSame && all(isfinite(nr)) && all(isfinite(orig)))
    {
        ok = 1.0;
        return float3(0.0, 0.0, 0.0);
    }

'''
def adapt(out):
    bp=VK/'out/build.json';assert hashlib.sha256(bp.read_bytes()).hexdigest()=='6b0c954cf5d7db348979c308eef7847a4abd840ae51e360bd86cad4881587f1f'
    b=json.loads(bp.read_text());p=VK/'out/sky_cache_vk.hlsl';raw=p.read_bytes();assert hashlib.sha256(raw).hexdigest()==b['generated'][str(p)]
    key='float3 FreshEdit(float3 nr, float3 orig, out float ok)\n{\n';s=raw.decode();assert s.count(key)==1
    changed=s.replace(key,key+GUARD,1);assert changed.replace(GUARD,'',1)==s
    out=Path(out);out.mkdir(parents=True,exist_ok=True);(out/'sky_cache_vk.hlsl').write_text(changed)
    (out/'exact-identity-shader.patch').write_text(''.join(difflib.unified_diff(s.splitlines(keepends=True),changed.splitlines(keepends=True),fromfile='frozen_generated_sky.hlsl',tofile='r2_identity_sky.hlsl')))
    return out/'sky_cache_vk.hlsl'
