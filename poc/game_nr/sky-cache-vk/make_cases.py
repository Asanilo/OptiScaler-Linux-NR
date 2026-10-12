#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Independent immutable assets: known geometry + saved real NR720/currentSR. Host CPU only."""
import hashlib,json,sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'sky-cache-cpu'))
import sky_cache_reference as sky
from fixture_schema import validate
ROOT=Path(__file__).resolve().parent
RESULT=Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/runs/highfps-lowres-lowres-current-runtime-a-20261012T183618.266410Z/output/run-result.json')
def asset(p,a,ch):
 p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(a.astype('<f4').tobytes());return existing(p,list(a.shape[:2][::-1]),ch)
def existing(p,dims,ch):
 p=p.resolve();b=p.read_bytes();return {'path':str(p),'windows_path':'Z:'+str(p).replace('/','\\'),'sha256':hashlib.sha256(b).hexdigest(),'bytes':len(b),'extent':dims,'format':100 if ch==1 else 103 if ch==2 else 109,'channels':ch}
def generate(out):
 out=Path(out);cases=[]
 for kind in ['translation','zero_edit','whole_invalid','low_confidence_depth_occlusion']:
  w,h=16,8;case={'schema':'sky-cache-owned-vk-v1','case_id':kind,'phase_status':'KNOWN_SYNTHETIC_ALIGNED','color_extent':[w,h],'guide_extent':[w,h],'max_links':4,'max_age':4,'budgets_ms':{'cold':120000,'frame':30000,'fence':10000,'wall':300000},'frames':[]};history=None
  for f in range(5):
   color=np.full((h,w,4),.25,dtype='<f4');color[...,:3]+=np.arange(w)[None,:,None]/256+f/128;color[...,3]=np.arange(h*w).reshape(h,w)/256
   if kind=='zero_edit':color[0,0]=[-.25,-0.,0.,-0.]
   depth=np.full((h,w),.5,dtype='<f4');mv=np.zeros((h,w,2),dtype='<f4')
   if f and kind!='zero_edit':mv[...,0]=-1
   if f and kind=='low_confidence_depth_occlusion':depth[:,6:10]=.95
   edited=color.copy()
   if kind!='zero_edit':edited[...,:3]=(color[...,:3]+1/512)*np.exp2(np.arange(w)[None,:,None]/32+.25)-1/512
   reset=kind=='whole_invalid' and f>=2;src=100+f
   frame={'source':src,'link_from':src-1,'origin':100,'history_version':1,'session':kind,'reset':reset,'refresh':f==0,'jitter_delta_uv':[0.,0.],'assets':{},'expected':None}
   for role,a,ch in [('source',color,4),('edited',edited,4),('depth',depth,1),('mv',mv,2)]:frame['assets'][role]=asset(out/'assets'/kind/f'f{f}-{role}.f32',a,ch)
   if f==0:
    history,_=sky.capture(color,edited,depth,src);history.session=kind;expected,_=sky.apply(color,history.edit,history.guide,history.levels,history.level_guide)
   elif reset:expected=color.copy()
   else:
    history,_=sky.reproject(history,color,depth,mv,src);expected,_=sky.apply(color,history.edit,history.guide,history.levels,history.level_guide)
   frame['expected']=asset(out/'oracle'/kind/f'f{f}-expected.rgba32f',expected,4);case['frames'].append(frame)
  validate(case);p=out/'cases'/f'{kind}.json';p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(case,indent=2)+'\n');cases.append(p)
 # Same saved successful720 output is an anchor EDIT; subsequent actualSR always comes from raw saved source.
 r=json.loads(RESULT.read_text());stage=next(s for s in r['stages'] if s['feature_generation']==2)
 c={'schema':'sky-cache-owned-vk-v1','case_id':'saved-real-nr720-fourframes-candidate','phase_status':'UNVERIFIED_REAL_GUIDE_DOMAIN','color_extent':[1920,1080],'guide_extent':[1280,720],'max_links':4,'max_age':4,'budgets_ms':{'cold':120000,'frame':30000,'fence':10000,'wall':300000},'frames':[]}
 for f in stage['frames']:
  n=f['frame'];prefix=RESULT.parent;roles={}
  for role,actual,ch,dims in [('source','source',4,[1920,1080]),('edited','output',4,[1920,1080]),('depth','depth',1,[1280,720]),('mv','mv',2,[1280,720])]:roles[role]=existing(prefix/f['dumps'][actual],dims,ch)
  assert roles['source']['sha256']==f['source_sha256'] and roles['edited']['sha256']==f['output_sha256']
  c['frames'].append({'source':f['source_game_frame'],'link_from':f['source_game_frame']-1,'origin':stage['frames'][0]['source_game_frame'],'history_version':1,'session':c['case_id'],'reset':False,'refresh':n==0,'jitter_delta_uv':[0.,0.],'assets':roles,'expected':None})
 validate(c);p=out/'cases'/'saved-real.json';p.write_text(json.dumps(c,indent=2)+'\n');cases.append(p)
 return cases
if __name__=='__main__':print('\n'.join(str(p) for p in generate(ROOT/'out')))
