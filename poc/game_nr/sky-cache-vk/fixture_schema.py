#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib,json
from pathlib import Path
SCHEMA='sky-cache-owned-vk-v1'
def unique(pairs):
 d={}
 for k,v in pairs:
  if k in d:raise ValueError('duplicate key '+k)
  d[k]=v
 return d

def validate(c,assets=True):
 def exact(o,keys):
  if type(o)!=dict or set(o)!=set(keys):raise ValueError('exact object keys')
 def integer(x,lo,hi):
  if type(x)!=int or not lo<=x<=hi:raise ValueError('strict integer range')
 exact(c,['schema','case_id','phase_status','color_extent','guide_extent','max_links','max_age','budgets_ms','frames'])
 if c['schema']!=SCHEMA or c['phase_status'] not in ['KNOWN_SYNTHETIC_ALIGNED','UNVERIFIED_REAL_GUIDE_DOMAIN']:raise ValueError('schema/phase')
 if not isinstance(c['case_id'],str) or not c['case_id'] or len(c['case_id'])>63:raise ValueError('case ID')
 for key in ['color_extent','guide_extent']:
  if type(c[key])!=list or len(c[key])!=2:raise ValueError('extent')
  integer(c[key][0],1,1920);integer(c[key][1],1,1080)
 if c['phase_status']=='KNOWN_SYNTHETIC_ALIGNED' and c['guide_extent']!=c['color_extent']:raise ValueError('known aligned profile requires aligned extents')
 if type(c['max_links'])!=int or c['max_links']!=4 or type(c['max_age'])!=int or c['max_age']!=4:raise ValueError('bounded history')
 if c['budgets_ms']!={'cold':120000,'frame':30000,'fence':10000,'wall':300000}:raise ValueError('budgets')
 if type(c['frames'])!=list or not 1<=len(c['frames'])<=5:raise ValueError('anchor + at most4 links')
 for n,f in enumerate(c['frames']):
  exact(f,['source','link_from','origin','history_version','session','reset','refresh','jitter_delta_uv','assets','expected'])
  for k in ['source','link_from','origin','history_version']:integer(f[k],0,2**53-1)
  if type(f['reset'])!=bool or type(f['refresh'])!=bool or (n==0 and not f['refresh']):raise ValueError('reset/refresh')
  if f['refresh'] and f['origin']!=f['source']:raise ValueError('refresh requires SAME_ORIGIN anchor; late-origin capture unsupported')
  if type(f['session'])!=str or not f['session'] or len(f['session'])>63:raise ValueError('session')
  j=f['jitter_delta_uv']
  import math
  if type(j)!=list or len(j)!=2 or any(type(x) not in [float,int] or not math.isfinite(x) or abs(x)>1 for x in j):raise ValueError('jitter')
  exact(f['assets'],['source','edited','depth','mv'])
  for role,a in f['assets'].items():
   exact(a,['path','windows_path','sha256','bytes','extent','format','channels'])
   dims=c['color_extent'] if role in ['source','edited'] else c['guide_extent'];ch=1 if role=='depth' else 2 if role=='mv' else 4;fmt=100 if ch==1 else 103 if ch==2 else 109
   if a['extent']!=dims or type(a['channels'])!=int or a['channels']!=ch or type(a['format'])!=int or a['format']!=fmt or type(a['bytes'])!=int or a['bytes']!=dims[0]*dims[1]*ch*4:raise ValueError('per-role extent/format/byte size')
   p=Path(a['path'])
   if not p.is_absolute() or a['windows_path']!='Z:'+str(p).replace('/','\\'):raise ValueError('explicit path')
   if not isinstance(a['sha256'],str) or len(a['sha256'])!=64 or any(x not in '0123456789abcdef' for x in a['sha256']):raise ValueError('SHA')
   if assets:
    b=p.read_bytes()
    if len(b)!=a['bytes'] or hashlib.sha256(b).hexdigest()!=a['sha256']:raise ValueError('actual asset SHA/bytes')
  # Expected CPU oracle is only applicable to known synthetic geometry. Null is explicit for real guides.
  if c['phase_status']=='UNVERIFIED_REAL_GUIDE_DOMAIN':
   if f['expected'] is not None:raise ValueError('real phase cannot inherit synthetic oracle')
  else:
   exact(f['expected'],['path','windows_path','sha256','bytes','extent','format','channels'])
   a=f['expected'];p=Path(a['path'])
   if a['extent']!=c['color_extent'] or a['format']!=109 or a['channels']!=4 or a['bytes']!=c['color_extent'][0]*c['color_extent'][1]*16:raise ValueError('oracle extent/format/byte size')
   if not p.is_absolute() or a['windows_path']!='Z:'+str(p).replace('/','\\') or type(a['sha256'])!=str or len(a['sha256'])!=64:raise ValueError('oracle explicit path/SHA')
   if assets:
    b=p.read_bytes()
    if len(b)!=a['bytes'] or hashlib.sha256(b).hexdigest()!=a['sha256']:raise ValueError('oracle actual SHA/bytes')
 return c
if __name__=='__main__':
 import sys
 c=json.loads(Path(sys.argv[1]).read_text(),object_pairs_hook=unique);validate(c);print('PASS_CPU_SCHEMA_ASSETS_NO_GPU')
