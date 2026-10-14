#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exact e02b6bc cold readback adapter; read-only inputs, no new native schema.
CLI: audit_once.py READBACK_JSON --selector low_temporal|regional_antipop [--output NEW_JSON]
"""
from pathlib import Path
import argparse,hashlib,json,re,struct,sys
import once_pixels as pixels
PRODUCER='e02b6bc49a76636e7e63ae95c1041d65c786e2d1'
SCHEMA='native-async-pre-sr-once-readback-v1'
RAW='original-current720.rgba16f';ACTUAL='actual-sr-input720.rgba16f'
PREV='previous-mask.u32';NEXT='next-mask.u32'
SHOWN='current-owned-shown-guide720.rgba32f';BANK='current-final-bank-guide720.rgba32f';CONTEXT='current-context320x180.rgba32f'
IMG97_BYTES=7372800;GUIDE_BYTES=14745600;CONTEXT_BYTES=921600

def require(ok,message):
    if not ok:raise ValueError(message)
def uint(value,label,bits=64,positive=False):
    require(type(value) is int and (1 if positive else 0)<=value<2**bits,'strict uint: '+label)
    return value
def boolean(value,label):
    require(type(value) is bool,'strict bool: '+label);return value
def duplicate_reject(pairs):
    result={}
    for k,v in pairs:
        require(k not in result,'duplicate JSON key: '+k);result[k]=v
    return result

def load_assets(root,records,expected,image_names):
    require(type(records) is list,'literal asset list')
    names=[r['file'] for r in records];require(len(names)==len(set(names)) and set(names)==set(expected),'exact producer asset names; no alias or extra')
    loaded={};metadata={}
    for r in records:
        name=r['file'];require(uint(r['bytes'],'asset bytes')==expected[name],'asset byte declaration '+name)
        require(type(r['sha256']) is str and re.fullmatch('[0-9a-f]{64}',r['sha256']) is not None,'SHA literal '+name)
        q=(root/name).resolve();require(q.parent==root,'asset stays in same once directory')
        data=q.read_bytes();require(len(data)==expected[name] and hashlib.sha256(data).hexdigest()==r['sha256'],'actual asset byte/hash mismatch '+name)
        if name in image_names:
            width,height=image_names[name]
            require(uint(r['format'],'format',32)==109 and uint(r['width'],'width',32)==width and uint(r['height'],'height',32)==height,'actual image format/extent '+name)
            require(uint(r['usage'],'usage',32)&1 and uint(r['image_handle'],'image handle',positive=True),'actual image transferSrc/handle '+name)
        loaded[name]=data;metadata[name]=r
    return loaded,metadata

def audit_readback(path,selector):
    require(selector in ('low_temporal','regional_antipop'),'exact caller-selected actual runtime selector')
    path=Path(path).resolve();data=path.read_bytes()
    j=json.loads(data,object_pairs_hook=duplicate_reject,parse_constant=lambda s:(_ for _ in ()).throw(ValueError('nonfinite JSON constant '+s)))
    require(j['schema']==SCHEMA,'exact existing producer schema')
    require(j['cache_selected_host'] is True and j['excluded_from_performance'] is True,'eligible cold only, excluded from performance')
    current=uint(j['current_source'],'current',positive=True);origin=uint(j['origin_source'],'origin',positive=True)
    require(origin<=current and uint(j['age'],'age')==current-origin<=4,'actual current/origin age<=4')
    line=j['complete_lineage_sources'];require(type(line) is list and line==list(range(origin,current+1)) and all(type(v) is int for v in line),'exact complete original lineage')
    job=j['job_identity'];require(type(j['session']) is str and j['session'] and job['session']==j['session'],'actual session')
    require(type(job['resource_set']) is str and job['resource_set'] and uint(job['generation'],'generation')==0,'actual resource_set/generation')
    require(uint(job['origin_source'],'job origin')==origin and uint(job['origin_epoch'],'job epoch')==uint(j['current_epoch'],'epoch') and uint(job['origin_history'],'job history')==uint(j['current_history'],'history') and uint(job['params_version'],'job params')==uint(j['params_version'],'params'),'job/current common identity')
    source_images=[]
    for key,role in [('original_image_role_and_extent','immutable_unedited_current97'),('actual_SR_input_image_role_and_extent','actual_private97_bound_to_DLSS_SR')]:
        r=j[key];require(r['role']==role and uint(r['format'],'source format',32)==97 and uint(r['width'],'source width',32)==1280 and uint(r['height'],'source height',32)==720 and r['aspect']=='COLOR' and uint(r['bytes'],'source bytes')==IMG97_BYTES and uint(r['usage'],'source usage',32)&1,'literal original/SR image CI')
        source_images.append(uint(r['image_handle'],'source handle',positive=True))
    require(source_images[0]!=source_images[1],'actual independent original/SR images')
    copy=uint(j['copy_tick'],'copy tick',positive=True);sr=uint(j['actual_SR_read_tick'],'SR tick',positive=True);merge=uint(j['merge_tick'],'merge tick',positive=True)
    require(copy==sr and merge>=copy,'actual same-source SR/copy/merge order')
    proof=j['actual_GPU_completion_proofs'];require(uint(proof['copy_and_SRread_target_tick'],'copy target')==copy and uint(proof['copy_and_SRread_observed_tick'],'copy observed')>=copy and uint(proof['merge_target_tick'],'merge target')==merge and uint(proof['merge_observed_tick'],'merge observed')>=merge,'published actual GPU target/observation fields')
    timeline=j['native_timeline_identity'];uint(timeline['scheduler_address'],'scheduler',positive=True);uint(timeline['timeline_semaphore'],'timeline',positive=True)
    require(timeline['scope']=='same_Native_renderer_Scheduler_only','Native timeline scope')
    require(uint(j['readback_pixel_bytes'],'pixel total')==2*IMG97_BYTES and uint(j['guard_readback_bytes'],'guard bytes')==4,'actual old readback byte totals')
    m=j['mask_diagnostic'];require(m['scope']=='eligible_selected_current_only;raw_WholeInvalid_partialrecord_NOT_COVERED','exact cold-only scope')
    for key,top in [('current_source','current_source'),('origin_source','origin_source'),('epoch','current_epoch'),('history','current_history'),('params_version','params_version')]:
        require(uint(m[key],'mask '+key)==j[top],'mask/top identity '+key)
    require(uint(m['job'],'mask job')==uint(job['job'],'job'),'mask job identity')
    oldest=uint(m['oldest_contributing_origin'],'oldest',positive=True);require(oldest<=origin and current-oldest<=4,'contributing age, no new-origin relabel')
    previous_source=uint(m['previous_slot_source'],'previous source');require(previous_source<current,'previous slot source order')
    next_slot=uint(m['next_slot'],'next slot',32);prev_slot=uint(m['previous_slot'],'prev slot',32);bank=uint(m['final_guide_bank'],'guide bank',32)
    require(next_slot<2 and prev_slot==1-next_slot and bank==len(line)%2,'physical mask/guide slots and final CURRENT bank')
    whole=boolean(m['whole_host'],'whole');ctx_written=boolean(m['context_written_recorded'],'context_written')
    require(whole and ctx_written==(selector=='regional_antipop'),'eligible mask scope / actual Context12 branch')
    flag=uint(j['guard_word_actual'],'actual guard',32);require(uint(m['guard_word_actual'],'mask guard',32)==flag,'same actual guard word')
    require(uint(m['mask_readback_bytes'],'mask bytes')==8 and uint(m['image_readback_bytes'],'image bytes')==2*GUIDE_BYTES+(CONTEXT_BYTES if ctx_written else 0),'literal actual cold byte totals')
    require(m['actual_GPU_completion_proof']=='same existing copy_SRread_and_merge targets' and m['context_when_unwritten']=='NOT_READ_NOT_DUMPED','actual cold completion/unwritten scope')
    old,_=load_assets(path.parent,j['written_asset_sha256_bytes'],{RAW:IMG97_BYTES,ACTUAL:IMG97_BYTES},{})
    wanted={PREV:4,NEXT:4,SHOWN:GUIDE_BYTES,BANK:GUIDE_BYTES};images={SHOWN:(1280,720),BANK:(1280,720)}
    if ctx_written:wanted[CONTEXT]=CONTEXT_BYTES;images[CONTEXT]=(320,180)
    extra,records=load_assets(path.parent,m['written_assets'],wanted,images)
    require(records[SHOWN]['image_handle']!=records[BANK]['image_handle'],'guide pair must be physically independent')
    require(struct.unpack('<I',extra[PREV])[0]==uint(m['previous_mask_word_actual'],'prev word',32) and struct.unpack('<I',extra[NEXT])[0]==uint(m['next_mask_word_actual'],'next word',32),'mask actual bytes vs recorded words')
    result=pixels.audit_bytes(raw97=old[RAW],sr_input97=old[ACTUAL],guard4=struct.pack('<I',flag),masks8=extra[PREV]+extra[NEXT],shown_guide32=extra[SHOWN],final_guide32=extra[BANK],current_context32=extra.get(CONTEXT),selector=selector,whole_eligible=whole,context_written=ctx_written)
    result['metadata_adapter']='EXACT_e02b6bc_LITERAL_NO_ALIASES'
    result['metadata']={'path':str(path),'sha256':hashlib.sha256(data).hexdigest(),'producer_commit':PRODUCER,'session':j['session'],'current_source':current,'origin_source':origin,'previous_slot_source':previous_source,'next_slot':next_slot,'previous_slot':prev_slot,'final_bank':bank,'job':job['job'],'copy_tick':copy,'merge_tick':merge,'selector_from_caller_runtime_evidence':selector,'selector_not_in_readback_JSON':True}
    result['guard_payload_provenance']='PACKED_FROM_ACTUAL_GPU_READBACK_METADATA_WORD; producer DOES_NOT_WRITE_guard_FILE'
    result['recorded_identity_and_completion']='CONSISTENT_PUBLISHED_TARGETS_ONLY; ROOT_MUST_JOIN_ACTUAL_RUNTIME_PINS_LOGS'
    result['current_guide_identity']='physical/current metadata and exact pair verified; actual final-bank content oracle requires source depth beyond this dump'
    result['previous_mask_gate_execution']='NOT_PROVED_BY_ELIGIBLE_ONCE; stored pair alone does not prove previous branch execution'
    result['Present_API_FD_retirement']='REQUIRES_INDEPENDENT_ROOT_LOG_AND_NONREUSE_PRESENT_JOIN'
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('readback_json');parser.add_argument('--selector',required=True,choices=['low_temporal','regional_antipop']);parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    try:result=audit_readback(args.readback_json,args.selector)
    except (ValueError,KeyError,OSError,TypeError) as e:result={'status':'FAIL_METADATA_OR_ASSET','reason':str(e),'GPU_run_by_auditor':False}
    text=json.dumps(result,indent=2,allow_nan=False)+'\n'
    if args.output:
        with args.output.open('x') as f:f.write(text)
    print(text,end='');return 0 if result['status']=='PASS_PIXELS_ONLY' else 2
if __name__=='__main__':sys.exit(main())
