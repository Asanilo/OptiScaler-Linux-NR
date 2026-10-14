#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import json,struct,hashlib
import numpy as np
import once_pixels as a

def run():
    checks=[]
    def check(name,ok):
        if not bool(ok):raise AssertionError(name)
        checks.append({'name':name,'status':'PASS'})
    raw=np.zeros((8,16,4),dtype='<f4');raw[...,0]=np.linspace(-.2,4,128,dtype='<f4').reshape(8,16);raw[...,1]=.5;raw[...,2]=.01;raw[...,3]=np.arange(128).reshape(8,16)/256
    reference,_,bound=a.context12_reference(raw)
    check('Mode12_own_reference_inside_fixed_bound',a.compare_context(reference,raw)['status']=='PASS')
    corrupt=reference.copy();corrupt[0,0,0]+=np.float32(100*bound[0,0]);check('wrong_Context_detected_no_tolerance_fit',a.compare_context(corrupt,raw)['status']=='FAIL')
    corrupt=reference.copy();corrupt[0,0,1]=-0.;check('Context_literal_positive_zero_tail_bits',a.compare_context(corrupt,raw)['status']=='FAIL')
    black=np.zeros_like(raw);cref,ideal,b=a.context12_reference(black);check('black_PrintOf_eps_log2_minus9',np.all(cref[...,0]==-9) and a.compare_context(cref,black)['status']=='PASS')
    # Data domain extremes include half-range, near log2(1), negatives and signedzero. No residual-dependent mask.
    extrema=np.zeros_like(raw);extrema[:,0:4,:3]=65504;extrema[:,4:8,:3]=np.float32(1-a.EPS);extrema[:,8:12,:3]=np.float32(2**-24);extrema[:,12:16,:3]=-0.
    eref,_,eb=a.context12_reference(extrema);check('extreme_half_inputs_fixed_bound',a.compare_context(eref,extrema)['status']=='PASS')
    for dtype,size in [('<f2',7),('<u4',3)]:
        rejected=False
        try:a.exact_array(bytes(size),dtype,(2,))
        except ValueError:rejected=True
        check('exact_bytes_reject_'+dtype,rejected)
    # One full720 synthetic call exercises exact real payload lengths, not a GPU fixture.
    r=np.full((a.HEIGHT,a.WIDTH,4),.25,dtype='<f2');r[...,3]=np.float16(-0.);r[0,0,0]=np.float16(-.25)
    ctx,_,_=a.context12_reference(r.astype('<f4'));guide=np.zeros((a.HEIGHT,a.WIDTH,4),dtype='<f4');guide[...,0]=2;guide[...,1]=.5
    args=dict(raw97=r.tobytes(),sr_input97=r.tobytes(),guard4=struct.pack('<I',0),masks8=struct.pack('<II',0,3),shown_guide32=guide.tobytes(),final_guide32=guide.tobytes(),current_context32=ctx.tobytes(),selector='regional_antipop',whole_eligible=True,context_written=True)
    good=a.audit_bytes(**args);check('full_shape_pixel_scope_PASS',good['status']=='PASS_PIXELS_ONLY');check('zeroedit_not_inference_or_visible_effect_PASS',good['edit_energy']['nonzero_edit_acceptance']=='NO_OBSERVED_EDIT');check('signedzero_alpha_original_bits',good['alpha_equal_pixels']==a.WIDTH*a.HEIGHT)
    bad=dict(args);bad['masks8']=struct.pack('<II',0,1);check('regional_missing_bit1_FAIL',a.audit_bytes(**bad)['status']=='FAIL_PIXELS')
    bad=dict(args);bad['guard4']=struct.pack('<I',1);bad['masks8']=struct.pack('<II',3,0);fallback=a.audit_bytes(**bad);check('guard_real_raw_bit_identity_and_mask0',fallback['status']=='PASS_PIXELS_ONLY' and fallback['raw_guard_fallback']=='PASS')
    changed=r.copy();changed[0,0,0]=np.float16(.5);bad['sr_input97']=changed.tobytes();check('guard_failure_nonraw_FAIL',a.audit_bytes(**bad)['status']=='FAIL_PIXELS')
    bad=dict(args);alter=guide.copy();alter[0,0,0]=3;bad['shown_guide32']=alter.tobytes();check('CURRENT_independent_guide_mismatch_FAIL',a.audit_bytes(**bad)['status']=='FAIL_PIXELS')
    bad=dict(args);nan_input=r.copy();nan_input[0,0,0]=np.nan;bad['sr_input97']=nan_input.tobytes();nan_audit=a.audit_bytes(**bad)
    check('nonfinite_payload_FAIL_and_strict_JSON_null',nan_audit['status']=='FAIL_PIXELS' and nan_audit['edit_energy']['RGB_difference_RMS'] is None)
    json.dumps(nan_audit,allow_nan=False)
    broken=reference.copy();broken[0,0,0]=np.nan;context_bad=a.compare_context(broken,raw)
    check('nonfinite_Context_FAIL_and_strict_JSON_null',context_bad['status']=='FAIL' and context_bad['first_mismatches'][0]['actual'] is None)
    json.dumps(context_bad,allow_nan=False)
    # Independent float32 expression transcription, row-major sequential16 additions.
    for source in [raw,black,extrema]:
        p=np.asarray(np.maximum(source[...,:3],np.float32(0))+np.float32(a.EPS),dtype='<f4');v=np.asarray(p-np.float32(a.EPS),dtype='<f4');t=np.asarray(v*np.asarray(a.LUMA,dtype='<f4'),dtype='<f4');dot=np.asarray(np.asarray(t[...,0]+t[...,1],dtype='<f4')+t[...,2],dtype='<f4');log=np.asarray(np.log2((dot+np.float32(a.EPS)).astype(np.float64)),dtype='<f4')
        expected=np.zeros((2,4,4),dtype='<f4')
        for y in range(2):
            for x in range(4):
                acc=np.float32(0)
                for yy in range(4):
                    for xx in range(4):acc=np.float32(acc+log[y*4+yy,x*4+xx])
                expected[y,x,0]=np.float32(acc/np.float32(16))
        check('scalar_float32_rowmajor16_'+str(len(checks)),a.compare_context(expected,source)['status']=='PASS')
    return {'status':'PASS_CPU_ONLY','checks':checks,'count':len(checks),'rounding_policy':'FIXED_BEFORE_GPU_EVIDENCE','real_once':'NOT_RUN','metadata_adapter':'PENDING_B_FIELDS','GPU':'NOT_RUN'}
if __name__=='__main__':
    result=run();root=Path(__file__).resolve().parent;out=root/'out';out.mkdir(exist_ok=True);names=['.gitignore','README.md','once_pixels.py','test_once_pixels.py'];result['sources']={str(root/n):hashlib.sha256((root/n).read_bytes()).hexdigest() for n in names};(out/'cpu-checks.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'status':result['status'],'count':result['count'],'CPU_evidence':str(out/'cpu-checks.json'),'sha256':hashlib.sha256((out/'cpu-checks.json').read_bytes()).hexdigest()}))
