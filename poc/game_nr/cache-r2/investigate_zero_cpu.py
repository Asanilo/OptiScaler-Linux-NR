#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One directed offline pass: failed-run F0 edit plus exact F1 canonical; no F1 GPU invention."""
from pathlib import Path
import hashlib,json,sys
import numpy as np
ROOT=Path(__file__).resolve().parent
VK=ROOT.parent/'sky-cache-vk'
FAIL=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/runs/sky-cache-vk-cache-zero-edit-a-20261013T040119.628921Z/output')
sys.path.insert(0,str(ROOT.parent/'sky-cache-cpu'));import sky_cache_reference as sky
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def investigate():
 c=json.loads((VK/'out/cases/zero_edit.json').read_text());f0,f1=c['frames'][:2]
 def read(f,k):
  a=f['assets'][k];p=Path(a['path']);assert sha(p)==a['sha256'];w,h=a['extent'];return np.fromfile(p,dtype='<f4').reshape((h,w,4) if k in ['source','edited'] else (h,w,2) if k=='mv' else (h,w))
 gpu=np.fromfile(FAIL/'f0-edit-history.rgba32f',dtype='<f4').reshape(8,16,4)
 before=read(f0,'source');out0=np.fromfile(FAIL/'f0-cache-output.rgba32f',dtype='<f4').reshape(8,16,4)
 assert before.tobytes()==read(f0,'edited').tobytes()==out0.tobytes()
 h,_=sky.capture(before,read(f0,'edited'),read(f0,'depth'),100);assert np.count_nonzero(h.edit[...,:3])==0
 # ONLY edit comes from GPU. Guide and pyramid here are reconstructed by frozen CPU expressions.
 h.edit=gpu.astype(np.float64);first,g=sky.reduce_first(h.edit[...,:3],np.stack([np.log2(h.guide[...,0]),h.guide[...,1]],axis=-1),np.ones((8,16)));h.levels=sky.pyramid(first);h.level_guide=g
 h1,_=sky.reproject(h,read(f1,'source'),read(f1,'depth'),read(f1,'mv'),101);pred,mask=sky.apply(read(f1,'source'),h1.edit,h1.guide,h1.levels,h1.level_guide)
 expected=np.fromfile(f1['expected']['path'],dtype='<f4').reshape(8,16,4);error=np.abs(pred-expected)
 rgb=gpu[...,:3];coords=np.argwhere(rgb!=0)
 return {'schema':'sky-cache-r2-directed-zero-cpu-v1','observed_F0':{'source_edited_output_bytes_equal':True,'edit_nonzero_RGB_channels':int(len(coords)),'edit_nonzero_pixels':int(np.any(rgb!=0,axis=-1).sum()),'nonzero_columns':np.unique(coords[:,1]).tolist(),'nonzero_values':np.unique(rgb[rgb!=0]).astype(float).tolist(),'confidence_values':np.unique(gpu[...,3]).tolist()},'pins':{str(p):sha(p) for p in [FAIL/'f0-edit-history.rgba32f',FAIL/'f0-current-source.rgba32f',FAIL/'f0-cache-output.rgba32f',VK/'out/cases/zero_edit.json',VK/'out/sky_cache.spvasm',*list(Path(f1['assets'][k]['path']) for k in ['source','edited','depth','mv'])]},'F1_canonical_source_edited_bytes_equal':read(f1,'source').tobytes()==read(f1,'edited').tobytes(),'CPU_replay_from_F0_actual_edit_only':{'scope':'Not F1 GPU observation: only actual F0 edit, CPU-reconstructed guide+pyramid then CPU reprojection/apply','max_error_vs_frozen_F1_oracle':float(error.max()),'pixel00_output':pred[0,0].astype(float).tolist(),'pixel00_expected':expected[0,0].astype(float).tolist(),'pixel00_apply_entered':bool(mask[0,0])},'source_mechanism':'FreshEdit computes log2((nr+eps)/(orig+eps)) without exact equal-input branch. Tiny nonzero low edit survives confidence<1. Apply clamps negative original through max(original,0) unless its effective edit==0 exactly.','F1_actual_GPU_pixels':'UNAVAILABLE_FAILED_BEFORE_DUMP','cause_conclusion':'F0 numerical zero-intent residual confirmed; negative-clamp amplification is reproducible CPU mechanism, NOT confirmed actual F1 mismatch coordinates/cause.','identity_proposal':'Exact bit-equal finite originalRGB==editedRGB implies FreshEdit0/ok1 algebraically; bypass x/x/log2 only for that semantic identity. No residual threshold, epsilon/mask/tolerance change. NOT_APPLIED in diagnostic candidate.','GPU':'NOT_RUN','Wine':'NOT_RUN'}
if __name__=='__main__':print(json.dumps(investigate(),indent=2))
