# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline expression transcription of Stable Mode10 plus exact6bfc Context12/AntiPop, not NR or another cache algorithm."""
from pathlib import Path
import sys,numpy as np
sys.path.insert(0,'/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-highfps-worktree/poc/game_nr/sky-cache-cpu')
import sky_cache_reference as sky
LUMA=sky.LUMA

def context(raw):  # exact upstream Context12: average PrintOf(rgb,WP1).x over4x4
 h,w=raw.shape[:2];dst=np.zeros(((h+3)//4,(w+3)//4,4));log=sky.log_luma(raw)
 for y in range(dst.shape[0]):
  for x in range(dst.shape[1]):dst[y,x,0]=log[y*4:min(y*4+4,h),x*4:min(x*4+4,w)].mean()
 return dst

def context_at(tex,uv):  # exact ContextAt/ContextPrevAt: four half-context-texel-offset bilinear taps
 h,w=tex.shape[:2];t=.5/np.array([w,h]);return sum(sky.bilinear(tex,uv+t*[x,y])[0] for x,y in [(-1,-1),(1,-1),(-1,1),(1,1)])*.25

def tent(edit,uv):
 h,w=edit.shape[:2];step=12./np.array([w,h]);return sum(sky.bilinear(edit,uv+step*[x,y])[:3]*((2 if x==0 else 1)*(2 if y==0 else 1)/16.) for y in [-1,0,1] for x in [-1,0,1])

def temporal(raw,edit,previous,guide,previous_guide,depth,mv,whole=True,prior_valid=True,low_temporal=.95,anti_step=0.,ctx=None,prev_ctx=None,jitter=(0.,0.)):
 # Fine temporal0/crossfade0; unchanged original constants/masks. Alpha remains raw.
 if not whole:return raw.copy(),np.zeros_like(edit)
 h,w=raw.shape[:2];shown=edit.copy();out=raw.copy()
 for y in range(h):
  for x in range(w):
   uv=np.array([x+.5,y+.5])/[w,h];e=edit[y,x,:3];out_edit=e.copy();local=edit[max(0,y-1):min(h,y+2),max(0,x-1):min(w,x+2),:3]  # mu unused at fineTemporal0
   low_now=tent(edit,uv) if low_temporal>0 or anti_step>0 else np.zeros(3)
   q=uv+sky.motion_uv(depth,mv,uv,jitter)
   if prior_valid and np.all(q>=0) and np.all(q<=1):
    pos=q*[w,h]-.5;i=np.floor(pos).astype(int);f=pos-i;hist=np.zeros(3);wsum=0.
    for yy in range(2):
     for xx in range(2):
      ix,iy=np.clip(i[0]+xx,0,w-1),np.clip(i[1]+yy,0,h-1);rel=abs(previous_guide[iy,ix,0]-guide[y,x,0])/max(min(previous_guide[iy,ix,0],guide[y,x,0]),1e-7);wt=(f[0] if xx else 1-f[0])*(f[1] if yy else 1-f[1])*np.clip((.2-rel)/.1,0,1);hist+=previous[iy,ix,:3]*wt;wsum+=wt
    if wsum>.25:
     hist/=wsum;valid=float(np.clip(2*wsum-1,0,1));low_hist=tent(previous,q);step=float((low_hist-low_now)@LUMA)/.35;motion_px=np.linalg.norm((q-uv)*[w,h])/6.;low_w=low_temporal*valid*np.exp(-step*step)*np.exp(-motion_px*motion_px);low_out=(1-low_w)*low_now+low_w*low_hist
     if anti_step>0:
      still=float(np.clip(1-abs(context_at(ctx,uv)-context_at(prev_ctx,q))/.15,0,1))*valid;dl=float((low_out-low_hist)@LUMA);low_out+=(np.clip(dl,-anti_step,anti_step)-dl)*still
     out_edit=low_out+(e-low_now) # original fine temporal0, so detail remains whole
   shown[y,x,:3]=out_edit;shown[y,x,3]=1.
   if np.any(out_edit!=0):out[y,x,:3]=np.maximum((np.maximum(raw[y,x,:3],0)+1/512)*np.exp2(out_edit)-1/512,0)
 return out,shown
