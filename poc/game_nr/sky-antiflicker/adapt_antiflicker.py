#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""r2 math plus exact upstream Context12/AntiPop block; no new cache algorithm."""
from pathlib import Path
import hashlib,difflib
ROOT=Path(__file__).resolve().parent
R2=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-highfps-worktree/poc/game_nr/cache-r2/out/sky_cache_vk.hlsl')
UP=ROOT/'provenance/skynizz-6bfc4389-cache.hlsl'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def mode_block(s,mode):
 key='    if (gMode == '+str(mode)+')\n    {';start=s.rindex(key);p=start+len(key);level=1
 while level:
  if s[p]=='{':level+=1
  elif s[p]=='}':level-=1
  p+=1
 return s[start:p]
def adapt(out):
 assert sha(UP)=='0b123748c02a36438fc033b605aa8e79e6751b7d9ce8cc778d6d6bfbfd95de8c'
 import json
 b=json.loads((R2.parent/'build.json').read_text());assert sha(R2)==b['generated'][str(R2)]
 original=R2.read_text();s=original;u=UP.read_text()
 key='    uint gWholeValid; // standalone whole lineage guard at byte156; not pixel confidence\n';assert s.count(key)==1
 s=s.replace(key,key+'    float gAntiPopStep; // byte160; exact upstream rate*real smoothed dt, zero off\n    float gAntiPopFrameTol; // byte164, original upstream .15\n    uint gCtxW; // byte168\n    uint gCtxH; // byte172\n')
 key='SamplerState gLinear : register(s0);';at=s.index('[[vk::binding(20, 0)]]')
 s=s[:at]+'[[vk::binding(21, 0)]] Texture2D<float4> gContext : register(t12);\n[[vk::binding(22, 0)]] Texture2D<float4> gContextPrev : register(t13);\n\n'+s[at:]
 # Retain upstream byte-for-byte functions and context mode. Existing 21 bindings stay put.
 start=u.index('float3 PrintOf(');end=u.index('// Full belief inside',start);functions=u[start:end]
 s=s.replace('// An edit bounded to what the composition can actually produce:',functions+'\n// An edit bounded to what the composition can actually produce:',1)
 context=mode_block(u,12);last=s.rindex('    if (gMode == 7)');s=s[:last]+context+'\n\n'+s[last:]
 # Existing r2 Apply output/edit selection must include AntiPop (unlike pulling entire newer shader).
 key='if ((gTemporal > 0.0 || gLowTemporal > 0.0) && gDebugView == 0)';assert s.count(key)==1
 s=s.replace(key,'if ((gTemporal > 0.0 || gLowTemporal > 0.0 || gAntiPopStep > 0.0) && gDebugView == 0)')
 key='if (all(edit == 0.0) && gTemporal == 0.0 && gLowTemporal == 0.0 && gDebugView == 0)';assert s.count(key)==1
 s=s.replace(key,'if (all(edit == 0.0) && gTemporal == 0.0 && gLowTemporal == 0.0 && gAntiPopStep == 0.0 && gDebugView == 0)')
 s=s.replace('if (gMode == 4 && gWholeValid == 0)','if ((gMode == 4 || gMode == 10) && gWholeValid == 0)',1)
 key='if (inside) gOut0[id.xy] = gColour.Load(int3(id.xy, 0));';assert s.count(key)==1
 s=s.replace(key,'if (inside) { gOut0[id.xy] = gColour.Load(int3(id.xy, 0)); if (gMode == 10) gOut3[id.xy] = 0.0; }',1)
 m=mode_block(s,10);changed=m.replace('if (gLowTemporal > 0.0)','if (gLowTemporal > 0.0 || gAntiPopStep > 0.0)').replace('if (gLowTemporal <= 0.0)','if (gLowTemporal <= 0.0 && gAntiPopStep <= 0.0)').replace('const float3 lowOut = lerp(lowNow, lowHist, lowW);','float3 lowOut = lerp(lowNow, lowHist, lowW);')
 ap_start=u.index('                        // Anti light pop-in.');ap_end=u.index('                        hist += lc - lh;',ap_start);anti=u[ap_start:ap_end]
 key='                        float3 lowOut = lerp(lowNow, lowHist, lowW);\n\n';assert changed.count(key)==1;changed=changed.replace(key,key+anti,1)
 key='        const float eps = Eps();';assert changed.count(key)==1
 changed=changed.replace(key,'        // Identity policy only; nonzero full-strength Sky math stays unchanged.\n        if (all(outEdit == 0.0)) { gOut0[id.xy] = original; gOut3[id.xy] = float4(outEdit, 1.0); return; }\n\n'+key)
 s=s.replace(m,changed,1)
 out=Path(out);out.mkdir(exist_ok=True,parents=True);p=out/'sky_antiflicker.hlsl';p.write_text(s);(out/'exact-adaptation.patch').write_text(''.join(difflib.unified_diff(original.splitlines(True),s.splitlines(True),fromfile='frozen-r2-e99-source',tofile='sky-antiflicker-v1')))
 return p,{'base_r2_source_sha256':sha(R2),'upstream_source_sha256':sha(UP),'exact_Context_functions':functions,'exact_Context_mode12':context,'exact_AntiPop_block':anti}
