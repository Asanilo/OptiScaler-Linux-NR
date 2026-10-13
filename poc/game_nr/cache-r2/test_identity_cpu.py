#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Raw32-bit finite predicate and counterexamples; not a GPU execution proof."""
from pathlib import Path
import numpy as np,json,sys
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'sky-cache-cpu'))
import sky_cache_reference as sky
from adapt_identity import GUARD

def predicate(nr,orig):
    assert nr.dtype==orig.dtype==np.dtype('<f4') and nr.shape==orig.shape==(3,)
    return bool(np.array_equal(nr.view('<u4'),orig.view('<u4')) and np.isfinite(nr).all() and np.isfinite(orig).all())
def run(shader,asm):
    checks=[]
    def check(n,b):
        if not bool(b):raise AssertionError(n)
        checks.append({'name':n,'status':'PASS'})
    for name,a in [('positive',[.5,1,1.25]),('negative',[-.25,-1,-0.]),('mixed_signed_zero',[-0.,0.,-0.])]:
        a=np.array(a,dtype='<f4');check('same_raw_bits_'+name,predicate(a,a.copy()))
    a=np.array([.5,.5,.5],dtype='<f4');b=a.copy();b[0]=np.nextafter(b[0],np.float32(np.inf))
    check('one_ULP_unequal_raw_not_guarded',not predicate(b,a) and int(b[0].view('<u4'))-int(a[0].view('<u4'))==1)
    edit,_=sky.fresh_edit(a[None,None,:],b[None,None,:]);check('one_ULP_original_math_still_nonzero',np.any(edit!=0))
    neg=np.array([-.25,-.5,-1],dtype='<f4');other=np.array([-.5,-1,-2],dtype='<f4');check('same_clamped_RGB_not_raw_identity',np.array_equal(np.maximum(neg,0),np.maximum(other,0)) and not predicate(neg,other))
    zero=np.array([-0.,0.,0.],dtype='<f4');positive=zero.copy();positive[0]=0.;check('different_signed_zero_bits_not_guarded',not predicate(zero,positive))
    nan=np.array([0x7fc00001,0x7fc00001,0x7fc00001],dtype='<u4').view('<f4');check('same_NaN_payload_not_guarded',not predicate(nan,nan.copy()))
    inf=np.full(3,np.inf,dtype='<f4');check('same_inf_not_guarded',not predicate(inf,inf.copy()))
    check('finite_vs_NaN_not_guarded',not predicate(nan,a))
    c=json.loads((Path(__file__).resolve().parent.parent/'sky-cache-vk/out/cases/zero_edit.json').read_text())
    for f in c['frames']:
        source=np.fromfile(f['assets']['source']['path'],dtype='<f4').reshape(-1,4);edited=np.fromfile(f['assets']['edited']['path'],dtype='<f4').reshape(-1,4)
        check('frozen_zero_f'+str(f['source'])+'_every_pixel_raw_finite_equal',all(predicate(n[:3],o[:3]) for n,o in zip(edited,source)))
    h=Path(shader).read_text();block=h[h.index('float3 FreshEdit('):h.index('// The model\'s fresh edit at a pixel')]
    check('raw_bit_guard_before_max_original_production_math',GUARD in block and block.index('asuint(nr)')<block.index('nr = max(nr, 0.0)'))
    check('guard_has_no_residual_epsilon_or_threshold',all(w not in GUARD for w in ['gEpsilon','1e-','abs(','<=']))
    assembly=Path(asm).read_text();check('SPIRV_compiles_raw_bitcast_finite_checks',all(s in assembly for s in ['OpBitcast','OpIEqual','OpIsNan','OpIsInf']))
    return {'status':'PASS_CPU_ONLY','checks_count':len(checks),'checks':checks,'GPU':'NOT_RUN','scope':'float32 raw finite equality only; NaN/Inf false, signed-zero unequal bits false, 1ULP unequal retains original math'}
if __name__=='__main__':print(json.dumps(run(sys.argv[1],sys.argv[2]),indent=2))
