#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Small standalone bundle generator/schema/CPU oracle, not Native/Helper transport wire."""
from pathlib import Path
import hashlib,json,copy,sys
import numpy as np
ROOT=Path(__file__).resolve().parent;OLD=ROOT.parent/'sky-cache-vk'
sys.path.insert(0,str(ROOT.parent/'sky-cache-cpu'))
from late_arrival import Snapshot,Arrival,catch_up
SCHEMA='sky-cache-late-owned-v1'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(a):
 p=Path(a['path']);assert sha(p)==a['sha256'];w,h=a['extent'];return np.fromfile(p,dtype='<f4').reshape((h,w,a['channels']) if a['channels']>1 else (h,w))
def snapshot(f):
 a=f['assets'];return Snapshot.own(load(a['source']),load(a['depth']),load(a['mv']),source=f['source'],link_from=f['link_from'],session=f['session'],version=f['version'],epoch=f['epoch'],reset=f['reset'],jitter_delta=f['jitter_delta'])
def node(f):
 return {'source':f['source'],'link_from':f['link_from'],'session':'known-synthetic','version':3,'epoch':7,'reset':False,'jitter_delta':f['jitter_delta_uv'],'assets':{k:copy.deepcopy(f['assets'][k]) for k in ['source','depth','mv']}}
def descriptor(p,a):
 p=Path(p).resolve();p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(a.astype('<f4').tobytes());h,w=a.shape[:2];ch=a.shape[2] if a.ndim==3 else 1
 return {'path':str(p),'windows_path':'Z:'+str(p).replace('/','\\'),'sha256':sha(p),'bytes':p.stat().st_size,'extent':[w,h],'format':109 if ch==4 else 103 if ch==2 else 100,'channels':ch}
def oracle(job,out):
 origin=snapshot(job['origin']);current=snapshot(job['current']);arrival=Arrival.own(load(job['edited']),origin=job['arrival_origin'],session=job['arrival_session'],version=job['arrival_version'],epoch=job['arrival_epoch'],ready=job['ready'])
 r=catch_up(origin,arrival,[snapshot(x) for x in job['links']],current)
 job['expected']=descriptor(out/'oracle'/(job['id']+'.rgba32f'),r.output);job['expected_accepted']=r.record['accepted'];job['expected_reason']=r.reason
 return r.record

def generate(out):
 out=Path(out);out.mkdir(parents=True,exist_ok=True);base=json.loads((OLD/'out/cases/translation.json').read_text());jobs=[]
 def primary(c,delay,name):
  frames=c['frames'];o=node(frames[0]);return {'id':name,'origin':o,'edited':copy.deepcopy(frames[0]['assets']['edited']),'arrival_origin':o['source'],'arrival_session':o['session'],'arrival_version':o['version'],'arrival_epoch':o['epoch'],'ready':True,'links':[node(f) for f in frames[1:delay+1]],'current':node(frames[delay]),'expected':None,'expected_accepted':False,'expected_reason':''}
 for n in range(1,5):jobs.append(primary(base,n,'delay'+str(n)))
 for fname,name in [('zero_edit','zero_edit_delay4'),('low_confidence_depth_occlusion','occlusion_delay4')]:jobs.append(primary(json.loads((OLD/'out/cases'/f'{fname}.json').read_text()),4,name))
 for name in ['pending','missing','gap','reset','epoch','overage','last_phase_mismatch']:
  j=primary(base,4,name)
  if name=='pending':j['ready']=False
  elif name=='missing':j['links']=j['links'][:2]
  elif name=='gap':j['links'][1]['link_from']-=1
  elif name=='reset':j['links'][1]['reset']=True
  elif name=='epoch':j['arrival_epoch']+=1
  elif name=='overage':
   j['current']['source']=105;j['current']['link_from']=104
   a=load(j['current']['assets']['source']).copy();a[...,:3]+=np.float32(1/128)
   j['current']['assets']['source']=descriptor(out/'synthetic'/'source105.rgba32f',a)
  elif name=='last_phase_mismatch':j['current']['jitter_delta']=[.125,0.]
  jobs.append(j)
 proofs=[]
 for j in jobs:proofs.append(oracle(j,out))
 b={'schema':SCHEMA,'profile':'KNOWN_ALIGNED_SYNTHETIC_ONLY','color_extent':[16,8],'guide_extent':[16,8],'max_links':4,'max_age':4,'budgets_ms':{'cold':120000,'job':30000,'fence':10000,'wall':300000},'jobs':jobs}
 validate(b);p=out/'case-bundle.json';p.write_text(json.dumps(b,indent=2)+'\n');return p,proofs

def validate(b):
 def keys(o,names):
  if type(o)!=dict or set(o)!=set(names):raise ValueError('exact fixture keys')
 def integer(x):
  if type(x)!=int or not 0<=x<=2**53-1:raise ValueError('bounded fixture integer')
 def asset(a,ch,dims):
  keys(a,['path','windows_path','sha256','bytes','extent','format','channels'])
  if any(type(a[k])!=int for k in ['channels','format','bytes']) or a['extent']!=dims or a['channels']!=ch or a['format']!=(109 if ch==4 else 103 if ch==2 else 100) or a['bytes']!=dims[0]*dims[1]*ch*4:raise ValueError('role bytes/CI')
  p=Path(a['path'])
  if not p.is_absolute() or a['windows_path']!='Z:'+str(p).replace('/','\\') or sha(p)!=a['sha256'] or p.stat().st_size!=a['bytes']:raise ValueError('asset path/hash/size')
 def sn(f):
  keys(f,['source','link_from','session','version','epoch','reset','jitter_delta','assets'])
  for k in ['source','link_from','version','epoch']:integer(f[k])
  if type(f['session'])!=str or not f['session'] or type(f['reset'])!=bool:raise ValueError('identity/reset')
  if type(f['jitter_delta'])!=list or len(f['jitter_delta'])!=2 or any(type(x) not in [int,float] or not np.isfinite(x) or abs(x)>1 for x in f['jitter_delta']):raise ValueError('jitter')
  keys(f['assets'],['source','depth','mv'])
  for k,ch in [('source',4),('depth',1),('mv',2)]:asset(f['assets'][k],ch,b['color_extent'] if ch==4 else b['guide_extent'])
 keys(b,['schema','profile','color_extent','guide_extent','max_links','max_age','budgets_ms','jobs'])
 if b['schema']!=SCHEMA or b['profile']!='KNOWN_ALIGNED_SYNTHETIC_ONLY' or b['color_extent']!=[16,8] or b['guide_extent']!=[16,8] or b['max_links']!=4 or b['max_age']!=4:raise ValueError('exact small known fixture')
 if b['budgets_ms']!={'cold':120000,'job':30000,'fence':10000,'wall':300000} or type(b['jobs'])!=list or not 1<=len(b['jobs'])<=16:raise ValueError('bounded batch')
 ids=set()
 for j in b['jobs']:
  keys(j,['id','origin','edited','arrival_origin','arrival_session','arrival_version','arrival_epoch','ready','links','current','expected','expected_accepted','expected_reason'])
  if type(j['id'])!=str or not j['id'] or j['id'] in ids:raise ValueError('unique job');
  ids.add(j['id']);sn(j['origin']);sn(j['current'])
  for k in ['arrival_origin','arrival_version','arrival_epoch']:integer(j[k])
  if type(j['ready'])!=bool or type(j['expected_accepted'])!=bool or type(j['arrival_session'])!=str or type(j['expected_reason'])!=str or type(j['links'])!=list or len(j['links'])>4:raise ValueError('job types/link bound')
  for l in j['links']:sn(l)
  asset(j['edited'],4,b['color_extent']);asset(j['expected'],4,b['color_extent'])
 return b
if __name__=='__main__':p,proofs=generate(ROOT/'out');print(json.dumps({'case':str(p),'CPU_proofs':proofs},indent=2))
