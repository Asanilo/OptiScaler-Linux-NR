#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import json,copy
from late_cases import validate,snapshot,load
import sky_cache_reference as sky
from late_arrival import Arrival,catch_up

def run(bundle,proofs):
 b=json.loads(bundle.read_text());validate(b);rows=[]
 def check(n,v):
  if not bool(v):raise AssertionError(n)
  rows.append({'check':n,'status':'PASS'})
 for j,p in zip(b['jobs'],proofs):
  check(j['id']+'_CPU52_predicate_expected',p['accepted']==j['expected_accepted'] and (p['accepted'] or p['reason']==j['expected_reason']))
  if not p['accepted']:check(j['id']+'_zero_partial_math',p['capture_calls']==p['reproject_calls']==p['apply_calls']==0)
  else:
   o=snapshot(j['origin']);cur=snapshot(j['current']);h,_=sky.capture(o.color,load(j['edited']),o.depth,o.source)
   for l in j['links']:
    s=snapshot(l);h,_=sky.reproject(h,s.color,s.depth,s.mv,s.source,s.jitter_delta)
    immediate,_=sky.apply(s.color,h.edit,h.guide,h.levels,h.level_guide)
   arr=Arrival.own(load(j['edited']),origin=j['arrival_origin'],session=j['arrival_session'],version=j['arrival_version'],epoch=j['arrival_epoch']);late=catch_up(o,arr,[snapshot(x) for x in j['links']],cur)
   check(j['id']+'_same_current_immediate_vs_catchup_bits',immediate.tobytes()==late.output.tobytes()==load(j['expected']).tobytes())
 mutations=[('extra_key',lambda c:c.update(unknown=True)),('too_many_jobs',lambda c:c.update(jobs=c['jobs']*2)),('fifth_link',lambda c:c['jobs'][0].update(links=c['jobs'][0]['links']*5)),('bool_source',lambda c:c['jobs'][0]['origin'].update(source=True)),('wide_unproven_domain',lambda c:c.update(color_extent=[1920,1080])),('invalid_boolean_ready',lambda c:c['jobs'][0].update(ready=1)),('wrong_CI',lambda c:c['jobs'][0]['edited'].update(format=97)),('wrong_CPU_bytes',lambda c:c['jobs'][0]['edited'].update(bytes=8))]
 for name,mut in mutations:
  bad=copy.deepcopy(b);mut(bad)
  try:validate(bad)
  except (ValueError,AssertionError):pass
  else:raise AssertionError('negative accepted '+name)
  rows.append({'check':'schema_reject_'+name,'status':'PASS'})
 return {'status':'PASS_CPU_ONLY','checks_count':len(rows),'checks':rows,'jobs':len(b['jobs']),'accepted':sum(p['accepted'] for p in proofs),'negative':sum(not p['accepted'] for p in proofs),'GPU':'NOT_RUN','Wine':'NOT_RUN','phase':'KNOWN_SYNTHETIC_ONLY_REAL_720_1080_UNVERIFIED'}
