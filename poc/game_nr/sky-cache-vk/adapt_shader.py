#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Deterministic explicit bindings plus two policy guards; production math untouched."""
from pathlib import Path
import hashlib,re,difflib
SOURCE=Path('/home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/OptiScaler/shaders/dlssnr/precompile/dlssnr_cache.hlsl')
SHA='0efdd31f1976435b0b956b94814c96a70e68310b838f916aecc289375af02b4f'
def adapt(out):
    raw=SOURCE.read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA
    s=raw.decode();s=s.replace('cbuffer Params : register(b0)','[[vk::binding(0, 0)]]\ncbuffer Params : register(b0)',1)
    for kind,prefix,offset in [('Texture2D','t',1),('RWTexture2D','u',13)]:
        pattern=r'^('+kind+r'<[^>]+>\s+\w+\s*: register\('+prefix+r'(\d+)\);)'
        s=re.sub(pattern,lambda m:'[[vk::binding(%d, 0)]]\n%s'%(int(m[2])+offset,m[1]),s,flags=re.M)
    s=s.replace('SamplerState gLinear : register(s0);','[[vk::binding(20, 0)]]\nSamplerState gLinear : register(s0);',1)
    key='    float gJitterDeltaY;\n';assert s.count(key)==1
    s=s.replace(key,key+'    uint gWholeValid; // standalone whole lineage guard at byte156; not pixel confidence\n')
    key='    if (gMode == 11)\n';assert s.count(key)==1
    s=s.replace(key,'    if (gMode == 4 && gWholeValid == 0)\n    {\n        if (inside) gOut0[id.xy] = gColour.Load(int3(id.xy, 0));\n        return;\n    }\n\n'+key)
    key='        float3 result = max((max(original.rgb, 0.0) + eps) * exp2(edit) - eps, 0.0);';assert s.count(key)==1
    s=s.replace(key,'        // Explicit identity wrapper: preserve negatives / signed zero / original alpha.\n        if (all(edit == 0.0) && gTemporal == 0.0 && gLowTemporal == 0.0 && gDebugView == 0)\n        {\n            gOut0[id.xy] = original;\n            return;\n        }\n\n'+key)
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    (out/'sky_cache_vk.hlsl').write_text(s)
    (out/'shader-adaptation.patch').write_text(''.join(difflib.unified_diff(raw.decode().splitlines(keepends=True),s.splitlines(keepends=True),fromfile='frozen_sky.hlsl',tofile='sky_cache_vk.hlsl')))
    return out/'sky_cache_vk.hlsl'
