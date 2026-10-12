#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Single bounded host-only test pass; frozen Sky mathematical functions unchanged."""
import hashlib,json
from dataclasses import replace
from pathlib import Path
import numpy as np
import sky_cache_reference as sky
from late_arrival import Snapshot,Arrival,catch_up,digest

def run():
    checks=[];accepted=[]
    def check(name,value):
        if not bool(value):raise AssertionError(name)
        checks.append({'name':name,'status':'PASS'})
    def bits(a,b):return a.dtype==b.dtype and a.shape==b.shape and a.tobytes()==b.tobytes()
    h,w=8,16;depth=np.full((h,w),.5,dtype=np.float32);mv=np.zeros((h,w,2),dtype=np.float32)
    original=np.full((h,w,4),.25,dtype=np.float32);original[...,:3]+=np.arange(w)[None,:,None]/256;original[...,3]=-0.
    def own(c,d,m,s,**kw):return Snapshot.own(c,d,m,source=s,link_from=s-1,session='s',version=3,epoch=7,**kw)
    origin=own(original,depth,mv,100)
    edited=original.copy();edited[...,:3]=(original[...,:3]+1/512)*np.exp2(.5+np.arange(w)[None,:,None]/64)-1/512
    arrival=Arrival.own(edited,origin=100,session='s',version=3,epoch=7)
    links=[]
    for n in range(1,5):
        c=original.copy();c[...,:3]+=n/8;c[...,3]=np.arange(h*w).reshape(h,w)/256+n/16
        m=mv.copy();m[...,0]=-1;links.append(own(c,depth,m,100+n))
    before=[digest(x.color) for x in [origin,*links]]
    immediate,_=sky.capture(origin.color,arrival.edited,origin.depth,origin.source);immediate.session='s';immediate.version=3
    for n,link in enumerate(links,1):
        immediate,_=sky.reproject(immediate,link.color,link.depth,link.mv,link.source,link.jitter_delta)
        immediate_out,_=sky.apply(link.color,immediate.edit,immediate.guide,immediate.levels,immediate.level_guide)
        late=catch_up(origin,arrival,links[:n],link);accepted.append(late.record)
        check(f'complete_{n}_links_equals_immediate_current_output',late.record['accepted'] and bits(late.output,immediate_out))
        check(f'complete_{n}_links_same_history_source_origin',late.history.source==link.source and late.history.origin==100 and np.array_equal(late.history.edit,immediate.edit) and np.array_equal(late.history.guide,immediate.guide))
        check(f'complete_{n}_links_exact_API_counts',late.record['capture_calls']==1 and late.record['reproject_calls']==n and late.record['apply_calls']==1)
        check(f'complete_{n}_links_current_alpha_bits',bits(late.output[...,3],link.color[...,3]))
    current=links[-1];late=catch_up(origin,arrival,links,current)
    wrong,_=sky.capture(current.color,arrival.edited,current.depth,current.source)
    wrong_out,_=sky.apply(current.color,wrong.edit,wrong.guide,wrong.levels,wrong.level_guide)
    check('arrival_source_different_origin_wrong_capture_discriminated',current.source!=origin.source and not np.allclose(wrong_out[...,:3],late.output[...,:3]))
    check('capture_provenance_origin_color_and_depth',late.record['origin_color_sha256']==digest(origin.color) and late.record['origin_depth_sha256']==digest(origin.depth))
    original[0,0,0]=9;edited[0,0,0]=9
    check('origin_and_arrival_are_owned_not_borrowed',origin.color[0,0,0]!=9 and arrival.edited[0,0,0]!=9)
    try:origin.color.flags.writeable=True
    except ValueError:immutable=True
    else:immutable=False
    check('origin_bytes_cannot_be_overwritten_by_cache_output',immutable)
    check('all_unedited_snapshot_hashes_unchanged',before==[digest(x.color) for x in [origin,*links]])
    try:Snapshot.own(late.output,depth,mv,source=104,link_from=103,session='s',version=3,epoch=7,already_edited=True)
    except ValueError:rejected=True
    else:rejected=False
    check('recursive_output_not_accepted_as_input',rejected)
    negative=current.color.copy();negative[0,0]=[-1,-0.,0.,-0.];negative_current=own(negative,depth,current.mv,104)
    # Every rejection must be whole bit-identity, and precede any Sky call.
    cases=[('pending',None,links,current),('not_ready',replace(arrival,ready=False),links,current),('missing',arrival,[],current),('partial',arrival,links[:2],current),('reverse_order',arrival,list(reversed(links)),current),('gap',arrival,[replace(links[0],link_from=99),*links[1:]],current),('reset',arrival,[links[0],replace(links[1],reset=True),*links[2:]],current),('epoch_link',arrival,[replace(links[0],epoch=8),*links[1:]],current),('version_link',arrival,[replace(links[0],version=4),*links[1:]],current),('session_link',arrival,[replace(links[0],session='other'),*links[1:]],current),('epoch_arrival',replace(arrival,epoch=8),links,current),('arrival_origin',replace(arrival,origin=99),links,current),('more_than4',arrival,[*links,links[-1]],current),('age5',arrival,links,replace(current,source=105)),('origin_reset',arrival,links,current),('current_epoch',arrival,links,replace(current,epoch=8)),('current_payload_mismatch',arrival,links,negative_current)]
    for name,a,l,c in cases:
        o=replace(origin,reset=True) if name=='origin_reset' else origin
        out=catch_up(o,a,l,c)
        check(name+'_bit_exact_current_no_partial_accept',not out.record['accepted'] and bits(out.output,c.color) and out.history is None and out.record['capture_calls']==out.record['reproject_calls']==out.record['apply_calls']==0)
    for name in ['pending','gap','reset','epoch','age']:
        l=links if name!='gap' else links[:1];c=negative_current
        a=None if name=='pending' else arrival
        if name=='reset':c=replace(c,reset=True)
        if name=='epoch':c=replace(c,epoch=8)
        if name=='age':c=replace(c,source=105)
        out=catch_up(origin,a,l,c);check(name+'_negative_signedzero_alpha_fallback',bits(out.output,c.color))
    # Zero EDIT at origin; source changes before arrival, including negative and +/-zero.
    z0=origin.color.copy();z0[0,0]=[-1.,-0.,0.,-0.];zo=own(z0,depth,mv,100);za=Arrival.own(z0,origin=100,session='s',version=3,epoch=7)
    zl=[]
    for n in range(1,5):
        z=z0.copy();z[...,:3]+=n/16;z[0,0]=[-1-n/16,-0.,0.,-0.];zl.append(own(z,depth,mv,100+n))
        out=catch_up(zo,za,zl,zl[-1]);check(f'zero_edit_{n}_links_exact_changed_current_negative_signedzero_alpha',out.record['accepted'] and bits(out.output,zl[-1].color))
    # Distinct origin depth proves capture uses original depth; immediate mode is the reference.
    different_depth=depth.copy();different_depth[:,6:10]=.95
    dl=[own(x.color,different_depth,x.mv,x.source) for x in links]
    immediate,_=sky.capture(origin.color,arrival.edited,origin.depth,100)
    for x in dl:immediate,_=sky.reproject(immediate,x.color,x.depth,x.mv,x.source)
    expected,_=sky.apply(dl[-1].color,immediate.edit,immediate.guide,immediate.levels,immediate.level_guide)
    out=catch_up(origin,arrival,dl,dl[-1]);check('distinct_origin_depth_catchup_equals_immediate',bits(out.output,expected) and out.record['origin_depth_sha256']!=digest(dl[-1].depth))
    check('frozen_original_Sky_source_pin',hashlib.sha256(Path(sky.SOURCE).read_bytes()).hexdigest()==sky.SOURCE_SHA)
    return {'schema':'sky-late-arrival-cpu-result-v1','status':'PASS_CPU_ONLY','checks_count':len(checks),'checks':checks,'accepted_lineages':accepted,'limits':{'saved_links':4,'age':4,'test_pixels':128},'scope':'known aligned synthetic only; capture immutable origin-source/origin-depth, atomic whole-chain acceptance; same unmodified Sky math as immediate mode','GPU':'NOT_RUN','Wine':'NOT_RUN','model':'NOT_RUN','native_nonblocking_delivery':'NOT_IMPLEMENTED','GPU_fence':'NOT_IMPLEMENTED','real_720_to_1080_phase':'UNVERIFIED','quality':'NOT_EVALUATED'}
if __name__=='__main__':print(json.dumps(run(),indent=2))
