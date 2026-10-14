#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Small in-memory CPU mocks of frozen producer literals, not a runtime Fixture/schema."""
from pathlib import Path
from unittest.mock import patch
import copy,json,hashlib,struct
import numpy as np
import audit_once as b
import once_pixels as a

def run():
    root=Path(__file__).resolve().parent/'out'/'cpu-metadata-memory-only'
    meta=root/'readback.json';checks=[]
    raw=np.full((720,1280,4),.25,dtype='<f2');raw[...,3]=np.float16(-0.)
    guide=np.zeros((720,1280,4),dtype='<f4');guide[...,0]=2;guide[...,1]=-.1
    ctx,_,_=a.context12_reference(raw.astype('<f4'))
    payload={b.RAW:raw.tobytes(),b.ACTUAL:raw.tobytes(),b.PREV:struct.pack('<I',3),b.NEXT:struct.pack('<I',3),b.SHOWN:guide.tobytes(),b.BANK:guide.tobytes(),b.CONTEXT:ctx.tobytes()}
    def rec(n,image=None):
        r={'file':n,'bytes':len(payload[n]),'sha256':hashlib.sha256(payload[n]).hexdigest()}
        if image:
            w,h,handle=image;r.update(format=109,width=w,height=h,usage=15,image_handle=handle)
        return r
    def im(role,handle):return dict(role=role,image_handle=handle,format=97,width=1280,height=720,usage=15,aspect='COLOR',bytes=7372800)
    j=dict(schema=b.SCHEMA,session='CPU_MOCK_NOT_REAL_GPU',current_source=102,origin_source=100,current_epoch=1,current_history=1,params_version=1,age=2,complete_lineage_sources=[100,101,102],cache_selected_host=True,excluded_from_performance=True,
      job_identity=dict(session='CPU_MOCK_NOT_REAL_GPU',resource_set='cpu-unit-only',generation=0,job=0,origin_source=100,origin_epoch=1,origin_history=1,params_version=1),
      original_image_role_and_extent=im('immutable_unedited_current97',1),actual_SR_input_image_role_and_extent=im('actual_private97_bound_to_DLSS_SR',2),
      copy_tick=5,actual_SR_read_tick=5,merge_tick=7,native_timeline_identity=dict(scheduler_address=1,timeline_semaphore=2,scope='same_Native_renderer_Scheduler_only'),
      actual_GPU_completion_proofs=dict(copy_and_SRread_target_tick=5,copy_and_SRread_observed_tick=5,merge_target_tick=7,merge_observed_tick=7),readback_pixel_bytes=14745600,guard_readback_bytes=4,guard_word_actual=0,written_asset_sha256_bytes=[rec(b.RAW),rec(b.ACTUAL)])
    j['mask_diagnostic']=dict(scope='eligible_selected_current_only;raw_WholeInvalid_partialrecord_NOT_COVERED',current_source=102,origin_source=100,oldest_contributing_origin=100,previous_slot_source=101,epoch=1,history=1,params_version=1,job=0,previous_slot=1,next_slot=0,final_guide_bank=1,whole_host=True,context_written_recorded=True,guard_word_actual=0,previous_mask_word_actual=3,next_mask_word_actual=3,mask_readback_bytes=8,image_readback_bytes=30412800,actual_GPU_completion_proof='same existing copy_SRread_and_merge targets',context_when_unwritten='NOT_READ_NOT_DUMPED',written_assets=[rec(b.PREV),rec(b.NEXT),rec(b.SHOWN,(1280,720,3)),rec(b.BANK,(1280,720,4)),rec(b.CONTEXT,(320,180,5))])
    def execute(m,selector='regional_antipop',p=None,override_meta=None):
        files={root/k:v for k,v in (p or payload).items()};files[meta]=override_meta or json.dumps(m).encode()
        original=Path.read_bytes
        def read(q):return files[q] if q in files else original(q)
        with patch.object(Path,'read_bytes',read):return b.audit_readback(meta,selector)
    def check(name,ok):
        if not ok:raise AssertionError(name)
        checks.append({'name':name,'status':'PASS'})
    result=execute(j);check('exact_e02_regional_fields_pixels_only',result['status']=='PASS_PIXELS_ONLY')
    check('guard_recorded_word_not_invented_file','DOES_NOT_WRITE_guard_FILE' in result['guard_payload_provenance'])
    check('eligible_once_scope_not_raw_or_branch_proof','NOT_MEASURED' in result['scope'] and 'NOT_PROVED' in result['previous_mask_gate_execution'])
    for name,mutate in [
      ('mismatched_current',lambda m:m['mask_diagnostic'].__setitem__('current_source',103)),
      ('wrong_completed_target',lambda m:m['actual_GPU_completion_proofs'].__setitem__('copy_and_SRread_observed_tick',4)),
      ('bool_not_uint',lambda m:m.__setitem__('copy_tick',True)),
      ('wrong_final_bank',lambda m:m['mask_diagnostic'].__setitem__('final_guide_bank',0)),
      ('raw_WholeInvalid_not_eligible',lambda m:m['mask_diagnostic'].__setitem__('whole_host',False)),
      ('mask_recorded_word_vs_bytes',lambda m:m['mask_diagnostic'].__setitem__('next_mask_word_actual',1)),
      ('guide_not_independent',lambda m:m['mask_diagnostic']['written_assets'][3].__setitem__('image_handle',3)),
      ('bad_SHA',lambda m:m['written_asset_sha256_bytes'][0].__setitem__('sha256','0'*64)),
      ('unexpected_asset_alias',lambda m:m['mask_diagnostic']['written_assets'][0].__setitem__('file','prev-mask.u32'))]:
        bad=copy.deepcopy(j);mutate(bad);rejected=False
        try:execute(bad)
        except ValueError:rejected=True
        check(name+'_reject',rejected)
    rejected=False
    try:execute(j,override_meta=('{'+'"schema":"X",'+json.dumps(j)[1:]).encode())
    except ValueError:rejected=True
    check('duplicate_actual_JSON_key_reject',rejected)
    # low_temporal does NOT dump Context. No fabricated 0 image/ctxPASS.
    low=copy.deepcopy(j);p=dict(payload);p.pop(b.CONTEXT);p[b.NEXT]=struct.pack('<I',1);m=low['mask_diagnostic'];m['context_written_recorded']=False;m['next_mask_word_actual']=1;m['image_readback_bytes']=29491200;m['written_assets']=m['written_assets'][:-1];m['written_assets'][1]['sha256']=hashlib.sha256(p[b.NEXT]).hexdigest()
    result=execute(low,'low_temporal',p);check('actual_unwritten_context_no_file_no_dummy',result['status']=='PASS_PIXELS_ONLY' and result['context']['status']=='NOT_APPLICABLE_CONTEXT_NOT_RECORDED' and result['payload_sha256']['current_context32'] is None)
    return {'status':'PASS_CPU_ONLY','checks':checks,'count':len(checks),'source_of_fields':'frozen_e02_native_async_pre_sr_diagnostic.cpp:128-148','GPU':'NOT_RUN','real_once':'NOT_RUN','runtime_fixture_created':False,'disk_pixel_assets_created':False}
if __name__=='__main__':
    result=run();out=Path(__file__).resolve().parent/'out';out.mkdir(exist_ok=True);(out/'adapter-checks.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'status':result['status'],'count':result['count'],'evidence':str(out/'adapter-checks.json')}))
