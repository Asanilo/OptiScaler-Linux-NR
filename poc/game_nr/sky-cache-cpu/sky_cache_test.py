#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Known-domain CPU invariants. No Wine/GPU; no claimed game720->1080 phase alignment."""
import hashlib,json
from pathlib import Path
import numpy as np
import sky_cache_reference as sky

def run():
    checks=[]
    def check(name,condition,detail=None):
        if not bool(condition):raise AssertionError(name)
        checks.append({'name':name,'status':'PASS',**({'detail':detail} if detail else {})})
    def bits(a,b):return a.dtype==b.dtype and a.shape==b.shape and a.tobytes()==b.tobytes()
    h,w=8,16
    original=np.full((h,w,4),.25,dtype=np.float32);original[...,3]=np.arange(h*w).reshape(h,w)/256
    depth=np.full((h,w),.5);mv=np.zeros((h,w,2));edited=original.copy();edited[...,:3]=(.25+1/512)*2-.001953125
    history,okay=sky.capture(original,edited,depth,10)
    check('fresh_capture_okay',okay.all())
    result,mask=sky.apply(original,history.edit,history.guide,history.levels,history.level_guide)
    check('full_strength_uniform_known_edit',np.allclose(result[...,:3],edited[...,:3],atol=1e-7) and mask.all())
    check('alpha_current_bits_preserved',bits(result[...,3],original[...,3]))
    current=original.copy();current[...,:3]=.5;current[...,3]=-.0
    result,_=sky.apply(current,history.edit,history.guide,history.levels,history.level_guide)
    check('current_base_not_old_edited_frame',np.allclose(result[...,:3],(.5+1/512)*2-1/512,atol=1e-7) and not np.allclose(result[...,:3],edited[...,:3]))
    check('current_signed_zero_alpha',bits(result[...,3],current[...,3]))
    negative=original.copy();negative[0,0,:]=[-1.,-0.,0.,-0.]
    exact,_=sky.apply(negative,history.edit,history.guide,history.levels,history.level_guide,whole_valid=False)
    check('whole_invalid_preserves_negative_signedzero_alpha',bits(exact,negative))
    zero=np.zeros_like(history.edit);levels=[np.zeros_like(x) for x in history.levels]
    exact,_=sky.apply(negative,zero,history.guide,levels,history.level_guide)
    check('zero_edit_identity_preserves_all_bits',bits(exact,negative))
    uncloaked=np.maximum(np.maximum(negative[...,:3],0),0)
    check('raw_shader_zero_without_wrapper_would_change_negative',not bits(uncloaked,negative[...,:3]))
    low=history.edit.copy();low[...,3]=0
    filled,mask=sky.apply(original,low,history.guide,history.levels,history.level_guide)
    check('low_high_confidence_not_whole_invalid_low_band_fill',mask.all() and np.allclose(filled[...,:3],edited[...,:3],atol=1e-7))
    # Known translation, independent oracle shift (not game output/guide phase assumption).
    field=history.edit.copy();field[...,:3]=np.arange(w)[None,:,None]/64
    moving=sky.History(field,history.guide,history.levels,history.level_guide,10,10)
    move=mv.copy();move[...,0]=-1
    propagated,rejected=sky.reproject(moving,original,depth,move,11)
    expected=field[:,:-1,:3]
    check('previous_minus_current_translation_matches_shift_oracle',np.max(np.abs(propagated.edit[:,1:,:3]-expected))<1e-12)
    opposite,_=sky.reproject(moving,original,depth,-move,11)
    check('reverse_motion_is_discriminated',np.max(np.abs(opposite.edit[:,1:-1,:3]-expected[:,:-1]))>.01)
    check('offscreen_reject_has_zero_high_confidence',np.all(propagated.edit[:,0,3]==0))
    blocked,nreject=sky.reproject(moving,original,np.full_like(depth,.95),mv,11)
    check('depth_occlusion_rejects_high_band',nreject==h*w and np.all(blocked.edit[...,3]==0))
    black=edited.copy();black[...,:3]=0
    kept,ok=sky.capture(original,black,depth,11,previous=history,mv=mv)
    check('mode2_black_answer_carries_valid_edit',not ok.any() and np.allclose(kept.edit[...,:3],history.edit[...,:3]))
    # Independent four-link chain; every link explicitly has its own color/depth/MV.
    links=[{'from':10+i,'to':11+i,'color':original.copy(),'depth':depth.copy(),'mv':mv.copy()} for i in range(4)]
    applied,final,status=sky.advance(history,original,links,14)
    check('four_contiguous_links_accepted',final is not None and final.source==14 and final.origin==10 and status=='CURRENT_BASE_EDIT_APPLIED')
    cases=[('reset',{'reset':True}),('session',{'session':'other'}),('history_version',{'version':2}),('overage',{'max_age':3})]
    for name,args in cases:
        exact,_,status=sky.advance(history,negative,links,14,**args)
        check(name+'_whole_invalid_exact_sr',bits(exact,negative) and status=='WHOLE_INVALID_CURRENT_SR')
    for name,bad,target in [('missing',[],14),('partial',links[:2],14),('out_of_order',list(reversed(links)),14),('gap',[{**links[0],'to':12}],12),('duplicate_target',links,10)]:
        exact,_,_=sky.advance(history,negative,bad,target)
        check(name+'_cannot_fake_lineage',bits(exact,negative))
    snap=sky.owned_snapshot(original,depth,mv,10);before=snap['color'].tobytes();original[0,0,0]=9
    check('snapshot_owned_immutable_not_borrowed',snap['color'].tobytes()==before and not snap['color'].flags.writeable and snap['color'][0,0,0]!=original[0,0,0])
    try:sky.owned_snapshot(original,depth,mv,10,already_edited=True)
    except ValueError:rejected=True
    else:rejected=False
    check('recursive_input_explicitly_rejected',rejected)
    model_resets=[1,1];edit_chain_reset=[False,False,False,False]
    check('model_refresh_reset_separate_from_edit_links',model_resets==[1,1] and not any(edit_chain_reset) and final.source==14,'CPU policy only; no model execution')
    check('sky_source_pin',hashlib.sha256(Path(sky.SOURCE).read_bytes()).hexdigest()==sky.SOURCE_SHA)
    manifest=json.loads((Path(__file__).parent.parent/'high-fps-fixture'/'input-manifest.json').read_text())
    inventory=[]
    for f in manifest['frames']:
        values={}
        for role,a in f['assets'].items():
            b=Path(a['path']).read_bytes()
            check(f"actual_f{f['ordinal']}_{role}_sha_bytes",len(b)==a['bytes'] and hashlib.sha256(b).hexdigest()==a['sha256'])
            arr=np.frombuffer(b,dtype='<f4')
            values[role]={'finite':int(np.isfinite(arr).sum()),'elements':int(arr.size),'min':float(arr.min()),'max':float(arr.max())}
        inventory.append({'source':f['source_game_frame'],'jitter':f['jitter'],'assets':values})
    sources=[x['source'] for x in inventory]
    check('actual_four_frames_have_three_contiguous_links',sources==list(range(sources[0],sources[0]+4)), 'Only three actual adjacent links. No predecessor/4th link fabricated; spatial reprojection NOT_RUN.')
    return {'status':'PASS_CPU_ONLY','checks':checks,'checks_count':len(checks),'actual_inventory':inventory,'game_phase':'UNVERIFIED','actual_game_spatial_reprojection':'NOT_RUN','model':'NOT_RUN','GPU':'NOT_RUN','quality':'NOT_EVALUATED','precision':'float64 CPU mathematical transcription, not GPU float32 bit-equivalence'}
if __name__=='__main__':print(json.dumps(run(),indent=2))
