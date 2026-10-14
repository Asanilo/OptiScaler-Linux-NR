#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only one actual P1 current-source/cache-output pair; no invented prior depth/phase or GPU oracle."""
from pathlib import Path
import json,hashlib,sys,numpy as np
sys.path.insert(0,'/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-highfps-worktree/poc/game_nr/sky-cache-cpu')
import sky_cache_reference as sky
P=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/bbport/runs/native-async-pre-sr-game-presr-async-real4-a/native-async-pre-sr-once')
meta=P/'readback.json';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def audit():
 assert sha(meta)=='aade5772d6285a27c5cfd0e387c0102492ff7c12b32eaae2296261feab738bab'
 j=json.loads(meta.read_text());arrays=[];pins=[]
 for v in j['written_asset_sha256_bytes']:
  p=P/v['file'];assert p.stat().st_size==v['bytes'] and sha(p)==v['sha256'];arrays.append(np.fromfile(p,dtype='<f2').astype('<f4').reshape(720,1280,4));pins.append({'path':str(p),'bytes':v['bytes'],'sha256':v['sha256']})
 raw,display=arrays;assert np.isfinite(raw).all() and np.isfinite(display).all();edit,ok=sky.fresh_edit(raw,display);out=raw.copy();nonzero=np.any(edit!=0,axis=-1);rgb=np.maximum((np.maximum(raw[...,:3],0)+1/512)*np.exp2(edit)-1/512,0);out[nonzero,:3]=rgb[nonzero]
 d=display[...,:3]-raw[...,:3];dn=out[...,:3]-raw[...,:3];rms=lambda x:float(np.sqrt(np.mean(np.square(x.astype(np.float64)))))
 result={'status':'PASS_READONLY_SOURCE_OUTPUT_PINS_AND_ENERGY_ONLY','source':j['current_source'],'origin':j['origin_source'],'age':j['age'],'pins':pins,'raw_finite':int(np.isfinite(raw).sum()),'display_finite':int(np.isfinite(display).sum()),'alpha_equal_bits':int(np.count_nonzero(raw[...,3].view('<u4')==display[...,3].view('<u4'))),'actual_display_minus_raw_RGB_RMS':rms(d),'full_edit_reconstruction_minus_raw_RGB_RMS':rms(dn),'full_reconstruction_energy_ratio':rms(dn)/rms(d),'effective_edit_logRGB_RMS':rms(edit),'effective_edit_logLuma_mean':float(np.mean(edit@sky.LUMA)),'effective_edit_logLuma_RMS':rms(edit@sky.LUMA),'changed_RGB_pixels':int(np.count_nonzero(np.any(d!=0,axis=-1))),'no_legal_prior_shown_available':'NO_TEMPORAL_CLAIM; candidate must take full entry effect','scope':'actual current/cache-applied private97 pair, not immutable-origin NR truth or actual prev-shown/depth chain. Full-intensity reconstruction energy only. Cannot evaluate temporal flicker/phase, occlusion or new candidate GPU behavior on this single pair. Newcandidate lowband/AntiPop do not smooth whole-invalid entry/exit by design.','GPU':'NOT_RUN','Wine':'NOT_RUN'}
 assert result['alpha_equal_bits']==720*1280 and result['full_reconstruction_energy_ratio']>.99 # existing effect not hidden by a small gain
 return result
if __name__=='__main__':print(json.dumps(audit(),indent=2))
