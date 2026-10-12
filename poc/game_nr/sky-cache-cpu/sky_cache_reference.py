#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""CPU transcription of selected frozen Sky HLSL expressions (not another NR algorithm).
Source: dlssnr_cache.hlsl Capture2/Reproject1/Pyramid3/Apply4, HDR/WP1.
Optional crossfade/temporal/despeckle/stabilizer off, refreshBlend1/lowGain1/highGain1.
Own whole-lineage/zero-edit wrapper preserves current SR bytes; low confidence is NOT invalidity.
No native wire, GPU, model, production IPC or quality/phase claim.
"""
from dataclasses import dataclass
import numpy as np
SAT=lambda x:np.clip(x,0.0,1.0)
LUMA=np.array([.2126,.7152,.0722],dtype=np.float64)
SOURCE='/home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/OptiScaler/shaders/dlssnr/precompile/dlssnr_cache.hlsl'
SOURCE_SHA='0efdd31f1976435b0b956b94814c96a70e68310b838f916aecc289375af02b4f'

def linear_depth(z,inverted=False):  # HLSL LinDepth85
    return 1.0/np.maximum(z if inverted else 1.0-z,1e-7)
def log_luma(c,epsilon=1/512):return np.log2(np.maximum(c[...,:3],0)@LUMA+epsilon)
def clamp_edit(e,scale=1,max_luma_edit=1.5):  # HLSL ClampEdit121
    limit=max_luma_edit*max(scale,1);l=e@LUMA;lc=np.clip(l,-limit,limit);e=e+(lc-l)[...,None]
    return np.clip(e,lc[...,None]-1,lc[...,None]+1)
def fresh_edit(original,edited,epsilon=1/512):  # HLSL FreshEdit145
    a=np.maximum(original[...,:3],0);b=np.maximum(edited[...,:3],0)
    okay=~((a@LUMA>8*epsilon)&(b@LUMA<.05*(a@LUMA)))
    return clamp_edit(np.log2((b+epsilon)/(a+epsilon))),okay

def bilinear(tex,uv):
    h,w=tex.shape[:2];pos=np.asarray(uv)*[w,h]-.5;i=np.floor(pos).astype(int);f=pos-i;value=np.zeros(tex.shape[2:],dtype=np.float64)
    for y in range(2):
        for x in range(2):value+=tex[np.clip(i[1]+y,0,h-1),np.clip(i[0]+x,0,w-1)]*((f[0] if x else 1-f[0])*(f[1] if y else 1-f[1]))
    return value

def catmull_rom(tex,uv):  # exact five-tap HLSL CatmullRom269, clamped by ReadHistory
    h,w=tex.shape[:2];size=np.array([w,h]);pos=np.asarray(uv)*size;t=np.floor(pos-.5)+.5;f=pos-t
    w0=f*(-.5+f*(1-.5*f));w1=1+f*f*(-2.5+1.5*f);w2=f*(.5+f*(2-1.5*f));w3=f*f*(-.5+.5*f);w12=w1+w2
    tc0=(t-1)/size;tc3=(t+2)/size;tc12=(t+w2/np.maximum(w12,1e-6))/size
    terms=[(np.array([tc12[0],tc0[1]]),w12[0]*w0[1]),(np.array([tc0[0],tc12[1]]),w0[0]*w12[1]),(tc12,w12[0]*w12[1]),(np.array([tc3[0],tc12[1]]),w3[0]*w12[1]),(np.array([tc12[0],tc3[1]]),w12[0]*w3[1])]
    return sum(bilinear(tex,q)[...,:3]*weight for q,weight in terms)/max(sum(weight for _,weight in terms),1e-6)

def read_history(edit,guide,q,depth,tolerance=.1):  # HLSL ReadHistory296
    h,w=edit.shape[:2];pos=np.asarray(q)*[w,h]-.5;i=np.floor(pos).astype(int);f=pos-i
    e=np.zeros(3);conf=0.;luma=0.;wsum=0.;all_valid=1.;lo=np.full(3,1e9);hi=-lo
    for y in range(2):
        for x in range(2):
            ix,iy=np.clip(i[0]+x,0,w-1),np.clip(i[1]+y,0,h-1);g=guide[iy,ix];v=edit[iy,ix]
            rel=abs(g[0]-depth)/max(min(g[0],depth),1e-7);wd=float(SAT((2*tolerance-rel)/max(tolerance,1e-6)));wb=(f[0] if x else 1-f[0])*(f[1] if y else 1-f[1]);weight=wb*wd
            all_valid=min(all_valid,wd);e+=v[:3]*weight;conf+=v[3]*weight;luma+=g[1]*weight;wsum+=weight;lo=np.minimum(lo,v[:3]);hi=np.maximum(hi,v[:3])
    if wsum>1e-4:e/=wsum;conf/=wsum;luma/=wsum
    if all_valid>0:e=(1-all_valid)*e+all_valid*np.clip(catmull_rom(edit,q),lo,hi)
    return e,conf,luma,float(SAT(wsum))

def motion_uv(depth,mv,uv,jitter_delta=(0.,0.)):  # HLSL MotionUvOffset222 (caller explicitly selects domain)
    dh,dw=depth.shape;mh,mw=mv.shape[:2];c=np.clip((np.asarray(uv)*[dw,dh]).astype(int),[0,0],[dw-1,dh-1]);best=c.copy();nearest=float('inf')
    for y in range(-1,2):
        for x in range(-1,2):
            p=np.clip(c+[x,y],[0,0],[dw-1,dh-1]);z=linear_depth(depth[p[1],p[0]])
            if z<nearest:nearest=z;best=p
    best_uv=(best+.5)/[dw,dh];p=np.clip((best_uv*[mw,mh]).astype(int),[0,0],[mw-1,mh-1]);return mv[p[1],p[0]]/[mw,mh]+np.asarray(jitter_delta)

def reduce_first(edit,guide,weights):  # HLSL ReduceToFirstLevel452, exact 4x4 weighted reduction
    h,w=weights.shape;level=np.zeros(((h+3)//4,(w+3)//4,4));g=np.zeros((*level.shape[:2],2))
    for y in range(level.shape[0]):
        for x in range(level.shape[1]):
            sl=np.s_[y*4:min(y*4+4,h),x*4:min(x*4+4,w)];weights4=weights[sl];total=float(weights4.sum());level[y,x,3]=total/16
            if total>1e-6:level[y,x,:3]=(edit[sl]*weights4[...,None]).sum(axis=(0,1))/total;g[y,x]=(guide[sl]*weights4[...,None]).sum(axis=(0,1))/total
    return level,g

def pyramid(first):  # HLSL mode3, each next texel coverage-weighted4x4, including partial edges/16
    levels=[first]
    for _ in range(2):
        src=levels[-1];h,w=src.shape[:2];target=np.zeros(((h+3)//4,(w+3)//4,4))
        for y in range(target.shape[0]):
            for x in range(target.shape[1]):
                block=src[y*4:min(y*4+4,h),x*4:min(x*4+4,w)];weight=block[...,3];total=float(weight.sum());target[y,x,3]=total/16
                if total>1e-6:target[y,x,:3]=(block[...,:3]*weight[...,None]).sum(axis=(0,1))/total
        levels.append(target)
    return levels

def fetch_level(tex,q):  # HLSL FetchLevel413: confidence-weighted bilinear normalized edit
    h,w=tex.shape[:2];pos=np.asarray(q)*[w,h]-.5;i=np.floor(pos).astype(int);f=pos-i;acc=np.zeros(3);weight=0.
    for y in range(2):
        for x in range(2):
            v=tex[np.clip(i[1]+y,0,h-1),np.clip(i[0]+x,0,w-1)];wt=(f[0] if x else 1-f[0])*(f[1] if y else 1-f[1])*v[3];acc+=v[:3]*wt;weight+=wt
    return (acc/weight if weight>1e-6 else np.zeros(3)),weight

def apply(original,history,guide,levels,level_guide,whole_valid=True,epsilon=1/512):  # HLSL mode4/own validity wrapper
    if not whole_valid:return original.copy(),np.zeros(original.shape[:2],bool) # real SR bits: negatives/zero sign/alpha too
    h,w=original.shape[:2];result=original.copy();applied=np.zeros((h,w),bool);l1=levels[0];lh,lw=l1.shape[:2]
    for y in range(h):
        for x in range(w):
            uv=np.array([x+.5,y+.5])/[w,h];pos=uv*[lw,lh]-.5;i=np.floor(pos).astype(int);f=pos-i;g=guide[y,x];logd=np.log2(max(g[0],1e-7));acc=np.zeros(3);weight=0.
            for yy in range(2):
                for xx in range(2):
                    ix,iy=np.clip(i[0]+xx,0,lw-1),np.clip(i[1]+yy,0,lh-1);s=l1[iy,ix];lg=level_guide[iy,ix];wb=(f[0] if xx else 1-f[0])*(f[1] if yy else 1-f[1]);wt=wb*s[3]*np.exp(-(((lg[0]-logd)/.15)**2+((lg[1]-g[1])/1.)**2));acc+=s[:3]*wt;weight+=wt
            l2,a2=fetch_level(levels[1],uv);l3,a3=fetch_level(levels[2],uv);low=l3 if a3>1e-6 else np.zeros(3);low=low*(1-SAT(a2*4))+l2*SAT(a2*4);low=low*(1-SAT(weight*4))+(acc/weight if weight>1e-6 else low)*SAT(weight*4)
            hist=history[y,x];high=hist[:3]-low;e=clamp_edit(low+hist[3]*high)
            # Explicit zero-edit identity wrapper; do not inherit HLSL max(original,0) on identity.
            if np.all(e==0):continue
            result[y,x,:3]=np.maximum((np.maximum(original[y,x,:3],0)+epsilon)*np.exp2(e)-epsilon,0);applied[y,x]=True
    return result,applied # alpha never touched, low hist.a still legitimately gets low-band

@dataclass
class History:
    edit:np.ndarray
    guide:np.ndarray
    levels:list
    level_guide:np.ndarray
    source:int
    origin:int
    session:str='cpu-known-domain'
    version:int=1

def capture(original,edited,depth,source,previous=None,mv=None):  # Mode2 refreshBlend1, optional modifiers disabled
    e,ok=fresh_edit(original,edited)
    # Mode2 black-answer fallback only where the carried surface is actually valid.
    if previous is not None:
        if mv is None:raise ValueError('capture with previous history requires real per-frame MV')
        h,w=depth.shape
        for y in range(h):
            for x in range(w):
                if ok[y,x]:continue
                uv=np.array([x+.5,y+.5])/[w,h];q=uv+motion_uv(depth,mv,uv)
                if np.all(q>=0) and np.all(q<=1):
                    old,_,_,valid=read_history(previous.edit,previous.guide,q,linear_depth(depth[y,x]))
                    if valid>.5:e[y,x]=old
    hist=np.concatenate([e,np.ones((*e.shape[:2],1))],axis=-1)
    lin=linear_depth(depth);guide=np.stack([lin,log_luma(original)],axis=-1);first,g=reduce_first(e,np.stack([np.log2(lin),guide[...,1]],axis=-1),np.ones(depth.shape));return History(hist,guide,pyramid(first),g,source,source),ok

def reproject(history,current,depth,mv,source,jitter_delta=(0.,0.),decay=.97):  # Mode1, no automatic real-game phase assumptions
    h,w=current.shape[:2];out=np.zeros((h,w,4));guide=np.stack([linear_depth(depth),log_luma(current)],axis=-1);weights=np.zeros((h,w));rejected=0
    for y in range(h):
        for x in range(w):
            uv=np.array([x+.5,y+.5])/[w,h];q=uv+motion_uv(depth,mv,uv,jitter_delta);lin,luma=guide[y,x]
            if np.all(q>=0) and np.all(q<=1):e,conf,old_luma,valid=read_history(history.edit,history.guide,q,lin)
            else:e,conf,old_luma,valid=np.zeros(3),0.,0.,0.
            vcolour=float(SAT((2*.5-abs(luma-old_luma))/.5));doubt=(.6+.4*vcolour)*float(SAT(2*valid-1))*decay
            out[y,x,:3]=e;out[y,x,3]=SAT(conf*doubt);weights[y,x]=valid;rejected+=valid<.5
    first,g=reduce_first(out[...,:3],np.stack([np.log2(guide[...,0]),guide[...,1]],axis=-1),weights);return History(out,guide,pyramid(first),g,source,history.origin,history.session,history.version),rejected

def advance(history,current,links,target,session='cpu-known-domain',version=1,max_age=4,reset=False):
    # Entire session/lineage invalidity differs from per-pixel high-band confidence.
    if reset or history.session!=session or history.version!=version or target<=history.source or target-history.origin>max_age or not links or len(links)>4:return current.copy(),None,'WHOLE_INVALID_CURRENT_SR'
    expected=history.source;work=history
    for link in links:
        if link['from']!=expected or link['to']!=expected+1:return current.copy(),None,'MISSING_LINK_CURRENT_SR'
        work,_=reproject(work,link['color'],link['depth'],link['mv'],link['to'],link.get('jitter_delta',(0.,0.)));expected=link['to']
    if expected!=target:return current.copy(),None,'INCOMPLETE_CHAIN_CURRENT_SR'
    output,mask=apply(current,work.edit,work.guide,work.levels,work.level_guide)
    return output,work,'CURRENT_BASE_EDIT_APPLIED' if np.any(mask) else 'ZERO_EDIT_CURRENT_SR'


def owned_snapshot(color,depth,mv,source,already_edited=False):
    """Native-facing CPU policy demonstration only; borrowed real views must be copied on GPU."""
    if already_edited:raise ValueError('recursive edited input forbidden')
    if color.shape[:2]!=depth.shape or mv.shape!=(*depth.shape,2):
        raise ValueError('CPU known-domain test requires aligned resources; game720/1080 phase UNVERIFIED')
    if not all(np.isfinite(a).all() for a in (color,depth,mv)):raise ValueError('nonfinite snapshot')
    result={'source':source}
    for key,a in [('color',color),('depth',depth),('mv',mv)]:
        result[key]=a.copy();result[key].flags.writeable=False
    return result
