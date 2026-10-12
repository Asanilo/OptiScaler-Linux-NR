#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded CPU lineage adapter only. Sky math is imported unchanged; no GPU/native delivery."""
from dataclasses import dataclass
import hashlib
import numpy as np
import sky_cache_reference as sky

def immutable(a):
    a=np.asarray(a)
    # Bytes backing prevents setflags(write=True), unlike a merely readonly owning array.
    return np.frombuffer(a.tobytes(order='C'),dtype=a.dtype).reshape(a.shape)

def digest(a):return hashlib.sha256(a.tobytes(order='C')).hexdigest()

def valid_id(x):return type(x)==int and 0<=x<=2**53-1

@dataclass(frozen=True)
class Snapshot:
    source:int
    link_from:int
    session:str
    version:int
    epoch:int
    color:np.ndarray
    depth:np.ndarray
    mv:np.ndarray
    reset:bool=False
    jitter_delta:tuple=(0.,0.)
    unedited:bool=True

    @classmethod
    def own(cls,color,depth,mv,*,source,link_from,session,version,epoch,reset=False,jitter_delta=(0.,0.),already_edited=False):
        if already_edited:raise ValueError('cached output cannot be declared an unedited SR snapshot')
        if not all(valid_id(x) for x in (source,link_from,version,epoch)):raise ValueError('strict bounded source/history IDs')
        if type(session)!=str or not session or type(reset)!=bool:raise ValueError('snapshot identity/reset')
        color,depth,mv=map(np.asarray,(color,depth,mv))
        if color.ndim!=3 or color.shape[2]!=4 or depth.shape!=color.shape[:2] or mv.shape!=(*depth.shape,2) or not 0<depth.size<=4096:raise ValueError('small known aligned geometry only')
        if any(a.dtype.kind!='f' or not np.isfinite(a).all() for a in (color,depth,mv)):raise ValueError('finite floating snapshot')
        if len(jitter_delta)!=2 or not np.isfinite(jitter_delta).all():raise ValueError('explicit cache jitter delta')
        return cls(source,link_from,session,version,epoch,immutable(color),immutable(depth),immutable(mv),reset,tuple(jitter_delta))

@dataclass(frozen=True)
class Arrival:
    origin:int
    session:str
    version:int
    epoch:int
    edited:np.ndarray
    ready:bool=True

    @classmethod
    def own(cls,edited,*,origin,session,version,epoch,ready=True):
        if not all(valid_id(x) for x in (origin,version,epoch)) or type(ready)!=bool:raise ValueError('arrival identity/ready')
        return cls(origin,session,version,epoch,immutable(edited),ready)

@dataclass
class Outcome:
    output:np.ndarray
    history:object
    reason:str
    record:dict

def catch_up(origin,arrival,links,current):
    """Capture origin pair, then validated raw links, then apply CURRENT; never partial acceptance."""
    record={'current_source':current.source,'effect_origin':origin.source,'effect_age':current.source-origin.source,'session':current.session,'version':current.version,'epoch':current.epoch,'arrival_source':current.source,'capture_calls':0,'reproject_calls':0,'apply_calls':0,'origin_color_sha256':digest(origin.color),'origin_depth_sha256':digest(origin.depth),'current_source_sha256':digest(current.color),'model':'NOT_RUN','GPU':'NOT_RUN','native_delivery':'NOT_IMPLEMENTED','real_720_1080_phase':'UNVERIFIED'}
    def fallback(reason):
        record['accepted']=False;record['reason']=reason
        return Outcome(current.color.copy(),None,reason,record)
    if arrival is None or not arrival.ready:return fallback('PENDING_CURRENT_SR')
    if len(links)>4:return fallback('TOO_MANY_LINKS_CURRENT_SR')
    age=current.source-origin.source
    if age<0 or age>4:return fallback('AGE_INVALID_CURRENT_SR')
    if arrival.origin!=origin.source:return fallback('ARRIVAL_ORIGIN_MISMATCH_CURRENT_SR')
    key=(origin.session,origin.version,origin.epoch)
    if (arrival.session,arrival.version,arrival.epoch)!=key or (current.session,current.version,current.epoch)!=key:return fallback('IDENTITY_MISMATCH_CURRENT_SR')
    if origin.reset or current.reset:return fallback('RESET_CURRENT_SR')
    if not origin.unedited or not current.unedited:return fallback('EDITED_SNAPSHOT_CURRENT_SR')
    if arrival.edited.shape!=origin.color.shape or arrival.edited.dtype.kind!='f' or not np.isfinite(arrival.edited).all():return fallback('INVALID_EDITED_RESULT_CURRENT_SR')
    # Validate the ENTIRE chain before any Capture/Reproject/Apply. No source_gap substitute.
    expected=origin.source
    for link in links:
        if link.reset:return fallback('RESET_CURRENT_SR')
        if not link.unedited:return fallback('EDITED_SNAPSHOT_CURRENT_SR')
        if (link.session,link.version,link.epoch)!=key:return fallback('IDENTITY_MISMATCH_CURRENT_SR')
        if link.link_from!=expected or link.source!=expected+1:return fallback('GAP_OR_ORDER_CURRENT_SR')
        if link.color.shape!=origin.color.shape or link.depth.shape!=origin.depth.shape or link.mv.shape!=origin.mv.shape:return fallback('DOMAIN_MISMATCH_CURRENT_SR')
        expected=link.source
    if expected!=current.source or len(links)!=age:return fallback('MISSING_OR_INCOMPLETE_CHAIN_CURRENT_SR')
    last=links[-1] if links else origin
    if last.link_from!=current.link_from or last.jitter_delta!=current.jitter_delta:return fallback('CURRENT_LINK_METADATA_MISMATCH_CURRENT_SR')
    if any(a.dtype!=b.dtype or a.shape!=b.shape or a.tobytes()!=b.tobytes() for a,b in [(last.color,current.color),(last.depth,current.depth),(last.mv,current.mv)]):return fallback('CURRENT_SNAPSHOT_MISMATCH_CURRENT_SR')
    # The result belongs to origin. Never capture against arrival-time currentSR/current-depth.
    history,_=sky.capture(origin.color,arrival.edited,origin.depth,origin.source)
    history.session=origin.session;history.version=origin.version;record['capture_calls']=1
    for link in links:
        history,_=sky.reproject(history,link.color,link.depth,link.mv,link.source,link.jitter_delta)
        record['reproject_calls']+=1
    output,_=sky.apply(current.color,history.edit,history.guide,history.levels,history.level_guide)
    record['apply_calls']=1;record['accepted']=True;record['reason']='COMPLETE_ORIGIN_CATCHUP_CURRENT_BASE';record['final_source']=history.source;record['final_origin']=history.origin
    return Outcome(output,history,record['reason'],record)
