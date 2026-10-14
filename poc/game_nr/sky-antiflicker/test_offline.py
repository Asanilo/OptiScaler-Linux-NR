#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import json,sys,hashlib
from pathlib import Path
import numpy as np
from temporal_reference import *
from adapt_antiflicker import mode_block

def run():
 checks=[]
 def check(name,b):
  if not bool(b):raise AssertionError(name)
  checks.append({'name':name,'status':'PASS'})
 h,w=8,16;raw=np.full((h,w,4),.5,dtype='<f4');raw[...,3]=np.arange(h*w,dtype='<f4').reshape(h,w)/256;depth=np.full((h,w),.5);mv=np.zeros((h,w,2));guide=np.stack([sky.linear_depth(depth),sky.log_luma(raw)],axis=-1);edit=np.full((h,w,4),.25);edit[...,3]=1.;ctx=context(raw)
 out,shown=temporal(raw,edit,edit,guide,guide,depth,mv,ctx=ctx,prev_ctx=ctx)
 check('constant_full_edit_not_diminished',np.allclose(shown[...,:3],.25,atol=1e-12));check('constant_full_output_actual_positive_effect',np.all(out[...,:3]>raw[...,:3]));check('alpha_bits_preserved',np.array_equal(out[...,3].view('<u4'),raw[...,3].view('<u4')))
 special=raw.copy();special[0,0,:3]=[-.5,-0.,0.];zero=np.zeros_like(edit)
 result,z=temporal(special,zero,zero,guide,guide,depth,mv,prior_valid=False);check('zero_edit_negative_signedzero_alpha_bitidentity',result.tobytes()==special.tobytes())
 for reason in ['missing_chain','gap','reset','epoch_mismatch','age5']:
  result,z=temporal(special,edit,edit,guide,guide,depth,mv,whole=False);check(reason+'_whole_invalid_exact_current',result.tobytes()==special.tobytes())
 changed=guide.copy();changed[...,0]*=4
 result,z=temporal(raw,edit,zero,guide,changed,depth,mv,ctx=ctx,prev_ctx=ctx);check('occlusion_does_not_blend_old_shown',np.allclose(z[...,:3],edit[...,:3]));check('no_prior_history_full_target',np.allclose(temporal(raw,edit,zero,guide,guide,depth,mv,prior_valid=False)[1][...,:3],edit[...,:3]))
 new=edit.copy();new[...,:3]=.255;old=edit.copy();result,z=temporal(raw,new,old,guide,guide,depth,mv,ctx=ctx,prev_ctx=ctx);target_delta=float(np.mean(new[...,:3]-old[...,:3]));shown_delta=float(np.mean(z[...,:3]-old[...,:3]));check('lowband_reduces_small_refresh_step_not_full_edit',0<shown_delta<target_delta);check('transition_does_not_attenuate_full_prior_edit',np.mean(z[...,:3])>=np.mean(old[...,:3]))
 new[...,:3]=.85;result,z=temporal(raw,new,old,guide,guide,depth,mv,low_temporal=0,anti_step=.75/60,ctx=ctx,prev_ctx=ctx);check('upstream_antipop_bounds_still_regional_delta',abs(float(np.mean(z[...,:3])- .25))<=.75/60+1e-12)
 lit=raw.copy();lit[...,:3]=1.;litctx=context(lit);result,z=temporal(lit,new,old,guide,guide,depth,mv,low_temporal=0,anti_step=.75/60,ctx=litctx,prev_ctx=ctx);check('real_context_light_change_bypasses_antipop',np.allclose(z[...,:3],new[...,:3]))
 # Low pixel confidence still admits legitimate regional fill, exactly existing Sky Apply math.
 history,ok=sky.capture(raw,raw*np.array([1.1,1.1,1.1,1]),depth,1);history.edit[...,3]=0;filled,mask=sky.apply(raw,history.edit,history.guide,history.levels,history.level_guide);check('low_highband_confidence_is_not_whole_invalid',np.any(filled[...,:3]>raw[...,:3]))
 # Measured short cohort morphology: no history at entry, then invalid raw. NOT smoothed by these candidates.
 entry,z=temporal(raw,edit,zero,guide,guide,depth,mv,prior_valid=False,anti_step=.75/60,ctx=ctx,prev_ctx=ctx)
 base_entry,_=temporal(raw,edit,zero,guide,guide,depth,mv,prior_valid=False,low_temporal=0,ctx=ctx,prev_ctx=ctx)
 check('cohort_entry_full_target_not_faded_from_raw',entry.tobytes()==base_entry.tobytes())
 for f in range(4):
  leave,_=temporal(raw,edit,edit,guide,guide,depth,mv,whole=False,anti_step=.75/60,ctx=ctx,prev_ctx=ctx)
  check('cohort_invalid_raw_f'+str(f)+'_does_not_fade_old_edit',leave.tobytes()==raw.tobytes())
 r=Path(__file__).resolve().parent;shader=(r/'out/sky_antiflicker.hlsl').read_text();u=(r/'provenance/skynizz-6bfc4389-cache.hlsl').read_text();start=u.index('                        // Anti light pop-in.');end=u.index('                        hist += lc - lh;',start)
 base=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-highfps-worktree/poc/game_nr/cache-r2/out/sky_cache_vk.hlsl').read_text();check('Capture_and_Reproject_branches_byteidentical_r2',mode_block(base,'1 || gMode == 2')==mode_block(shader,'1 || gMode == 2'));check('Pyramid3_byteidentical_r2',mode_block(base,3)==mode_block(shader,3));
 check('AntiPop_block_exact_upstream_not_new_math',u[start:end] in shader);check('Context12_exact_upstream',mode_block(u,12) in shader);check('r2_finite_raw_bit_identity_still_present','all(asuint(nr) == asuint(orig))' in shader and 'all(isfinite(nr))' in shader);check('r2_full_strength_gains_present','gLowGain * low + gHighGain * hist.a * high' in shader);check('no_age_epsilon_gain_change',all(x not in shader for x in ['gAgeHalfLife','gAgeNeutral']))
 asm=(r/'out/sky_antiflicker.spvasm').read_text();check('SPIRV_binding21_22_present',all('Binding '+str(n) in asm for n in [21,22]));check('SPIRV_WholeValid156_AntiPop160_Ctx172',all('Offset '+str(n) in asm for n in [156,160,164,168,172]));check('SPIRV_no_new_extra_features',set(line.split('OpCapability ')[1].strip() for line in asm.splitlines() if 'OpCapability ' in line)=={'Shader'})
 return {'status':'PASS_CPU_ONLY','checks':checks,'count':len(checks),'energy':{'constant_full_edit':.25,'small_step_target':target_delta,'small_step_shown':shown_delta,'constant_output_vs_raw_RMS':float(np.sqrt(np.mean((out[...,:3]-raw[...,:3])**2)))},'scope':'aligned synthetic expression/lineage/layout gates only; no game jitter, actual GPU, ghosting/quality/visibility/performance acceptance','GPU':'NOT_RUN'}
if __name__=='__main__':print(json.dumps(run(),indent=2))
